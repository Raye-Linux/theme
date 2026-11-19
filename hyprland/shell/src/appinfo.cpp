// SPDX-License-Identifier: GPL-3.0-or-later

#include "appinfo.h"

#include <KApplicationTrader>
#include <KService>
#include <KSycoca>

AppInfo::AppInfo(QObject *parent)
    : QObject(parent)
{
    // the app database may not be built yet in a new account
    KSycoca::self()->ensureCacheValid();
    // an app installed or removed while the session runs
    connect(KSycoca::self(), &KSycoca::databaseChanged, this, [this] {
        m_cache.clear();
    });
}

QString AppInfo::lookup(const QString &windowClass)
{
    if (windowClass.isEmpty())
        return {};
    // the app id is the desktop file name for most Wayland apps
    for (const QString &name : {windowClass, windowClass.toLower()}) {
        if (KService::Ptr s = KService::serviceByDesktopName(name))
            return s->desktopEntryName() + QStringLiteral(".desktop");
    }
    // X11 apps and a few others name their window after StartupWMClass,
    // or after the last part of a reverse-DNS id (dolphin, org.kde.dolphin)
    const KService::List apps = KApplicationTrader::query([&windowClass](const KService::Ptr &s) {
        if (s->noDisplay())
            return false;
        const QString wm = s->property<QString>(QStringLiteral("StartupWMClass"));
        if (!wm.isEmpty())
            return wm.compare(windowClass, Qt::CaseInsensitive) == 0;
        const QString entry = s->desktopEntryName();
        return entry.endsWith(QLatin1Char('.') + windowClass.toLower());
    });
    if (!apps.isEmpty())
        return apps.first()->desktopEntryName() + QStringLiteral(".desktop");
    return {};
}

QVariantMap AppInfo::forDesktopId(const QString &desktopId)
{
    QString name = desktopId;
    if (name.endsWith(QLatin1String(".desktop")))
        name.chop(8);
    const KService::Ptr s = KService::serviceByDesktopName(name);
    if (!s)
        return {};
    return {{QStringLiteral("id"), QString(s->desktopEntryName() + QStringLiteral(".desktop"))},
            {QStringLiteral("name"), s->name()},
            {QStringLiteral("icon"), s->icon()}};
}

QVariantMap AppInfo::forClass(const QString &windowClass, const QString &initialClass)
{
    const QString key = windowClass + QLatin1Char('\n') + initialClass;
    auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd())
        return it.value();
    QString id = lookup(windowClass);
    if (id.isEmpty() && initialClass != windowClass)
        id = lookup(initialClass);
    const QVariantMap entry = id.isEmpty() ? QVariantMap() : forDesktopId(id);
    m_cache.insert(key, entry);
    return entry;
}

QVariantMap AppInfo::forClients(const QVariantList &clients)
{
    QVariantMap out;
    for (const QVariant &c : clients) {
        const QVariantMap client = c.toMap();
        const QVariantMap entry = forClass(client.value(QStringLiteral("class")).toString(),
                                           client.value(QStringLiteral("initialClass")).toString());
        if (!entry.isEmpty())
            out.insert(client.value(QStringLiteral("address")).toString(), entry);
    }
    return out;
}
