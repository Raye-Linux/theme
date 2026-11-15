// com.canonical.Unity.LauncherEntry: the unread count an application
// publishes about ITSELF, which is what a badge on a taskbar button
// means. The taskbar used to count notifications instead, so clearing
// the notification centre cleared every badge while the mail was still
// unread, and reading a message in the app left the badge behind.
//
// An application emits, on the session bus:
//   com.canonical.Unity.LauncherEntry.Update(
//       "application://org.kde.kmail2.desktop",
//       { "count": 5, "count-visible": true })
// Telegram, Discord, Thunderbird, Nextcloud and the GNOME/Elementary
// mail clients all speak it.
//
// This class is a pipe and nothing else: it hands QML the appUri and
// the properties exactly as they arrived, and says when a sender has
// gone. What the numbers MEAN is decided in taskbarlogic.js
// (launcherEntryKey, applyLauncherEntry, dropLauncherEntry), where it
// can be tested without a bus.
//
// THE INTERFACE HAS NO "WHAT IS IT NOW?". It is signals only: an
// application says its count when it changes, and there is nothing to
// ask. So after the shell restarts the badges are empty until each
// application next speaks -- which is the other reason the bus name
// below is taken: libunity re-publishes when it appears. An application
// that rolls its own and only emits on a change will show nothing until
// its next message, and no amount of code here can make it answer.
//
// WHY IT WATCHES THE SENDERS. An application that quits, crashes or
// is killed sends no farewell. Without the watch its last number stays
// on the icon for ever, and a badge nobody can clear sends someone into
// an app to look for a message that is not there -- worse than no badge.
// Every sender is watched for unregistration, and its entries go with it.
//
// It also takes the bus name com.canonical.Unity while it runs.
// libunity only starts publishing when something owns that name, so
// without it a part of the applications say nothing at all; every dock
// that supports this interface does the same. The cost is that a few
// applications read that name as "a full Unity is here" and switch on
// Unity-only extras of their own. `announceUnity` turns it off for
// anyone who would rather have the quieter half.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusMessage>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantMap>
#include <qqmlregistration.h>

class QDBusServiceWatcher;

class LauncherEntryWatcher : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // off by default: the taskbar switches it on with "Show badges"
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool announceUnity READ announceUnity WRITE setAnnounceUnity NOTIFY announceUnityChanged)
    // whether the name was really taken; nothing depends on it, it is
    // there so a failure can be seen instead of guessed at
    Q_PROPERTY(bool announcing READ isAnnouncing NOTIFY announcingChanged)

public:
    explicit LauncherEntryWatcher(QObject *parent = nullptr);
    ~LauncherEntryWatcher() override;

    bool isActive() const
    {
        return m_active;
    }
    void setActive(bool active);

    bool announceUnity() const
    {
        return m_announceUnity;
    }
    void setAnnounceUnity(bool announce);

    bool isAnnouncing() const
    {
        return m_announcing;
    }

Q_SIGNALS:
    void activeChanged();
    void announceUnityChanged();
    void announcingChanged();
    // an application said something about itself
    void entryUpdate(const QString &appUri, const QVariantMap &properties);
    // a sender is gone and will say nothing more about this app
    void entryGone(const QString &appUri);

private Q_SLOTS:
    // Takes the whole message: the sender's name is what lets a gone
    // application be matched to the badges it left behind.
    // QDBusMessage is included, not forward declared: moc puts every
    // slot argument in a metatype array that needs the complete type,
    // and a forward declaration fails to compile there.
    void onUpdate(const QDBusMessage &message);
    void onServiceUnregistered(const QString &service);

private:
    void start();
    void stop();
    void takeUnityName();
    void dropUnityName();

    bool m_active = false;
    bool m_announceUnity = true;
    bool m_announcing = false;
    QDBusServiceWatcher *m_watcher = nullptr;
    QHash<QString, QSet<QString>> m_urisByService;
};
