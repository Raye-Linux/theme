// ShellSettings: the defaults of /etc/xdg/rayeshellrc, the checks on what
// a user file may say, what the shell writes, and live changes.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shellsettings.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDBusConnection>
#include <QFile>
#include <QSignalSpy>
#include <QDir>
#include <QStandardPaths>
#include <QTest>

using namespace Qt::StringLiterals;

class TestShellSettings : public QObject
{
    Q_OBJECT

private:
    int m_n = 0;
    QStringList m_files;

    // a user file over the shipped defaults, the way the session reads
    // them. It lives in the test's own XDG_CONFIG_HOME, under a plain name:
    // KConfigWatcher only follows files named that way.
    KSharedConfig::Ptr config(const QByteArray &user = {})
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        QDir().mkpath(dir);
        const QString name = u"rayeshelltest-%1-%2rc"_s.arg(QCoreApplication::applicationPid()).arg(++m_n);
        QFile f(dir + u'/' + name);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("cannot write the test config");
        f.write(user);
        f.close();
        m_files << f.fileName();
        auto c = KSharedConfig::openConfig(name, KConfig::SimpleConfig);
        c->addConfigSources({QStringLiteral(RAYE_SHELLRC)});
        c->reparseConfiguration();
        return c;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(QFile::exists(QStringLiteral(RAYE_SHELLRC)));
        // never the real home
        QVERIFY(qEnvironmentVariableIsSet("XDG_CONFIG_HOME"));
    }

    void cleanupTestCase()
    {
        for (const QString &f : std::as_const(m_files))
            QFile::remove(f);
    }

    void defaults()
    {
        ShellSettings s(config());
        QCOMPARE(s.alignment(), u"center"_s);
        QCOMPARE(s.searchMode(), u"box"_s);
        QVERIFY(s.taskView());
        QVERIFY(!s.widgets());
        QVERIFY(!s.autoHide());
        QVERIFY(s.badges());
        QVERIFY(s.flashing());
        QVERIFY(!s.allDisplays());
        QCOMPARE(s.multiDisplayApps(), u"all"_s);
        QVERIFY(!s.shareWindow());
        QVERIFY(s.showDesktopCorner());
        QCOMPARE(s.combine(), u"always"_s);
        QCOMPARE(s.smallButtons(), u"never"_s);
        QVERIFY(s.recentSearches());
        QVERIFY(!s.endTask());
        QCOMPARE(s.taskbarPinned(),
                 QStringList({u"org.kde.dolphin.desktop"_s, u"firefox.desktop"_s,
                              u"org.raye.store.desktop"_s, u"org.raye.settings.desktop"_s}));
        QVERIFY(s.trayHiddenMenu());
        QVERIFY(s.trayShown().isEmpty());
        QVERIFY(s.trayHidden().isEmpty());
        QCOMPARE(s.startLayout(), u"default"_s);
        QVERIFY(s.startRecentApps());
        QVERIFY(!s.startMostUsed());
        QVERIFY(s.startRecommended());
        QCOMPARE(s.startFolders(), QStringList({u"settings"_s, u"files"_s}));
        QCOMPARE(s.startAllView(), u"category"_s);
        QCOMPARE(s.startPinned().size(), 11);
        QCOMPARE(s.startPinned().first(), u"org.kde.dolphin.desktop"_s);
        QVERIFY(s.startPinnedFolders().isEmpty());
        QVERIFY(s.restartNeeded().isEmpty());
        QVERIFY(!s.clockShowSeconds());
        QVERIFY(s.clockShowDate());
        QCOMPARE(s.clockUse24h(), u"auto"_s);
        QVERIFY(!s.focusUntil().isValid());
        QCOMPARE(s.quickSettingsTiles().first(), u"wifi"_s);
        QCOMPARE(s.toastSeconds(), 5);
        QVERIFY(s.lockBadges());
        QVERIFY(s.searchRecent().isEmpty());
        QVERIFY(s.volumeChime());
        QCOMPARE(s.look(), u"windows11"_s);
        QVERIFY(!s.windows10());
        QVERIFY(s.startTiles().isEmpty());
        QCOMPARE(s.startTileColumns(), u"auto"_s);
    }

    // The Windows 10 look keeps the taskbar on the left, but the person's
    // own alignment stays in the file for when they go back to 11
    void windows10Look()
    {
        ShellSettings s(config("[Appearance]\nLook=windows10\n[Taskbar]\nAlignment=center\n"));
        QCOMPARE(s.look(), u"windows10"_s);
        QVERIFY(s.windows10());
        QCOMPARE(s.alignment(), u"left"_s);
        QCOMPARE(s.value(u"Taskbar"_s, u"Alignment"_s).toString(), u"center"_s);

        ShellSettings eleven(config("[Appearance]\nLook=windows11\n[Taskbar]\nAlignment=center\n"));
        QVERIFY(!eleven.windows10());
        QCOMPARE(eleven.alignment(), u"center"_s);

        ShellSettings shortName(config("[Appearance]\nLook=10\n"));
        QVERIFY(shortName.windows10());
        ShellSettings win(config("[Appearance]\nLook=Win10\n"));
        QVERIFY(win.windows10());
        ShellSettings odd(config("[Appearance]\nLook=vista\n"));
        QCOMPARE(odd.look(), u"windows11"_s);
    }

    void tilesAreKept()
    {
        auto c = config("[Start]\nTileColumns=7\n");
        ShellSettings s(c);
        QCOMPARE(s.startTileColumns(), u"auto"_s);
        QSignalSpy start(&s, &ShellSettings::startChanged);
        s.setStartTiles(u"Work|org.kde.kate.desktop=wide;|firefox.desktop=small"_s);
        s.setStartTileColumns(u"2"_s);
        QCOMPARE(start.count(), 2);
        QCOMPARE(s.startTiles(), u"Work|org.kde.kate.desktop=wide;|firefox.desktop=small"_s);
        QCOMPARE(s.startTileColumns(), u"2"_s);
        s.setStartTileColumns(u"many"_s);
        QCOMPARE(s.startTileColumns(), u"auto"_s);
    }

    // Settings shows a row only when its key is in the shipped file: the
    // file and the table must list the same keys with the same defaults
    void fileAndTableAgree()
    {
        KConfig shipped(QStringLiteral(RAYE_SHELLRC), KConfig::SimpleConfig);
        QStringList inFile;
        for (const QString &group : shipped.groupList()) {
            if (group == u"JumpLists"_s)
                continue;
            for (const QString &key : shipped.group(group).keyList())
                inFile << group + u'/' + key;
        }
        inFile.sort();
        QStringList table = ShellSettings::keys();
        table.sort();
        QCOMPARE(inFile, table);

        ShellSettings s(config());
        for (const QString &k : table) {
            const auto parts = k.split(u'/');
            QCOMPARE(s.value(parts[0], parts[1]), ShellSettings::defaultValue(parts[0], parts[1]));
        }
        QVERIFY(!s.value(u"Taskbar"_s, u"Nope"_s).isValid());
    }

    void userFileWins()
    {
        ShellSettings s(config("[Taskbar]\nAlignment=left\nSearch=hidden\nTaskView=false\n"
                               "[Clock]\nShowSeconds=true\nUse24h=true\n"
                               "[Notifications]\nToastSeconds=30\n"));
        QCOMPARE(s.alignment(), u"left"_s);
        QCOMPARE(s.searchMode(), u"hidden"_s);
        QVERIFY(!s.taskView());
        QVERIFY(s.clockShowSeconds());
        QCOMPARE(s.clockUse24h(), u"true"_s);
        QCOMPARE(s.toastSeconds(), 30);
        // untouched keys keep the shipped default
        QCOMPARE(s.combine(), u"always"_s);
    }

    void badValuesFallBack()
    {
        ShellSettings s(config("[Taskbar]\nAlignment=sideways\nSearch=\nCombine=sometimes\n"
                               "Badges=maybe\nPinned=../../etc/passwd,a b.desktop,good.desktop,good.desktop,-x.desktop\n"
                               "[Start]\nFolders=settings,nowhere,files,settings\nAllView=3d\n"
                               "RestartNeeded=kernel;rm -rf\n"
                               "[Notifications]\nToastSeconds=0\n"
                               "[QuickSettings]\nTiles=wifi,,Wi Fi,bluetooth,wifi\n"));
        QCOMPARE(s.alignment(), u"center"_s);
        QCOMPARE(s.searchMode(), u"box"_s);
        QCOMPARE(s.combine(), u"always"_s);
        QVERIFY(s.badges()); // not a boolean: the default
        QCOMPARE(s.taskbarPinned(), QStringList({u"good.desktop"_s}));
        QCOMPARE(s.startFolders(), QStringList({u"settings"_s, u"files"_s}));
        QCOMPARE(s.startAllView(), u"category"_s);
        QVERIFY(s.restartNeeded().isEmpty());
        QCOMPARE(s.toastSeconds(), 5);
        QCOMPARE(s.quickSettingsTiles(), QStringList({u"wifi"_s, u"bluetooth"_s}));

        ShellSettings big(config("[Notifications]\nToastSeconds=86400\n"));
        QCOMPARE(big.toastSeconds(), 5);
        ShellSettings text(config("[Notifications]\nToastSeconds=soon\n"));
        QCOMPARE(text.toastSeconds(), 5);
    }

    // the names Raye Settings' first plan used for some values
    void valueAliases()
    {
        ShellSettings s(config("[Taskbar]\nSearch=iconLabel\nCombine=whenFull\nSmallButtons=whenFull\n"
                               "MultiDisplayApps=mainAndOpen\nAlignment=LEFT\n"
                               "[Start]\nLayout=morePins\n"));
        QCOMPARE(s.searchMode(), u"iconlabel"_s);
        QCOMPARE(s.combine(), u"full"_s);
        QCOMPARE(s.smallButtons(), u"full"_s);
        QCOMPARE(s.multiDisplayApps(), u"main-and-open"_s);
        QCOMPARE(s.alignment(), u"left"_s);
        QCOMPARE(s.startLayout(), u"pins"_s);
        ShellSettings r(config("[Start]\nLayout=moreRecommended\n"));
        QCOMPARE(r.startLayout(), u"recommendations"_s);
    }

    void volumeChimeOff()
    {
        ShellSettings off(config("[Sound]\nVolumeChime=false\n"));
        QVERIFY(!off.volumeChime());
        ShellSettings odd(config("[Sound]\nVolumeChime=loud\n"));
        QVERIFY(odd.volumeChime());
    }

    void restartReason()
    {
        ShellSettings s(config("[Start]\nRestartNeeded=kernel-update\n"));
        QCOMPARE(s.restartNeeded(), u"kernel-update"_s);
    }

    void recentSearches()
    {
        auto c = config();
        ShellSettings s(c);
        QSignalSpy spy(&s, &ShellSettings::searchChanged);
        s.addRecentSearch(u"  notepad "_s);
        s.addRecentSearch(u"calc"_s);
        s.addRecentSearch(u"notepad"_s);
        s.addRecentSearch(u"   "_s);
        QCOMPARE(s.searchRecent(), QStringList({u"notepad"_s, u"calc"_s}));
        QVERIFY(spy.count() >= 3);
        for (int i = 0; i < 20; ++i)
            s.addRecentSearch(u"q%1"_s.arg(i));
        QCOMPARE(s.searchRecent().size(), ShellSettings::MaxRecentSearches);
        QCOMPARE(s.searchRecent().first(), u"q19"_s);
        s.addRecentSearch(QString(1000, u'x'));
        QCOMPARE(s.searchRecent().first().size(), ShellSettings::MaxSearchLength);
        // a comma must not split one search into two
        s.addRecentSearch(u"a, b"_s);
        QCOMPARE(s.searchRecent().first(), u"a, b"_s);
        s.removeRecentSearch(u"a, b"_s);
        QVERIFY(!s.searchRecent().contains(u"a, b"_s));
        // the write landed in the user file
        KConfig back(c->name(), KConfig::SimpleConfig);
        QVERIFY(back.group(u"Search"_s).readEntry("Recent", QStringList()).contains(u"q19"_s));
        s.clearRecentSearches();
        QVERIFY(s.searchRecent().isEmpty());

        ShellSettings off(config("[Taskbar]\nRecentSearches=false\n[Search]\nRecent=old\n"));
        QVERIFY(off.searchRecent().isEmpty());
        off.addRecentSearch(u"secret"_s);
        QVERIFY(off.searchRecent().isEmpty());
    }

    void jumpLists()
    {
        ShellSettings s(config());
        s.setJumpListPins(u"org.kde.kate.desktop"_s,
                          {u"file:///home/me/notes.txt"_s, u"https://example.org/x"_s,
                           u"/home/me/plain.txt"_s, u"file:///home/me/notes.txt"_s});
        QCOMPARE(s.jumpListPins(u"org.kde.kate.desktop"_s),
                 QStringList({u"file:///home/me/notes.txt"_s, u"file:///home/me/plain.txt"_s}));
        // a key that is not a desktop id is never written or read
        s.setJumpListPins(u"../evil"_s, {u"file:///x"_s});
        QVERIFY(s.jumpListPins(u"../evil"_s).isEmpty());
        QVERIFY(!s.config()->group(u"JumpLists"_s).hasKey("../evil"));
        s.setJumpListPins(u"org.kde.kate.desktop"_s, {});
        QVERIFY(!s.config()->group(u"JumpLists"_s).hasKey("org.kde.kate.desktop"));
    }

    void focus()
    {
        ShellSettings s(config());
        const QDateTime until = QDateTime(QDate(2026, 11, 1), QTime(14, 30), QTimeZone::UTC);
        s.setFocusUntil(until);
        QCOMPARE(s.focusUntil(), until);
        s.setFocusUntil(QDateTime());
        QVERIFY(!s.focusUntil().isValid());
        ShellSettings bad(config("[Focus]\nUntil=tomorrow\n"));
        QVERIFY(!bad.focusUntil().isValid());
    }

    void pinsAreChecked()
    {
        ShellSettings s(config());
        s.setTaskbarPinned({u"org.kde.kate.desktop"_s, u"bad id"_s, u"org.kde.kate.desktop"_s});
        QCOMPARE(s.taskbarPinned(), QStringList({u"org.kde.kate.desktop"_s}));
        s.setStartPinned({u"firefox.desktop"_s});
        QCOMPARE(s.startPinned(), QStringList({u"firefox.desktop"_s}));
        s.setQuickSettingsTiles({u"bluetooth"_s, u"wifi"_s});
        QCOMPARE(s.quickSettingsTiles(), QStringList({u"bluetooth"_s, u"wifi"_s}));
        s.setStartAllView(u"grid"_s);
        QCOMPARE(s.startAllView(), u"grid"_s);
        // not a view: back to the default
        s.setStartAllView(u"bogus"_s);
        QCOMPARE(s.startAllView(), u"category"_s);
        s.setStartPinnedFolders(u"[{\"name\":\"Games\"}]"_s);
        QCOMPARE(s.startPinnedFolders(), u"[{\"name\":\"Games\"}]"_s);
    }

    void checks()
    {
        QVERIFY(ShellSettings::isDesktopId(u"org.kde.dolphin.desktop"_s));
        QVERIFY(ShellSettings::isDesktopId(u"org.mozilla.firefox_beta-1.desktop"_s));
        QVERIFY(!ShellSettings::isDesktopId(u"dolphin"_s));
        QVERIFY(!ShellSettings::isDesktopId(u"../x.desktop"_s));
        QVERIFY(!ShellSettings::isDesktopId(u"a/b.desktop"_s));
        QVERIFY(!ShellSettings::isDesktopId(u"-a.desktop"_s));
        QVERIFY(ShellSettings::isToken(u"energysaver"_s));
        QVERIFY(ShellSettings::isToken(u"kernel-update"_s));
        QVERIFY(!ShellSettings::isToken(u"Wi Fi"_s));
        QVERIFY(!ShellSettings::isToken(u""_s));
    }

    // Raye Settings writes with KConfig::Notify: the shell follows at once
    void followsChanges()
    {
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("no session bus (run under dbus-run-session)");
        auto c = config();
        ShellSettings s(c);
        QSignalSpy taskbar(&s, &ShellSettings::taskbarChanged);
        QSignalSpy clock(&s, &ShellSettings::clockChanged);
        KConfig writer(c->name(), KConfig::SimpleConfig);
        writer.group(u"Taskbar"_s).writeEntry("Alignment", u"left"_s, KConfig::Notify);
        writer.sync();
        QVERIFY(taskbar.wait(5000));
        QCOMPARE(s.alignment(), u"left"_s);
        QCOMPARE(clock.count(), 0);

        // the look: Start, the taskbar and every surface follow at once
        QSignalSpy look(&s, &ShellSettings::lookChanged);
        taskbar.clear();
        writer.group(u"Appearance"_s).writeEntry("Look", u"windows10"_s, KConfig::Notify);
        writer.sync();
        QVERIFY(look.wait(5000));
        QVERIFY(s.windows10());
        QVERIFY(taskbar.count() >= 1);
    }
};

QTEST_GUILESS_MAIN(TestShellSettings)
#include "tst_shellsettings.moc"
