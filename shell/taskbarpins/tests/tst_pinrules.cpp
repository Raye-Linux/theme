// One list of pinned apps for every taskbar (src/pinrules.cpp).
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pinrules.h"

#include <QTest>

using namespace RayePins;
using namespace Qt::StringLiterals;

class PinRulesTest : public QObject
{
    Q_OBJECT

private:
    static QList<Bar> bars(const QStringList &main, const QStringList &two, const QStringList &three = {u"?"_s})
    {
        QList<Bar> out{{u"main"_s, true, main}, {u"two"_s, false, two}};
        if (three != QStringList{u"?"_s})
            out.append({u"three"_s, false, three});
        return out;
    }

    const QStringList a{u"applications:a.desktop"_s};
    const QStringList ab{u"applications:a.desktop"_s, u"applications:b.desktop"_s};
    const QStringList ba{u"applications:b.desktop"_s, u"applications:a.desktop"_s};

private Q_SLOTS:
    void modes()
    {
        QCOMPARE(buttonsFrom(u"all"_s), Buttons::All);
        QCOMPARE(buttonsFrom(u" Main-And-Window "_s), Buttons::MainAndWindow);
        QCOMPARE(buttonsFrom(u"window-only"_s), Buttons::WindowOnly);
        QCOMPARE(buttonsFrom(u"odd"_s), Buttons::All);
        QCOMPARE(buttonsFrom(QString()), Buttons::All);
    }

    void pinOnMainReachesEveryCopy()
    {
        const auto out = afterChange(bars(ab, a, a), u"main"_s, Buttons::All);
        QCOMPARE(out.size(), 2);
        QCOMPARE(out.value(u"two"_s), ab);
        QCOMPARE(out.value(u"three"_s), ab);
    }

    void pinOnACopyReachesMainAndTheOthers()
    {
        const auto out = afterChange(bars(a, ab, a), u"two"_s, Buttons::All);
        QCOMPARE(out.size(), 2);
        QCOMPARE(out.value(u"main"_s), ab);
        QCOMPARE(out.value(u"three"_s), ab);
    }

    void unpinAndMoveToo()
    {
        QCOMPARE(afterChange(bars(ab, a), u"two"_s, Buttons::All).value(u"main"_s), a);
        QCOMPARE(afterChange(bars(ab, ba), u"two"_s, Buttons::All).value(u"main"_s), ba);
        // everything unpinned is a list too
        QCOMPARE(afterChange(bars(ab, {}), u"two"_s, Buttons::All).value(u"main"_s), QStringList());
        QVERIFY(afterChange(bars(ab, {}), u"two"_s, Buttons::All).contains(u"main"_s));
    }

    void nothingWhenTheyAlreadyMatch()
    {
        QVERIFY(afterChange(bars(ab, ab, ab), u"two"_s, Buttons::All).isEmpty());
        QVERIFY(afterChange(bars(ab, {}), u"main"_s, Buttons::WindowOnly).isEmpty());
    }

    void onlyMainKeepsPinsInTheOtherModes()
    {
        for (Buttons mode : {Buttons::MainAndWindow, Buttons::WindowOnly}) {
            // pinned on a copy: goes to the main taskbar, the copy stays empty
            auto out = afterChange(bars(a, ba), u"two"_s, mode);
            QCOMPARE(out.value(u"main"_s), ab);
            QCOMPARE(out.value(u"two"_s), QStringList());
            // pinned on the main one: copies keep no pins
            out = afterChange(bars(ab, {}), u"main"_s, mode);
            QVERIFY(out.isEmpty());
            out = afterChange(bars(ab, a), u"main"_s, mode);
            QCOMPARE(out.value(u"two"_s), QStringList());
            QVERIFY(!out.contains(u"main"_s));
        }
    }

    void unknownBarChangesNothing()
    {
        QVERIFY(afterChange(bars(ab, a), u"elsewhere"_s, Buttons::All).isEmpty());
    }

    void noMainTaskbar()
    {
        QList<Bar> two{{u"two"_s, false, ab}, {u"three"_s, false, a}};
        QCOMPARE(afterChange(two, u"two"_s, Buttons::All).value(u"three"_s), ab);
        QCOMPARE(afterChange(two, u"two"_s, Buttons::WindowOnly).value(u"two"_s), QStringList());
    }
};

QTEST_GUILESS_MAIN(PinRulesTest)
#include "tst_pinrules.moc"
