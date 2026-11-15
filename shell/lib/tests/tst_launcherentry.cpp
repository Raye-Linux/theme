// com.canonical.Unity.LauncherEntry on a real bus: an application says
// what its own count is, and loses its badge when it goes away.
//
// Needs a session bus of its own; CMake runs it under dbus-run-session.
//
// The case worth the test is anApplicationThatGoesAwayTakesItsBadge\-
// WithIt. An application that quits, crashes or is killed sends no
// farewell, so without the sender watch its last number stays on the
// icon for ever -- a badge nobody can clear, sending someone into an
// app to look for a message that is not there.
//
// AND IT IS EASY TO WRITE THAT CHECK SO IT CANNOT PASS.
// QDBusConnection::disconnectFromBus() does NOT close a connection that
// is still referenced, and it says so only as a warning on stderr: a
// local QDBusConnection still in scope keeps the sender alive, no
// NameOwnerChanged is ever emitted, and the check fails against a
// perfectly good implementation. Measured 2026-11-06: that is exactly
// how this file failed the first time it ran. Every sender here lives
// in a scope of its own and is let go BEFORE disconnectFromBus, and
// every test uses a connection name of its own so one leaking cannot
// be handed to the next.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "launcherentry.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TestLauncherEntry : public QObject
{
    Q_OBJECT

private:
    static inline const QString s_mail = u"application://org.kde.kmail2.desktop"_s;

    // one Update, sent the way an application sends it
    static void say(QDBusConnection &bus, const QString &appUri, const QVariantMap &properties)
    {
        QDBusMessage m = QDBusMessage::createSignal(u"/org/raye/test/launcherentry"_s, u"com.canonical.Unity.LauncherEntry"_s, u"Update"_s);
        m << appUri << properties;
        QVERIFY(bus.send(m));
    }

    // A sender of its own, let go before the bus is told to drop it.
    // Returns once the connection is really gone.
    static void closeApp(const QString &name)
    {
        QDBusConnection::disconnectFromBus(name);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY2(QDBusConnection::sessionBus().isConnected(), "no session bus: run under dbus-run-session");
    }

    void sleepsUntilSwitchedOn()
    {
        LauncherEntryWatcher w;
        QCOMPARE(w.isActive(), false);
        QCOMPARE(w.isAnnouncing(), false);

        QSignalSpy updates(&w, &LauncherEntryWatcher::entryUpdate);
        const QString name = u"raye-test-sleeps"_s;
        {
            QDBusConnection app = QDBusConnection::connectToBus(QDBusConnection::SessionBus, name);
            QVERIFY(app.isConnected());
            say(app, s_mail, {{u"count"_s, 5}, {u"count-visible"_s, true}});
            // nothing is listening yet, so nothing may arrive
            QTest::qWait(150);
            QCOMPARE(updates.count(), 0);
        }
        closeApp(name);
    }

    void passesOnWhatTheApplicationSaid()
    {
        LauncherEntryWatcher w;
        w.setActive(true);
        // a private bus has no other dock on it, so the name is free
        QVERIFY(w.isAnnouncing());

        QSignalSpy updates(&w, &LauncherEntryWatcher::entryUpdate);
        const QString name = u"raye-test-passes-on"_s;
        {
            QDBusConnection app = QDBusConnection::connectToBus(QDBusConnection::SessionBus, name);
            QVERIFY(app.isConnected());

            say(app, s_mail, {{u"count"_s, 5}, {u"count-visible"_s, true}});
            QVERIFY(updates.wait(5000));
            QCOMPARE(updates.count(), 1);
            QCOMPARE(updates.at(0).at(0).toString(), s_mail);
            const QVariantMap got = updates.at(0).at(1).toMap();
            QCOMPARE(got.value(u"count"_s).toInt(), 5);
            QCOMPARE(got.value(u"count-visible"_s).toBool(), true);

            // a second update about the same app is passed on as it is:
            // what it means is decided in taskbarlogic.js, not here
            say(app, s_mail, {{u"count"_s, 7}, {u"count-visible"_s, false}});
            QTRY_COMPARE(updates.count(), 2);
            QCOMPARE(updates.at(1).at(1).toMap().value(u"count-visible"_s).toBool(), false);
        }
        closeApp(name);
    }

    void anApplicationThatGoesAwayTakesItsBadgeWithIt()
    {
        LauncherEntryWatcher w;
        w.setActive(true);

        QSignalSpy updates(&w, &LauncherEntryWatcher::entryUpdate);
        QSignalSpy gone(&w, &LauncherEntryWatcher::entryGone);
        const QString name = u"raye-test-goes-away"_s;
        {
            QDBusConnection app = QDBusConnection::connectToBus(QDBusConnection::SessionBus, name);
            QVERIFY(app.isConnected());
            say(app, s_mail, {{u"count"_s, 5}, {u"count-visible"_s, true}});
            QVERIFY(updates.wait(5000));
            QCOMPARE(gone.count(), 0);
        }
        // the application quits without saying anything
        closeApp(name);
        QVERIFY(gone.wait(5000));
        QCOMPARE(gone.count(), 1);
        QCOMPARE(gone.at(0).at(0).toString(), s_mail);
    }

    void switchingItOffWithdrawsEveryBadge()
    {
        // "Show badges on taskbar buttons" off has to clear what is on
        // screen, not freeze it
        LauncherEntryWatcher w;
        w.setActive(true);

        QSignalSpy updates(&w, &LauncherEntryWatcher::entryUpdate);
        QSignalSpy gone(&w, &LauncherEntryWatcher::entryGone);
        const QString name = u"raye-test-switch-off"_s;
        {
            QDBusConnection app = QDBusConnection::connectToBus(QDBusConnection::SessionBus, name);
            say(app, s_mail, {{u"count"_s, 5}, {u"count-visible"_s, true}});
            QVERIFY(updates.wait(5000));

            w.setActive(false);
            QCOMPARE(gone.count(), 1);
            QCOMPARE(gone.at(0).at(0).toString(), s_mail);
            QCOMPARE(w.isAnnouncing(), false);

            // and nothing arrives any more
            const int seen = updates.count();
            say(app, s_mail, {{u"count"_s, 9}, {u"count-visible"_s, true}});
            QTest::qWait(150);
            QCOMPARE(updates.count(), seen);
        }
        closeApp(name);
    }

    void anEmptyUriIsNotAnApplication()
    {
        LauncherEntryWatcher w;
        w.setActive(true);
        QSignalSpy updates(&w, &LauncherEntryWatcher::entryUpdate);
        const QString name = u"raye-test-empty-uri"_s;
        {
            QDBusConnection app = QDBusConnection::connectToBus(QDBusConnection::SessionBus, name);
            say(app, QString(), {{u"count"_s, 5}, {u"count-visible"_s, true}});
            QTest::qWait(200);
            QCOMPARE(updates.count(), 0);
        }
        closeApp(name);
    }

    void theUnityNameCanBeLeftAlone()
    {
        LauncherEntryWatcher w;
        w.setAnnounceUnity(false);
        w.setActive(true);
        QCOMPARE(w.isAnnouncing(), false);
        // and the signals still arrive: owning the name only wakes the
        // applications that wait for it, it is not how they are heard
        QSignalSpy updates(&w, &LauncherEntryWatcher::entryUpdate);
        const QString name = u"raye-test-no-unity-name"_s;
        {
            QDBusConnection app = QDBusConnection::connectToBus(QDBusConnection::SessionBus, name);
            say(app, s_mail, {{u"count"_s, 2}, {u"count-visible"_s, true}});
            QVERIFY(updates.wait(5000));
        }
        closeApp(name);
    }
};

QTEST_MAIN(TestLauncherEntry)
#include "tst_launcherentry.moc"
