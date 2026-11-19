// The Raye title bar of Qt apps on Hyprland: where the buttons sit, what
// a click hits, double clicks and colours.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "titlebar.h"

#include <QTest>

using namespace Raye;
using TitleBar::Part;

class TestTitleBar : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void buttonsSitOnTheRight()
    {
        const TitleBar::Buttons all;
        QCOMPARE(TitleBar::height(), 32);
        QCOMPARE(TitleBar::buttonRect(Part::Close, 800, all), QRectF(754, 0, 46, 32));
        QCOMPARE(TitleBar::buttonRect(Part::Maximize, 800, all), QRectF(708, 0, 46, 32));
        QCOMPARE(TitleBar::buttonRect(Part::Minimize, 800, all), QRectF(662, 0, 46, 32));
        QVERIFY(!TitleBar::buttonRect(Part::Title, 800, all).isValid());
    }

    void dialogsHaveCloseOnly()
    {
        const TitleBar::Buttons dialog = TitleBar::buttonsFor(true, false);
        QVERIFY(!dialog.minimize && !dialog.maximize);
        QVERIFY(!TitleBar::buttonRect(Part::Maximize, 400, dialog).isValid());
        QCOMPARE(TitleBar::hitTest(QPointF(330, 10), 400, dialog), Part::Title);
        QCOMPARE(TitleBar::hitTest(QPointF(380, 10), 400, dialog), Part::Close);
    }

    void fixedSizeWindowsCannotMaximize()
    {
        const TitleBar::Buttons fixed = TitleBar::buttonsFor(false, true);
        QVERIFY(fixed.minimize && !fixed.maximize);
        // Minimize moves next to Close
        QCOMPARE(TitleBar::buttonRect(Part::Minimize, 400, fixed), QRectF(308, 0, 46, 32));
    }

    void hitTest_data()
    {
        QTest::addColumn<QPointF>("point");
        QTest::addColumn<int>("part");
        QTest::newRow("title") << QPointF(100, 16) << int(Part::Title);
        QTest::newRow("icon") << QPointF(15, 16) << int(Part::Title);
        QTest::newRow("close") << QPointF(799, 0) << int(Part::Close);
        QTest::newRow("close-left-edge") << QPointF(754, 31) << int(Part::Close);
        QTest::newRow("maximize") << QPointF(753, 16) << int(Part::Maximize);
        QTest::newRow("minimize") << QPointF(662, 16) << int(Part::Minimize);
        QTest::newRow("just-left-of-minimize") << QPointF(661.5, 16) << int(Part::Title);
        QTest::newRow("content") << QPointF(400, 32) << int(Part::None);
        QTest::newRow("outside") << QPointF(801, 10) << int(Part::None);
        QTest::newRow("above") << QPointF(400, -1) << int(Part::None);
    }
    void hitTest()
    {
        QFETCH(QPointF, point);
        QFETCH(int, part);
        QCOMPARE(int(TitleBar::hitTest(point, 800, TitleBar::Buttons())), part);
    }

    void titleStopsBeforeTheButtons()
    {
        const QRectF withIcon = TitleBar::titleRect(800, true, TitleBar::Buttons());
        QCOMPARE(withIcon.left(), 40.0);
        QCOMPARE(withIcon.right(), 800.0 - 3 * 46 - 8);
        QCOMPARE(TitleBar::titleRect(800, false, TitleBar::Buttons()).left(), 12.0);
        QCOMPARE(TitleBar::iconRect(), QRectF(12, 8, 16, 16));
        // a very narrow window: an empty title, never a negative one
        QCOMPARE(TitleBar::titleRect(100, true, TitleBar::Buttons()).width(), 0.0);
    }

    void doubleClick()
    {
        QVERIFY(TitleBar::isDoubleClick(1000, QPointF(100, 10), 1300, QPointF(102, 12), 400));
        QVERIFY(!TitleBar::isDoubleClick(1000, QPointF(100, 10), 1500, QPointF(100, 10), 400));
        QVERIFY(!TitleBar::isDoubleClick(1000, QPointF(100, 10), 1100, QPointF(120, 10), 400));
        QVERIFY(!TitleBar::isDoubleClick(-1, QPointF(100, 10), 1100, QPointF(100, 10), 400));
    }

    void colours()
    {
        const QColor night(0x14, 0x0f, 0x1f);
        QVERIFY(TitleBar::isDark(night));
        QVERIFY(!TitleBar::isDark(QColor(0xf3, 0xf3, 0xf3)));
        // red under Close with a white cross, a light veil under the others
        QCOMPARE(TitleBar::buttonBackground(Part::Close, true, false, night), QColor(0xE5, 0x33, 0x4F));
        QCOMPARE(TitleBar::glyphColor(Part::Close, true, false, QColor(0xf3, 0xea, 0xff)), QColor(Qt::white));
        const QColor veil = TitleBar::buttonBackground(Part::Minimize, true, false, night);
        QVERIFY(veil.isValid() && veil.lightness() > night.lightness());
        QVERIFY(!TitleBar::buttonBackground(Part::Minimize, false, false, night).isValid());
        QVERIFY(!TitleBar::buttonBackground(Part::Title, true, false, night).isValid());
        QCOMPARE(TitleBar::glyphColor(Part::Maximize, true, false, QColor(0xf3, 0xea, 0xff)), QColor(0xf3, 0xea, 0xff));
        // an inactive window's title fades
        QVERIFY(TitleBar::textColor(QColor(0xf3, 0xea, 0xff), false).alphaF() < 0.6);
        QCOMPARE(TitleBar::textColor(QColor(0xf3, 0xea, 0xff), true), QColor(0xf3, 0xea, 0xff));
    }
};

QTEST_GUILESS_MAIN(TestTitleBar)
#include "tst_titlebar.moc"
