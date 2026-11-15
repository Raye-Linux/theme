// SPDX-License-Identifier: GPL-3.0-or-later

#include "launcherentry.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>

using namespace Qt::StringLiterals;

namespace
{
const auto s_interface = u"com.canonical.Unity.LauncherEntry"_s;
const auto s_signal = u"Update"_s;
const auto s_unityName = u"com.canonical.Unity"_s;
}

LauncherEntryWatcher::LauncherEntryWatcher(QObject *parent)
    : QObject(parent)
{
}

LauncherEntryWatcher::~LauncherEntryWatcher()
{
    stop();
}

void LauncherEntryWatcher::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    if (m_active) {
        start();
    } else {
        stop();
    }
    Q_EMIT activeChanged();
}

void LauncherEntryWatcher::setAnnounceUnity(bool announce)
{
    if (m_announceUnity == announce) {
        return;
    }
    m_announceUnity = announce;
    if (m_active) {
        if (m_announceUnity) {
            takeUnityName();
        } else {
            dropUnityName();
        }
    }
    Q_EMIT announceUnityChanged();
}

void LauncherEntryWatcher::start()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    // every sender, every path: an application picks its own object path
    // (libunity hashes the desktop id into it), so the path cannot be named
    bus.connect(QString(), QString(), s_interface, s_signal, this, SLOT(onUpdate(QDBusMessage)));

    if (!m_watcher) {
        m_watcher = new QDBusServiceWatcher(this);
        m_watcher->setConnection(bus);
        m_watcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
        connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, &LauncherEntryWatcher::onServiceUnregistered);
    }
    if (m_announceUnity) {
        takeUnityName();
    }
}

void LauncherEntryWatcher::stop()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        bus.disconnect(QString(), QString(), s_interface, s_signal, this, SLOT(onUpdate(QDBusMessage)));
    }
    dropUnityName();
    if (m_watcher) {
        const QStringList watched = m_watcher->watchedServices();
        for (const QString &service : watched) {
            m_watcher->removeWatchedService(service);
        }
    }
    // the badges go with it: nothing is left asserting a stale number
    const auto services = m_urisByService;
    m_urisByService.clear();
    for (auto it = services.cbegin(); it != services.cend(); ++it) {
        for (const QString &uri : it.value()) {
            Q_EMIT entryGone(uri);
        }
    }
}

void LauncherEntryWatcher::takeUnityName()
{
    if (m_announcing) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    // Not ReplaceExisting: another dock holding it is not ours to take,
    // and the signals are broadcast to everyone anyway.
    m_announcing = bus.isConnected() && bus.registerService(s_unityName);
    Q_EMIT announcingChanged();
}

void LauncherEntryWatcher::dropUnityName()
{
    if (!m_announcing) {
        return;
    }
    QDBusConnection::sessionBus().unregisterService(s_unityName);
    m_announcing = false;
    Q_EMIT announcingChanged();
}

void LauncherEntryWatcher::onUpdate(const QDBusMessage &message)
{
    const QList<QVariant> args = message.arguments();
    if (args.size() < 2) {
        return;
    }
    const QString appUri = args.at(0).toString();
    if (appUri.isEmpty()) {
        return;
    }

    // a{sv} arrives as a QDBusArgument unless something already demarshalled it
    QVariantMap properties;
    const QVariant payload = args.at(1);
    if (payload.canConvert<QVariantMap>()) {
        properties = payload.toMap();
    } else if (payload.canConvert<QDBusArgument>()) {
        QDBusArgument arg = payload.value<QDBusArgument>();
        arg >> properties;
    }

    // Remember who speaks for this app, so that its going away can take
    // the badge with it. An application uses its unique name (":1.42")
    // far more often than a well-known one; both are watchable.
    const QString service = message.service();
    if (!service.isEmpty()) {
        QSet<QString> &uris = m_urisByService[service];
        if (!uris.contains(appUri)) {
            uris.insert(appUri);
            if (m_watcher && !m_watcher->watchedServices().contains(service)) {
                m_watcher->addWatchedService(service);
            }
        }
    }

    Q_EMIT entryUpdate(appUri, properties);
}

void LauncherEntryWatcher::onServiceUnregistered(const QString &service)
{
    const QSet<QString> uris = m_urisByService.take(service);
    if (m_watcher) {
        m_watcher->removeWatchedService(service);
    }
    for (const QString &uri : uris) {
        Q_EMIT entryGone(uri);
    }
}
