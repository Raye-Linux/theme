// SPDX-License-Identifier: GPL-3.0-or-later

#include "taskbarpins.h"
#include "pinrules.h"

#include <KConfigLoader>
#include <KLocalizedString>
#include <KConfigPropertyMap>
#include <KPluginFactory>
#include <KSharedConfig>
#include <Plasma/Applet>
#include <Plasma/Containment>
#include <Plasma/Corona>

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QMenu>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QProcess>
#include <QTimer>

using namespace Qt::StringLiterals;

K_PLUGIN_CLASS_WITH_JSON(TaskbarPins, "metadata.json")

static const QStringList s_taskManagers = {u"org.kde.plasma.icontasks"_s, u"org.kde.plasma.taskmanager"_s};
// applets of the taskbar whose Plasma settings stay out of their menus
// (the Raye ones have none to show)
static const QStringList s_noSettings = {u"org.kde.plasma.icontasks"_s, u"org.kde.plasma.taskmanager"_s,
                                         u"org.kde.plasma.systemtray"_s, u"org.kde.plasma.digitalclock"_s,
                                         u"org.kde.plasma.kickoff"_s, u"org.kde.plasma.showdesktop"_s};
static const char s_lastPins[] = "_rayeLastLaunchers";
static const char s_watched[] = "_rayeWatched";
static const char s_queued[] = "_rayeQueued";
static const char s_reading[] = "_rayeReading";

// The stock task manager names itself in its menu ("Pin to Task
// Manager"). Its menu is made of QML each time it opens; just before it
// shows, the words are swapped for the taskbar's, in the user's language.
// The same way, the "Configure..." entry of an item of the tray (network,
// sound, battery...) becomes the Raye Settings page Windows opens there.

namespace
{
struct SettingsEntry {
    const char *domain;
    const char *context; // nullptr: none
    const char *text;
    const char *page;
};
const SettingsEntry s_settingsEntries[] = {
    {"plasma_applet_org.kde.plasma.networkmanagement", nullptr, "&Configure Network Connections…", "network"},
    {"plasma_applet_org.kde.plasma.volume", nullptr, "&Configure Audio Devices…", "system/sound"},
    {"plasma_applet_org.kde.plasma.battery", nullptr, "&Configure Power Management…", "system/power"},
    {"plasma_applet_org.kde.plasma.brightness", nullptr, "&Configure Power Management…", "system/power"},
    {"plasma_applet_org.kde.plasma.brightness", "@action:inmenu", "Configure Night Light…", "system/night-light"},
    {"plasma_applet_org.kde.plasma.bluetooth", "@action:inmenu", "Configure &Bluetooth…", "devices/bluetooth"},
    {"plasma_applet_org.kde.plasma.clipboard", nullptr, "Configure Clipboard…", "system/clipboard"},
    {"plasma_applet_org.kde.plasma.devicenotifier", "Open auto mounter kcm", "Configure Removable Devices…", "devices/autoplay"},
    {"plasma_applet_org.kde.plasma.notifications", nullptr, "&Configure Event Notifications and Actions…", "system/notifications"},
};

// what the Raye entry says for a page
QString settingsText(const QString &page)
{
    if (page == u"network"_s)
        return i18nc("@action:inmenu", "Network and Internet settings");
    if (page == u"system/sound"_s)
        return i18nc("@action:inmenu", "Sound settings");
    if (page == u"system/power"_s)
        return i18nc("@action:inmenu", "Power settings");
    if (page == u"system/night-light"_s)
        return i18nc("@action:inmenu", "Night light settings");
    if (page == u"devices/bluetooth"_s)
        return i18nc("@action:inmenu", "Bluetooth settings");
    if (page == u"system/clipboard"_s)
        return i18nc("@action:inmenu", "Clipboard settings");
    if (page == u"devices/autoplay"_s)
        return i18nc("@action:inmenu", "AutoPlay settings");
    return i18nc("@action:inmenu", "Notification settings");
}
} // namespace
class MenuWords : public QObject
{
public:
    // entries of Plasma's settings that may still be in a menu: the panel
    // shows its "configure" again when it builds its views
    static QStringList &hidden()
    {
        static QStringList texts;
        return texts;
    }

    static void install()
    {
        static MenuWords *words = nullptr;
        if (!words) {
            words = new MenuWords(QCoreApplication::instance());
            QCoreApplication::instance()->installEventFilter(words);
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::Show)
            return false;
        auto *menu = qobject_cast<QMenu *>(watched);
        if (!menu)
            return false;
        static const char domain[] = "plasma_applet_org.kde.plasma.taskmanager";
        const QString pin = plain(i18ndc(domain, "action:inmenu", "&Pin to Task Manager"));
        const QString unpin = plain(i18ndc(domain, "action:inmenu", "Unpin from Task Manager"));
        const auto actions = menu->actions();
        for (QAction *a : actions) {
            const QString text = plain(a->text());
            if (const QString page = settingsPage(text); !page.isEmpty() && a->isVisible()) {
                // the stock entry runs its own module: a Raye one takes its place
                auto *raye = new QAction(QIcon::fromTheme(u"configure"_s), settingsText(page), menu);
                connect(raye, &QAction::triggered, raye, [page] {
                    QProcess::startDetached(u"raye-settings"_s, {u"--page"_s, page});
                });
                menu->insertAction(a, raye);
                a->setVisible(false);
                continue;
            }
            if (text == pin)
                a->setText(i18nc("@action:inmenu", "&Pin to taskbar"));
            else if (text == unpin)
                a->setText(i18nc("@action:inmenu", "&Unpin from taskbar"));
            else if (!text.isEmpty() && hidden().contains(text))
                a->setVisible(false);
        }
        return false;
    }

private:
    using QObject::QObject;

    static QString plain(QString text)
    {
        return text.remove(QLatin1Char('&'));
    }

    static QString settingsPage(const QString &text)
    {
        if (text.isEmpty())
            return {};
        for (const SettingsEntry &e : s_settingsEntries) {
            const QString stock = e.context ? i18ndc(e.domain, e.context, e.text) : i18nd(e.domain, e.text);
            if (plain(stock) == text)
                return QString::fromLatin1(e.page);
        }
        return {};
    }
};

TaskbarPins::TaskbarPins(QObject *parent, const QVariantList &args)
    : Plasma::ContainmentActions(parent, args)
{
    MenuWords::install();
}

QList<QAction *> TaskbarPins::contextualActions()
{
    return {};
}

void TaskbarPins::restore(const KConfigGroup &config)
{
    Q_UNUSED(config);
    Plasma::Containment *c = containment();
    if (!c || m_watched == c)
        return;
    m_watched = c;
    // "Show Panel Configuration" and "Add or Manage Widgets" in the menu
    // of every item of the taskbar
    auto hidePanelSettings = [c] {
        for (const QString &name : {u"configure"_s, u"add widgets"_s}) {
            QAction *action = c->internalAction(name);
            if (!action)
                continue;
            action->setVisible(false);
            const QString text = action->text().remove(QLatin1Char('&'));
            if (!MenuWords::hidden().contains(text))
                MenuWords::hidden().append(text);
        }
    };
    hidePanelSettings();
    QTimer::singleShot(0, this, hidePanelSettings);
    connect(c, &Plasma::Applet::immutabilityChanged, this, hidePanelSettings);
    connect(c, &Plasma::Containment::uiReadyChanged, this, hidePanelSettings);
    const auto applets = c->applets();
    for (Plasma::Applet *a : applets)
        watch(a);
    connect(c, &Plasma::Containment::appletAdded, this, [this](Plasma::Applet *a) {
        watch(a);
    });
}

static void tell(QObject *map, const QString &key);

static bool isTaskManager(Plasma::Applet *a)
{
    return s_taskManagers.contains(a->pluginMetaData().pluginId());
}

static QStringList launchersOf(Plasma::Applet *a)
{
    KConfigLoader *scheme = a->configScheme();
    KConfigSkeletonItem *item = scheme ? scheme->findItemByName(u"launchers"_s) : nullptr;
    return item ? item->property().toStringList() : QStringList();
}

// "Configure Icons-Only Task Manager…": Plasma shows it again when the
// widgets get locked or unlocked, so it is hidden again then
void TaskbarPins::hideSettingsEntries(Plasma::Applet *applet)
{
    if (QAction *configure = applet->internalAction(u"configure"_s))
        configure->setVisible(false);
    if (QAction *alternatives = applet->internalAction(u"alternatives"_s))
        alternatives->setVisible(false);
}

void TaskbarPins::watch(Plasma::Applet *applet)
{
    // the items inside the tray (network, sound...) keep their own
    // settings entries out of their menus too
    if (applet->pluginMetaData().pluginId() == u"org.kde.plasma.systemtray"_s && !applet->property(s_watched).toBool()) {
        if (auto *tray = qobject_cast<Plasma::Containment *>(applet)) {
            const auto items = tray->applets();
            for (Plasma::Applet *a : items)
                hideSettingsEntries(a);
            connect(tray, &Plasma::Containment::appletAdded, this, [this](Plasma::Applet *a) {
                hideSettingsEntries(a);
                connect(a, &Plasma::Applet::immutabilityChanged, a, [this, a] {
                    hideSettingsEntries(a);
                });
            });
            for (Plasma::Applet *a : items) {
                connect(a, &Plasma::Applet::immutabilityChanged, a, [this, a] {
                    hideSettingsEntries(a);
                });
            }
        }
    }
    if (s_noSettings.contains(applet->pluginMetaData().pluginId()) && !applet->property(s_watched).toBool()) {
        hideSettingsEntries(applet);
        connect(applet, &Plasma::Applet::immutabilityChanged, applet, [this, applet] {
            hideSettingsEntries(applet);
        });
    }
    KConfigLoader *scheme = applet->configScheme();
    if (!scheme || applet->property(s_watched).toBool())
        return;
    applet->setProperty(s_watched, true);
    if (isTaskManager(applet))
        applet->setProperty(s_lastPins, launchersOf(applet));
    // Later, not at once: a desktop script writes the setting both through
    // the applet's settings and straight into its config group, the second
    // after the signal. Reading the group once both are done also turns a
    // list written as one "a,b" string back into a list.
    connect(scheme, &KCoreConfigSkeleton::configChanged, applet, [this, applet] {
        if (applet->property(s_queued).toBool())
            return;
        applet->setProperty(s_queued, true);
        QTimer::singleShot(0, applet, [this, applet] {
            applet->setProperty(s_queued, false);
            changed(applet);
        });
    });
}

void TaskbarPins::changed(Plasma::Applet *applet)
{
    if (applet->property(s_reading).toBool())
        return;
    KConfigLoader *scheme = applet->configScheme();
    applet->setProperty(s_reading, true);
    scheme->read();
    applet->setProperty(s_reading, false);

    // what was written outside the applet's QML: hand it over
    if (KConfigPropertyMap *map = applet->configuration()) {
        const auto items = scheme->items();
        for (KConfigSkeletonItem *item : items) {
            const QVariant value = item->property();
            if (map->value(item->key()) != value)
                map->insert(item->key(), value);
        }
    }
    if (!isTaskManager(applet))
        return;
    const QStringList now = launchersOf(applet);
    if (applet->property(s_lastPins).toStringList() == now)
        return;
    applet->setProperty(s_lastPins, now);
    // the settings map may already hold the list (it follows the settings
    // on its own in some cases) without the task manager having been told:
    // tell it, it ignores a list it already shows
    if (KConfigPropertyMap *map = applet->configuration())
        tell(map, u"launchers"_s);
    share(applet);
}

// emit the change signal QML listens to for one key of a settings map
static void tell(QObject *map, const QString &key)
{
    const QMetaObject *mo = map->metaObject();
    const int index = mo->indexOfProperty(key.toUtf8().constData());
    if (index < 0)
        return;
    const QMetaMethod notify = mo->property(index).notifySignal();
    if (notify.isValid())
        notify.invoke(map, Qt::DirectConnection);
}

static bool isCopy(Plasma::Containment *c)
{
    return c->config().group(u"Raye"_s).readEntry("Role", QString()) == u"secondary"_s;
}

void TaskbarPins::share(Plasma::Applet *source)
{
    Plasma::Corona *corona = source->containment() ? source->containment()->corona() : nullptr;
    if (!corona)
        return;

    // the bars: the task manager of every copy, and of the main taskbar
    // (the user's own panel with one, on the main screen when there are several)
    QList<RayePins::Bar> bars;
    QHash<QString, Plasma::Applet *> byId;
    Plasma::Applet *main = nullptr;
    int mainScreen = -2;
    const auto containments = corona->containments();
    for (Plasma::Containment *c : containments) {
        if (c->containmentType() != Plasma::Containment::Panel)
            continue;
        const bool copy = isCopy(c);
        const auto applets = c->applets();
        for (Plasma::Applet *a : applets) {
            if (!isTaskManager(a))
                continue;
            if (copy) {
                const QString id = QString::number(a->id());
                bars.append({id, false, launchersOf(a)});
                byId.insert(id, a);
            } else if (!main || (c->screen() == 0 && mainScreen != 0)) {
                main = a;
                mainScreen = c->screen();
            }
            break;
        }
    }
    if (main) {
        const QString id = QString::number(main->id());
        bars.prepend({id, true, launchersOf(main)});
        byId.insert(id, main);
    }

    KSharedConfigPtr rc = KSharedConfig::openConfig(u"raye-taskbarrc"_s);
    rc->reparseConfiguration();
    const auto buttons = RayePins::buttonsFrom(rc->group(u"Taskbar"_s).readEntry("MultiDisplayButtons", QString()));

    const auto writes = RayePins::afterChange(bars, QString::number(source->id()), buttons);
    for (auto it = writes.cbegin(); it != writes.cend(); ++it) {
        Plasma::Applet *target = byId.value(it.key());
        KConfigLoader *scheme = target ? target->configScheme() : nullptr;
        KConfigSkeletonItem *item = scheme ? scheme->findItemByName(u"launchers"_s) : nullptr;
        if (!item)
            continue;
        // known before it is written, so the target does not send it back
        target->setProperty(s_lastPins, it.value());
        item->setProperty(it.value());
        scheme->save(); // tells the target, which hands it to its QML
        Q_EMIT target->configNeedsSaving();
    }
}

#include "taskbarpins.moc"
