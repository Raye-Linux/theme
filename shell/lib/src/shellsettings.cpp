// SPDX-License-Identifier: GPL-3.0-or-later

#include "shellsettings.h"

#include <KConfigGroup>

#include <QRegularExpression>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace
{

enum class Kind { Choice, Bool, Int, DesktopIds, Tokens, Strings, Text, Time };

struct Key {
    const char *group;
    const char *key;
    Kind kind;
    // the default, written the way the file writes it
    const char *fallback;
    // Choice: the allowed values, then "alias=value" pairs after a "|"
    const char *values = nullptr;
};

// Every key of rayeshellrc. Keep it the same as shell/lib/rayeshellrc
// (the tests check it) and as shell.md 3.21.
constexpr Key Table[] = {
    {"Taskbar", "Alignment", Kind::Choice, "center", "center,left"},
    {"Taskbar", "Search", Kind::Choice, "box", "box,iconlabel,icon,hidden"},
    {"Taskbar", "TaskView", Kind::Bool, "true"},
    {"Taskbar", "Widgets", Kind::Bool, "false"},
    {"Taskbar", "AutoHide", Kind::Bool, "false"},
    {"Taskbar", "Badges", Kind::Bool, "true"},
    {"Taskbar", "Flashing", Kind::Bool, "true"},
    {"Taskbar", "AllDisplays", Kind::Bool, "false"},
    {"Taskbar", "MultiDisplayApps", Kind::Choice, "all", "all,main-and-open,open|mainandopen=main-and-open"},
    {"Taskbar", "ShareWindow", Kind::Bool, "false"},
    {"Taskbar", "ShowDesktopCorner", Kind::Bool, "true"},
    {"Taskbar", "Combine", Kind::Choice, "always", "always,full,never|whenfull=full"},
    {"Taskbar", "SmallButtons", Kind::Choice, "never", "never,always,full|whenfull=full"},
    {"Taskbar", "RecentSearches", Kind::Bool, "true"},
    {"Taskbar", "EndTask", Kind::Bool, "false"},
    {"Taskbar", "Pinned", Kind::DesktopIds,
     "org.kde.dolphin.desktop,firefox.desktop,org.raye.store.desktop,org.raye.settings.desktop"},
    {"Tray", "HiddenMenu", Kind::Bool, "true"},
    {"Tray", "Shown", Kind::Strings, ""},
    {"Tray", "Hidden", Kind::Strings, ""},
    {"Start", "Layout", Kind::Choice, "default", "default,pins,recommendations|morepins=pins,morerecommended=recommendations"},
    {"Start", "RecentApps", Kind::Bool, "true"},
    {"Start", "MostUsed", Kind::Bool, "false"},
    {"Start", "Recommended", Kind::Bool, "true"},
    {"Start", "Folders", Kind::Tokens, "settings,files",
     "settings,files,documents,downloads,music,pictures,videos,network,personal"},
    {"Start", "AllView", Kind::Choice, "category", "category,grid,list"},
    {"Start", "Pinned", Kind::DesktopIds,
     "org.kde.dolphin.desktop,firefox.desktop,org.raye.store.desktop,org.raye.settings.desktop,"
     "org.kde.konsole.desktop,org.kde.kate.desktop,org.kde.kcalc.desktop,org.kde.gwenview.desktop,"
     "org.kde.haruna.desktop,org.kde.elisa.desktop,"
     "org.raye.taskmanager.desktop"},
    {"Start", "PinnedFolders", Kind::Text, ""},
    {"Start", "RestartNeeded", Kind::Tokens, ""},
    {"Clock", "ShowSeconds", Kind::Bool, "false"},
    {"Clock", "ShowDate", Kind::Bool, "true"},
    {"Clock", "Use24h", Kind::Choice, "auto", "auto,true,false"},
    {"Focus", "Until", Kind::Time, ""},
    {"QuickSettings", "Tiles", Kind::Tokens, "wifi,bluetooth,airplane,energysaver,nightlight,accessibility"},
    {"Notifications", "ToastSeconds", Kind::Int, "5", "1,300"},
    {"Notifications", "LockBadges", Kind::Bool, "true"},
    {"Sound", "VolumeChime", Kind::Bool, "true"},
    {"Search", "Recent", Kind::Strings, ""},
};

const Key *find(const QString &group, const QString &key)
{
    for (const Key &k : Table) {
        if (group == QLatin1String(k.group) && key == QLatin1String(k.key))
            return &k;
    }
    return nullptr;
}

QStringList splitList(const QString &text)
{
    return text.split(u',', Qt::SkipEmptyParts);
}

bool toBool(const QString &text, bool *ok)
{
    const QString t = text.trimmed().toLower();
    *ok = true;
    if (t == u"true"_s || t == u"1"_s || t == u"yes"_s || t == u"on"_s)
        return true;
    if (t == u"false"_s || t == u"0"_s || t == u"no"_s || t == u"off"_s)
        return false;
    *ok = false;
    return false;
}

QStringList unique(const QStringList &in)
{
    QStringList out;
    for (const QString &s : in) {
        if (!out.contains(s))
            out << s;
    }
    return out;
}

// what a value of the file becomes: checked, or the default
QVariant check(const Key &k, const QString &raw, const QStringList &rawList)
{
    switch (k.kind) {
    case Kind::Choice: {
        const QStringList parts = QString::fromLatin1(k.values).split(u'|');
        const QStringList allowed = parts[0].split(u',');
        const QString v = raw.trimmed().toLower();
        if (allowed.contains(v))
            return v;
        if (parts.size() > 1) {
            for (const QString &alias : parts[1].split(u',')) {
                const auto pair = alias.split(u'=');
                if (pair.size() == 2 && pair[0] == v)
                    return pair[1];
            }
        }
        return QString::fromLatin1(k.fallback);
    }
    case Kind::Bool: {
        bool ok = false;
        const bool v = toBool(raw, &ok);
        if (ok)
            return v;
        return QString::fromLatin1(k.fallback) == u"true"_s;
    }
    case Kind::Int: {
        const QStringList range = QString::fromLatin1(k.values).split(u',');
        bool ok = false;
        const int v = raw.trimmed().toInt(&ok);
        if (ok && v >= range[0].toInt() && v <= range[1].toInt())
            return v;
        return QString::fromLatin1(k.fallback).toInt();
    }
    case Kind::DesktopIds: {
        QStringList out;
        for (const QString &s : rawList) {
            const QString id = s.trimmed();
            if (ShellSettings::isDesktopId(id))
                out << id;
        }
        return unique(out);
    }
    case Kind::Tokens: {
        const QStringList allowed = k.values ? QString::fromLatin1(k.values).split(u',') : QStringList();
        QStringList out;
        for (const QString &s : rawList) {
            const QString id = s.trimmed();
            if (ShellSettings::isToken(id) && (allowed.isEmpty() || allowed.contains(id)))
                out << id;
        }
        out = unique(out);
        // a single reason, not a list
        if (QLatin1String(k.key) == "RestartNeeded"_L1)
            return out.size() == 1 && rawList.size() == 1 ? out.first() : QString();
        return out;
    }
    case Kind::Strings: {
        QStringList out;
        for (const QString &s : rawList) {
            if (!s.trimmed().isEmpty())
                out << s;
        }
        return unique(out);
    }
    case Kind::Text:
        return raw;
    case Kind::Time: {
        const QDateTime t = QDateTime::fromString(raw.trimmed(), Qt::ISODate);
        return t.isValid() ? t : QDateTime();
    }
    }
    return {};
}

QVariant readKey(const KConfigGroup &g, const Key &k)
{
    const QString raw = g.readEntry(k.key, QString::fromLatin1(k.fallback));
    const QStringList list = g.readEntry(k.key, splitList(QString::fromLatin1(k.fallback)));
    return check(k, raw, list);
}

} // namespace

ShellSettings::ShellSettings(QObject *parent)
    : ShellSettings(KSharedConfig::openConfig(u"rayeshellrc"_s, KConfig::CascadeConfig), parent)
{
}

ShellSettings::ShellSettings(KSharedConfig::Ptr config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
{
    watch();
}

void ShellSettings::watch()
{
    m_watcher = KConfigWatcher::create(m_config);
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this,
            [this](const KConfigGroup &group, const QByteArrayList &names) {
                QStringList keys;
                for (const QByteArray &n : names)
                    keys << QString::fromUtf8(n);
                notifyGroup(group.name());
                Q_EMIT changed(group.name(), keys);
            });
}

void ShellSettings::notifyGroup(const QString &group)
{
    if (group == u"Taskbar"_s) {
        Q_EMIT taskbarChanged();
        // RecentSearches off hides the list
        Q_EMIT searchChanged();
    } else if (group == u"Tray"_s) {
        Q_EMIT trayChanged();
    } else if (group == u"Start"_s) {
        Q_EMIT startChanged();
    } else if (group == u"Clock"_s) {
        Q_EMIT clockChanged();
    } else if (group == u"Focus"_s) {
        Q_EMIT focusChanged();
    } else if (group == u"QuickSettings"_s) {
        Q_EMIT quickSettingsChanged();
    } else if (group == u"Notifications"_s) {
        Q_EMIT notificationsChanged();
    } else if (group == u"Sound"_s) {
        Q_EMIT soundChanged();
    } else if (group == u"Search"_s) {
        Q_EMIT searchChanged();
    } else if (group == u"JumpLists"_s) {
        Q_EMIT jumpListsChanged();
    }
}

void ShellSettings::reload()
{
    m_config->reparseConfiguration();
    for (const char *g : {"Taskbar", "Tray", "Start", "Clock", "Focus", "QuickSettings", "Notifications", "Sound", "Search", "JumpLists"})
        notifyGroup(QString::fromLatin1(g));
}

KSharedConfig::Ptr ShellSettings::config() const
{
    return m_config;
}

QVariant ShellSettings::value(const QString &group, const QString &key) const
{
    const Key *k = find(group, key);
    if (!k)
        return {};
    return readKey(m_config->group(group), *k);
}

QVariant ShellSettings::defaultValue(const QString &group, const QString &key)
{
    const Key *k = find(group, key);
    if (!k)
        return {};
    const QString raw = QString::fromLatin1(k->fallback);
    return check(*k, raw, splitList(raw));
}

QStringList ShellSettings::keys()
{
    QStringList out;
    for (const Key &k : Table)
        out << QLatin1String(k.group) + u'/' + QLatin1String(k.key);
    return out;
}

bool ShellSettings::isDesktopId(const QString &id)
{
    static const QRegularExpression re(u"^[A-Za-z0-9_][A-Za-z0-9._+-]{0,200}\\.desktop$"_s);
    return re.match(id).hasMatch() && !id.contains(u".."_s);
}

bool ShellSettings::isToken(const QString &id)
{
    static const QRegularExpression re(u"^[a-z0-9][a-z0-9-]{0,63}$"_s);
    return re.match(id).hasMatch();
}

void ShellSettings::write(const QString &group, const QString &key, const QVariant &value)
{
    KConfigGroup g = m_config->group(group);
    if (value.metaType().id() == QMetaType::QStringList)
        g.writeEntry(key, value.toStringList(), KConfig::Notify);
    else
        g.writeEntry(key, value.toString(), KConfig::Notify);
    m_config->sync();
    notifyGroup(group);
    Q_EMIT changed(group, {key});
}

// --- getters ------------------------------------------------------------

#define RAYE_STRING(name, group, key) \
    QString ShellSettings::name() const { return value(u"" group ""_s, u"" key ""_s).toString(); }
#define RAYE_BOOL(name, group, key) \
    bool ShellSettings::name() const { return value(u"" group ""_s, u"" key ""_s).toBool(); }
#define RAYE_LIST(name, group, key) \
    QStringList ShellSettings::name() const { return value(u"" group ""_s, u"" key ""_s).toStringList(); }

RAYE_STRING(alignment, "Taskbar", "Alignment")
RAYE_STRING(searchMode, "Taskbar", "Search")
RAYE_BOOL(taskView, "Taskbar", "TaskView")
RAYE_BOOL(widgets, "Taskbar", "Widgets")
RAYE_BOOL(autoHide, "Taskbar", "AutoHide")
RAYE_BOOL(badges, "Taskbar", "Badges")
RAYE_BOOL(flashing, "Taskbar", "Flashing")
RAYE_BOOL(allDisplays, "Taskbar", "AllDisplays")
RAYE_STRING(multiDisplayApps, "Taskbar", "MultiDisplayApps")
RAYE_BOOL(shareWindow, "Taskbar", "ShareWindow")
RAYE_BOOL(showDesktopCorner, "Taskbar", "ShowDesktopCorner")
RAYE_STRING(combine, "Taskbar", "Combine")
RAYE_STRING(smallButtons, "Taskbar", "SmallButtons")
RAYE_BOOL(recentSearches, "Taskbar", "RecentSearches")
RAYE_BOOL(endTask, "Taskbar", "EndTask")
RAYE_LIST(taskbarPinned, "Taskbar", "Pinned")
RAYE_BOOL(trayHiddenMenu, "Tray", "HiddenMenu")
RAYE_LIST(trayShown, "Tray", "Shown")
RAYE_LIST(trayHidden, "Tray", "Hidden")
RAYE_STRING(startLayout, "Start", "Layout")
RAYE_BOOL(startRecentApps, "Start", "RecentApps")
RAYE_BOOL(startMostUsed, "Start", "MostUsed")
RAYE_BOOL(startRecommended, "Start", "Recommended")
RAYE_LIST(startFolders, "Start", "Folders")
RAYE_STRING(startAllView, "Start", "AllView")
RAYE_LIST(startPinned, "Start", "Pinned")
RAYE_STRING(startPinnedFolders, "Start", "PinnedFolders")
RAYE_STRING(restartNeeded, "Start", "RestartNeeded")
RAYE_BOOL(clockShowSeconds, "Clock", "ShowSeconds")
RAYE_BOOL(clockShowDate, "Clock", "ShowDate")
RAYE_STRING(clockUse24h, "Clock", "Use24h")
RAYE_LIST(quickSettingsTiles, "QuickSettings", "Tiles")
RAYE_BOOL(lockBadges, "Notifications", "LockBadges")
RAYE_BOOL(volumeChime, "Sound", "VolumeChime")

#undef RAYE_STRING
#undef RAYE_BOOL
#undef RAYE_LIST

int ShellSettings::toastSeconds() const
{
    return value(u"Notifications"_s, u"ToastSeconds"_s).toInt();
}

QDateTime ShellSettings::focusUntil() const
{
    return value(u"Focus"_s, u"Until"_s).toDateTime();
}

QStringList ShellSettings::searchRecent() const
{
    if (!recentSearches())
        return {};
    return value(u"Search"_s, u"Recent"_s).toStringList();
}

// --- what the shell writes -------------------------------------------------

void ShellSettings::setTaskbarPinned(const QStringList &ids)
{
    const Key *k = find(u"Taskbar"_s, u"Pinned"_s);
    write(u"Taskbar"_s, u"Pinned"_s, check(*k, {}, ids));
}

void ShellSettings::setStartPinned(const QStringList &ids)
{
    const Key *k = find(u"Start"_s, u"Pinned"_s);
    write(u"Start"_s, u"Pinned"_s, check(*k, {}, ids));
}

void ShellSettings::setStartAllView(const QString &view)
{
    const Key *k = find(u"Start"_s, u"AllView"_s);
    write(u"Start"_s, u"AllView"_s, check(*k, view, {}));
}

void ShellSettings::setStartPinnedFolders(const QString &folders)
{
    write(u"Start"_s, u"PinnedFolders"_s, folders);
}

void ShellSettings::setFocusUntil(const QDateTime &until)
{
    write(u"Focus"_s, u"Until"_s, until.isValid() ? until.toString(Qt::ISODate) : QString());
}

void ShellSettings::setQuickSettingsTiles(const QStringList &tiles)
{
    const Key *k = find(u"QuickSettings"_s, u"Tiles"_s);
    write(u"QuickSettings"_s, u"Tiles"_s, check(*k, {}, tiles));
}

void ShellSettings::addRecentSearch(const QString &text)
{
    const QString t = text.trimmed().left(MaxSearchLength);
    if (t.isEmpty() || !recentSearches())
        return;
    QStringList list = searchRecent();
    list.removeAll(t);
    list.prepend(t);
    while (list.size() > MaxRecentSearches)
        list.removeLast();
    write(u"Search"_s, u"Recent"_s, list);
}

void ShellSettings::removeRecentSearch(const QString &text)
{
    QStringList list = value(u"Search"_s, u"Recent"_s).toStringList();
    if (list.removeAll(text.trimmed()) > 0)
        write(u"Search"_s, u"Recent"_s, list);
}

void ShellSettings::clearRecentSearches()
{
    write(u"Search"_s, u"Recent"_s, QStringList());
}

namespace
{
// only files on this computer, as file: URLs; nothing else is a pin
QStringList checkPins(const QStringList &urls)
{
    QStringList out;
    for (const QString &u : urls) {
        const QUrl url = u.startsWith(u'/') ? QUrl::fromLocalFile(u) : QUrl(u, QUrl::StrictMode);
        if (url.isValid() && url.isLocalFile() && !url.toLocalFile().isEmpty()) {
            const QString s = url.toString(QUrl::FullyEncoded);
            if (!out.contains(s))
                out << s;
        }
    }
    return out;
}
} // namespace

QStringList ShellSettings::jumpListPins(const QString &desktopId) const
{
    if (!isDesktopId(desktopId))
        return {};
    return checkPins(m_config->group(u"JumpLists"_s).readEntry(desktopId, QStringList()));
}

void ShellSettings::setJumpListPins(const QString &desktopId, const QStringList &urls)
{
    if (!isDesktopId(desktopId))
        return;
    KConfigGroup g = m_config->group(u"JumpLists"_s);
    const QStringList pins = checkPins(urls);
    if (pins.isEmpty())
        g.deleteEntry(desktopId, KConfig::Notify);
    else
        g.writeEntry(desktopId, pins, KConfig::Notify);
    m_config->sync();
    Q_EMIT jumpListsChanged();
    Q_EMIT changed(u"JumpLists"_s, {desktopId});
}
