// What the Project panel asks of the screens, for a laptop, a desktop with
// two equal screens and three screens.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "displayplan.h"

#include <QTest>

using namespace DisplayPlan;

namespace
{
Screen screen(const QString &name, bool embedded, bool enabled, QPoint pos, QSize size, QList<QSize> sizes = {})
{
    Screen s;
    s.name = name;
    s.embedded = embedded;
    s.enabled = enabled;
    s.pos = pos;
    s.size = size;
    s.preferred = size;
    s.sizes = sizes.isEmpty() ? QList<QSize>{size} : sizes;
    return s;
}

const QSize hd(1920, 1080);
const QSize small(1280, 800);

QList<Screen> laptop(bool internalOn, bool externalOn, QPoint externalPos = QPoint(1920, 0))
{
    return {
        // the external one sorts first by name: being built in must win
        screen(QStringLiteral("DP-1"), false, externalOn, externalPos, hd),
        screen(QStringLiteral("eDP-1"), true, internalOn, QPoint(0, 0), hd),
    };
}

const Target *find(const QList<Target> &plan, const QString &name)
{
    for (const Target &t : plan) {
        if (t.name == name)
            return &t;
    }
    return nullptr;
}
}

class TestDisplayPlan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theBuiltInScreenIsThePcScreen()
    {
        QCOMPARE(mainScreen(laptop(true, true)), 1);
    }

    // the choice must not follow what is on right now, or "PC screen only"
    // after "Second screen only" would keep the wrong screen
    void onADesktopThePcScreenDoesNotMoveWhenItIsOff()
    {
        QList<Screen> two = {
            screen(QStringLiteral("HDMI-2"), false, true, QPoint(0, 0), hd),
            screen(QStringLiteral("DP-1"), false, false, QPoint(0, 0), hd),
        };
        QCOMPARE(mainScreen(two), 1);
        two[1].enabled = true;
        two[0].enabled = false;
        QCOMPARE(mainScreen(two), 1);
    }

    void current_data()
    {
        QTest::addColumn<QList<Screen>>("screens");
        QTest::addColumn<int>("mode");
        QTest::newRow("laptop alone") << QList<Screen>{screen(QStringLiteral("eDP-1"), true, true, {}, hd)} << int(Mode::PcOnly);
        QTest::newRow("nothing connected") << QList<Screen>{} << int(Mode::Unknown);
        QTest::newRow("internal only") << laptop(true, false) << int(Mode::PcOnly);
        QTest::newRow("external only") << laptop(false, true) << int(Mode::SecondOnly);
        QTest::newRow("side by side") << laptop(true, true) << int(Mode::Extend);
        QTest::newRow("same place") << laptop(true, true, QPoint(0, 0)) << int(Mode::Duplicate);
        QTest::newRow("all off") << laptop(false, false) << int(Mode::Unknown);
    }
    void current()
    {
        QFETCH(QList<Screen>, screens);
        QFETCH(int, mode);
        QCOMPARE(int(DisplayPlan::current(screens)), mode);
    }

    // two of three on: not one of the four, nothing is highlighted
    void twoOfThreeOnIsNotOneOfTheFourChoices()
    {
        QList<Screen> three = laptop(true, true);
        three.append(screen(QStringLiteral("HDMI-1"), false, false, {}, hd));
        QCOMPARE(int(DisplayPlan::current(three)), int(Mode::Unknown));
    }

    void withOneScreenThereIsNothingToChoose()
    {
        const QList<Screen> one{screen(QStringLiteral("eDP-1"), true, true, {}, hd)};
        QVERIFY(!canChoose(one));
        QVERIFY(plan(Mode::Extend, one).isEmpty());
    }

    void pickingWhatIsAlreadyOnChangesNothing()
    {
        QVERIFY(plan(Mode::Extend, laptop(true, true)).isEmpty());
        QVERIFY(plan(Mode::PcOnly, laptop(true, false)).isEmpty());
    }

    void pcScreenOnlyTurnsTheOthersOff()
    {
        const QList<Target> p = plan(Mode::PcOnly, laptop(true, true));
        QCOMPARE(p.size(), 2);
        QVERIFY(find(p, QStringLiteral("eDP-1"))->enabled);
        QCOMPARE(find(p, QStringLiteral("eDP-1"))->priority, 1);
        QVERIFY(!find(p, QStringLiteral("DP-1"))->enabled);
    }

    void secondScreenOnlyTurnsThePcScreenOffAndGivesTheOtherTheMainPlace()
    {
        const QList<Target> p = plan(Mode::SecondOnly, laptop(true, true));
        QVERIFY(!find(p, QStringLiteral("eDP-1"))->enabled);
        const Target *other = find(p, QStringLiteral("DP-1"));
        QVERIFY(other->enabled);
        QCOMPARE(other->pos, QPoint(0, 0));
        QCOMPARE(other->priority, 1);
    }

    void extendPutsTheOthersToTheRightOfThePcScreen()
    {
        const QList<Target> p = plan(Mode::Extend, laptop(true, false));
        const Target *main = find(p, QStringLiteral("eDP-1"));
        const Target *other = find(p, QStringLiteral("DP-1"));
        QCOMPARE(main->pos, QPoint(0, 0));
        QCOMPARE(main->priority, 1);
        QVERIFY(other->enabled);
        QCOMPARE(other->pos, QPoint(1920, 0));
        QCOMPARE(other->priority, 2);
    }

    // a 200 % panel takes half its pixels on the desktop: the next screen
    // starts there, or the two overlap
    void extendCountsTheScaleAndASidewaysScreen()
    {
        QList<Screen> two = laptop(true, false);
        two[1].scale = 2.0;
        QCOMPARE(find(plan(Mode::Extend, two), QStringLiteral("DP-1"))->pos, QPoint(960, 0));
        two[1].scale = 1.0;
        two[1].sideways = true;
        QCOMPARE(find(plan(Mode::Extend, two), QStringLiteral("DP-1"))->pos, QPoint(1080, 0));
    }

    void duplicateUsesTheBiggestSizeBothCanDo()
    {
        const QList<Screen> two = {
            screen(QStringLiteral("eDP-1"), true, true, {}, hd, {hd, small}),
            screen(QStringLiteral("HDMI-1"), false, true, QPoint(1920, 0), QSize(2560, 1440), {QSize(2560, 1440), hd, small}),
        };
        const QList<Target> p = plan(Mode::Duplicate, two);
        for (const Target &t : p) {
            QVERIFY(t.enabled);
            QCOMPARE(t.pos, QPoint(0, 0));
            QCOMPARE(t.size, hd);
        }
    }

    void duplicateWithNoSizeInCommonKeepsEachScreensOwn()
    {
        const QList<Screen> two = {
            screen(QStringLiteral("eDP-1"), true, true, {}, hd),
            screen(QStringLiteral("HDMI-1"), false, true, QPoint(1920, 0), small),
        };
        const QList<Target> p = plan(Mode::Duplicate, two);
        QCOMPARE(find(p, QStringLiteral("eDP-1"))->size, hd);
        QCOMPARE(find(p, QStringLiteral("HDMI-1"))->size, small);
        QCOMPARE(find(p, QStringLiteral("HDMI-1"))->pos, QPoint(0, 0));
    }

    // out of Duplicate, a screen goes back to the size it was made for
    void leavingDuplicateGivesBackThePreferredSize()
    {
        QList<Screen> two = {
            screen(QStringLiteral("eDP-1"), true, true, {}, hd, {hd, small}),
            screen(QStringLiteral("HDMI-1"), false, true, {}, QSize(2560, 1440), {QSize(2560, 1440), hd, small}),
        };
        two[0].size = small;
        two[1].size = small;
        const QList<Target> p = plan(Mode::Extend, two);
        QCOMPARE(find(p, QStringLiteral("HDMI-1"))->size, QSize(2560, 1440));
        QCOMPARE(find(p, QStringLiteral("eDP-1"))->size, hd);
        QCOMPARE(find(p, QStringLiteral("HDMI-1"))->pos, QPoint(1920, 0));
    }

    void threeScreensExtendInARow()
    {
        QList<Screen> three = laptop(true, false);
        three.append(screen(QStringLiteral("HDMI-1"), false, false, {}, small));
        const QList<Target> p = plan(Mode::Extend, three);
        QCOMPARE(find(p, QStringLiteral("DP-1"))->pos, QPoint(1920, 0));
        QCOMPARE(find(p, QStringLiteral("HDMI-1"))->pos, QPoint(3840, 0));
        QCOMPARE(find(p, QStringLiteral("HDMI-1"))->priority, 3);
    }

    void namesRoundTrip()
    {
        for (Mode m : {Mode::PcOnly, Mode::Duplicate, Mode::Extend, Mode::SecondOnly})
            QCOMPARE(int(modeFromName(modeName(m))), int(m));
        QCOMPARE(int(modeFromName(QStringLiteral("nonsense"))), int(Mode::Unknown));
    }
};

QTEST_GUILESS_MAIN(TestDisplayPlan)
#include "tst_displayplan.moc"
