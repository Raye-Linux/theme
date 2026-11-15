// The clock asks Raye Calendar's tool for a month and gets its events back.
// The tool here is fake-calendar-tool, which is slow for November.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../clockapplet.h"

#include <KPluginMetaData>
#include <QGuiApplication>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

using namespace Qt::StringLiterals;

class TestAgenda : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void oneMonth()
    {
        RayeClockApplet clock(nullptr, KPluginMetaData(), {});
        QSignalSpy ready(&clock, &RayeClockApplet::agendaReady);
        clock.requestAgenda(u"2026-11-26"_s, u"2027-01-10"_s);
        QVERIFY(ready.wait(5000));
        QCOMPARE(ready.first().first().toString().trimmed(), u"[\"2026-11-26\"]"_s);
    }

    // November is still on its way when December is asked for: its answer
    // must not replace December's
    void lastMonthWins()
    {
        RayeClockApplet clock(nullptr, KPluginMetaData(), {});
        QSignalSpy ready(&clock, &RayeClockApplet::agendaReady);
        clock.requestAgenda(u"2026-10-26"_s, u"2026-12-13"_s);
        clock.requestAgenda(u"2026-11-26"_s, u"2027-01-10"_s);
        QVERIFY(ready.wait(5000));
        QTest::qWait(1500);
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first().first().toString().trimmed(), u"[\"2026-11-26\"]"_s);
    }

    // the centre is sized on the screen of the clock that opened it, not on
    // the one its window was made on (two-screens.json: 1366 x 768, then
    // 1920 x 1080 to its right)
    void clockScreen()
    {
        const auto screens = QGuiApplication::screens();
        QCOMPARE(screens.size(), 2);
        RayeClockApplet clock(nullptr, KPluginMetaData(), {});
        QWindow panel;
        panel.setScreen(screens.at(1));
        QWindow centre;
        QCOMPARE(centre.screen(), screens.at(0));
        QCOMPARE(clock.moveToClockScreen(&centre, nullptr), QRect());
        const QRect area = clock.moveToClockScreen(&centre, &panel);
        QCOMPARE(area, screens.at(1)->availableGeometry());
        QCOMPARE(area.height(), 1080);
        QCOMPARE(centre.screen(), screens.at(1));
    }
};

QTEST_MAIN(TestAgenda)
#include "tst_agenda.moc"
