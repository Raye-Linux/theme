// What the Project panel keeps of the screens when it applies a choice:
// a change the screens refuse must leave it knowing them as they are, so
// the same choice, picked again, is tried again.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "displaycontroller.h"

#include <KScreen/Config>
#include <KScreen/Mode>
#include <KScreen/Output>
#include <KScreen/Screen>

#include <QSignalSpy>
#include <QTest>

namespace
{
KScreen::OutputPtr output(int id, const QString &name, KScreen::Output::Type type, QPoint pos)
{
    KScreen::ModePtr mode(new KScreen::Mode);
    mode->setId(QStringLiteral("1080"));
    mode->setSize(QSize(1920, 1080));
    mode->setRefreshRate(60);

    KScreen::OutputPtr o(new KScreen::Output);
    o->setId(id);
    o->setName(name);
    o->setType(type);
    o->setConnected(true);
    o->setEnabled(true);
    o->setPos(pos);
    o->setModes({{mode->id(), mode}});
    o->setCurrentModeId(mode->id());
    o->setPreferredModes({mode->id()});
    return o;
}

// a laptop with a second screen on its right: "extend"
KScreen::ConfigPtr extended()
{
    // a config read from the system always has one, and clone() needs it
    KScreen::ScreenPtr screen(new KScreen::Screen);
    screen->setMaxSize(QSize(8192, 8192));
    KScreen::ConfigPtr config(new KScreen::Config);
    config->setScreen(screen);
    config->setOutputs({
        {1, output(1, QStringLiteral("eDP-1"), KScreen::Output::Panel, QPoint(0, 0))},
        {2, output(2, QStringLiteral("DP-1"), KScreen::Output::DisplayPort, QPoint(1920, 0))},
    });
    return config;
}

// the screens as a test wants them to answer
class Controller : public DisplayController
{
public:
    explicit Controller(const KScreen::ConfigPtr &config)
        : DisplayController(config, nullptr)
        , system(config->clone())
    {
    }

    bool accept = false;
    int sends = 0;
    int reads = 0;
    // what the screens really are
    KScreen::ConfigPtr system;

protected:
    void read() override
    {
        ++reads;
        loaded(system->clone());
    }

    void send(const KScreen::ConfigPtr &config) override
    {
        ++sends;
        if (accept)
            system = config->clone();
        sent(config, accept, accept ? QString() : QStringLiteral("refused"));
    }
};
}

class TestDisplayController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aRefusedChangeLeavesTheScreensAsTheyAre()
    {
        Controller c(extended());
        QSignalSpy failed(&c, &DisplayController::failed);
        QCOMPARE(c.currentMode(), QStringLiteral("extend"));

        c.choose(QStringLiteral("pc-only"));
        QCOMPARE(c.sends, 1);
        QCOMPARE(failed.count(), 1);
        QVERIFY(!c.busy());
        QCOMPARE(c.currentMode(), QStringLiteral("extend"));
        // and it asks the screens where they are, in case part of it went through
        QCOMPARE(c.reads, 1);
    }

    void theSameChoiceIsTriedAgainAfterARefusal()
    {
        Controller c(extended());
        QSignalSpy chosen(&c, &DisplayController::chosen);

        c.choose(QStringLiteral("pc-only"));
        c.accept = true;
        c.choose(QStringLiteral("pc-only"));
        QCOMPARE(c.sends, 2);
        QCOMPARE(chosen.count(), 1);
        QCOMPARE(c.currentMode(), QStringLiteral("pc-only"));
    }

    void anAcceptedChangeIsWhatItKnows()
    {
        Controller c(extended());
        c.accept = true;
        QSignalSpy chosen(&c, &DisplayController::chosen);

        c.choose(QStringLiteral("pc-only"));
        QCOMPARE(chosen.count(), 1);
        QCOMPARE(c.currentMode(), QStringLiteral("pc-only"));
        QCOMPARE(c.reads, 0);
    }
};

QTEST_GUILESS_MAIN(TestDisplayController)
#include "tst_displaycontroller.moc"
