// "Recently added": which apps count as new, and for how long.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "recentlyadded.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace
{
constexpr qint64 Day = 24 * 60 * 60;
constexpr qint64 Now = 1793462520;
}

class TestRecentlyAdded : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void firstRunNothingIsNew()
    {
        const auto seen = RecentlyAdded::updateSeen({}, {u"a.desktop"_s, u"b.desktop"_s}, Now, true);
        QCOMPARE(seen.value(u"a.desktop"_s, -1), 0);
        QCOMPARE(seen.value(u"b.desktop"_s, -1), 0);
        QVERIFY(RecentlyAdded::select(seen, {}, Now, 7, 3).isEmpty());
    }

    void newAppGetsTheTimeItIsSeen()
    {
        QHash<QString, qint64> seen{{u"a.desktop"_s, 0}};
        seen = RecentlyAdded::updateSeen(seen, {u"a.desktop"_s, u"new.desktop"_s}, Now, false);
        QCOMPARE(seen.value(u"new.desktop"_s), Now);
        QCOMPARE(seen.value(u"a.desktop"_s), 0);
        // seen again later: keeps its first time
        seen = RecentlyAdded::updateSeen(seen, {u"a.desktop"_s, u"new.desktop"_s}, Now + 100, false);
        QCOMPARE(seen.value(u"new.desktop"_s), Now);
        QCOMPARE(RecentlyAdded::select(seen, {}, Now + 100, 7, 3), QStringList({u"new.desktop"_s}));
    }

    void removedAndInstalledAgainIsNew()
    {
        QHash<QString, qint64> seen{{u"a.desktop"_s, 0}};
        seen = RecentlyAdded::updateSeen(seen, {}, Now, false);
        QVERIFY(!seen.contains(u"a.desktop"_s));
        seen = RecentlyAdded::updateSeen(seen, {u"a.desktop"_s}, Now + 10, false);
        QCOMPARE(seen.value(u"a.desktop"_s), Now + 10);
    }

    void age()
    {
        const QHash<QString, qint64> seen{{u"week.desktop"_s, Now - 7 * Day},
                                          {u"older.desktop"_s, Now - 7 * Day - 1},
                                          {u"future.desktop"_s, Now + 2 * Day},
                                          {u"skew.desktop"_s, Now + 60}};
        const QStringList got = RecentlyAdded::select(seen, {}, Now, 7, 10);
        QCOMPARE(got, QStringList({u"skew.desktop"_s, u"week.desktop"_s}));
    }

    void openedIsNoLongerNew()
    {
        const QHash<QString, qint64> seen{{u"a.desktop"_s, Now - Day}, {u"b.desktop"_s, Now - Day}};
        // b was opened after it arrived; a only before (an old install)
        const QHash<QString, qint64> launched{{u"a.desktop"_s, Now - 30 * Day}, {u"b.desktop"_s, Now - 10}};
        QCOMPARE(RecentlyAdded::select(seen, launched, Now, 7, 3), QStringList({u"a.desktop"_s}));
    }

    void newestFirstAndLimit()
    {
        const QHash<QString, qint64> seen{{u"c.desktop"_s, Now - 3 * Day},
                                          {u"a.desktop"_s, Now - Day},
                                          {u"b.desktop"_s, Now - Day},
                                          {u"d.desktop"_s, Now - 2 * Day}};
        QCOMPARE(RecentlyAdded::select(seen, {}, Now, 7, 3),
                 QStringList({u"a.desktop"_s, u"b.desktop"_s, u"d.desktop"_s}));
        QVERIFY(RecentlyAdded::select(seen, {}, Now, 7, 0).isEmpty());
    }

    void model()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        qint64 clock = Now;
        QList<AppEntry> apps{{u"old.desktop"_s, u"Old"_s, u"old"_s}};
        QHash<QString, qint64> launched;

        RecentlyAddedModel m;
        m.setStorePath(dir.filePath(u"shell/apps-seen"_s));
        m.setClock([&clock] { return clock; });
        m.setAppSource([&apps] { return apps; });
        m.setLaunchSource([&launched] { return launched; });
        m.refresh();
        QCOMPARE(m.rowCount(), 0);

        clock += Day;
        apps.append({u"org.example.Game.desktop"_s, u"Game"_s, u"game"_s});
        m.refresh();
        QCOMPARE(m.rowCount(), 1);
        const QModelIndex i = m.index(0, 0);
        QCOMPARE(m.data(i, RecentlyAddedModel::DesktopIdRole).toString(), u"org.example.Game.desktop"_s);
        QCOMPARE(m.data(i, RecentlyAddedModel::NameRole).toString(), u"Game"_s);
        QCOMPARE(m.data(i, RecentlyAddedModel::IconNameRole).toString(), u"game"_s);
        QCOMPARE(m.data(i, RecentlyAddedModel::AddedRole).toDateTime().toSecsSinceEpoch(), clock);
        QCOMPARE(m.roleNames().value(RecentlyAddedModel::DesktopIdRole), QByteArray("desktopId"));

        // the store survives: a new model (next login) still knows
        RecentlyAddedModel again;
        again.setStorePath(dir.filePath(u"shell/apps-seen"_s));
        again.setClock([&clock] { return clock + 3600; });
        again.setAppSource([&apps] { return apps; });
        again.setLaunchSource([&launched] { return launched; });
        again.refresh();
        QCOMPARE(again.rowCount(), 1);

        launched.insert(u"org.example.Game.desktop"_s, clock + 60);
        clock += 120;
        QSignalSpy count(&m, &RecentlyAddedModel::countChanged);
        m.refresh();
        QCOMPARE(m.rowCount(), 0);
        QVERIFY(count.count() >= 1);
    }

    void bounds()
    {
        RecentlyAddedModel m;
        m.setLimit(-3);
        QCOMPARE(m.limit(), 0);
        m.setMaxAgeDays(0);
        QCOMPARE(m.maxAgeDays(), 1);
        m.setMaxAgeDays(1000);
        QCOMPARE(m.maxAgeDays(), 90);
    }
};

QTEST_GUILESS_MAIN(TestRecentlyAdded)
#include "tst_recentlyadded.moc"
