/*
 * SPDX-FileCopyrightText: 2026 techoraye
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Window frame rules of the theming spec (1.10, 3.3, 3.12): title bar 32,
// caption buttons 46x32, 8 px corners that go square against a screen
// edge, Windows' button colours, and the active/inactive shadows.

#include "rayedecorationmetrics.h"

#include <QTest>

using namespace Raye::DecorationMetrics;

class TestRayeDecorationMetrics : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sizes()
    {
        QCOMPARE(TitleBarHeight, 32);
        QCOMPARE(ButtonWidth, 46);
        QCOMPARE(ButtonHeight, 32);
        QCOMPARE(CornerRadius, 8);
        QCOMPARE(IconSize, 16);
        QCOMPARE(IconMargin, 12);
        QCOMPARE(GlyphSize, 10);
    }

    void cornersRoundUntilTheyTouchAnEdge()
    {
        const Corners free(cornersFor(8, false, Qt::Edges()));
        QCOMPARE(free.topLeft, 8.0);
        QCOMPARE(free.topRight, 8.0);
        QCOMPARE(free.bottomRight, 8.0);
        QCOMPARE(free.bottomLeft, 8.0);

        const Corners maximized(cornersFor(8, true, Qt::Edges()));
        QCOMPARE(maximized.topLeft, 0.0);
        QCOMPARE(maximized.bottomRight, 0.0);

        // snapped to the left half of the screen: the left side is square
        const Corners left(cornersFor(8, false, Qt::LeftEdge | Qt::TopEdge | Qt::BottomEdge));
        QCOMPARE(left.topLeft, 0.0);
        QCOMPARE(left.bottomLeft, 0.0);
        QCOMPARE(left.topRight, 0.0); // touches the top edge too
        QCOMPARE(left.bottomRight, 0.0);

        const Corners rightOnly(cornersFor(8, false, Qt::RightEdge));
        QCOMPARE(rightOnly.topLeft, 8.0);
        QCOMPARE(rightOnly.bottomLeft, 8.0);
        QCOMPARE(rightOnly.topRight, 0.0);
        QCOMPARE(rightOnly.bottomRight, 0.0);
    }

    void glyphIsCentredAndSnapped()
    {
        // 46x32 button at 100 %
        const QRectF glyph(glyphRect(QRectF(100, 0, 46, 32), 1.0));
        QCOMPARE(glyph.size(), QSizeF(10, 10));
        QCOMPARE(glyph.center().x(), 123.0);
        QCOMPARE(glyph.center().y(), 16.0);
        // at 150 % it stays 10 logical pixels, on the device pixel grid
        const QRectF big(glyphRect(QRectF(0, 0, 46, 32), 1.5));
        QCOMPARE(big.width(), 10.0);
        QCOMPARE(qRound(big.left() * 1.5) / 1.5, big.left());
    }

    void buttonColours()
    {
        const QColor title(0x13, 0x0E, 0x1F);
        // rest: nothing drawn
        QVERIFY(!buttonBackground(false, false, false, title, true).isValid());
        // hover: white at 6 % over the title bar on night
        const QColor hover(buttonBackground(false, true, false, title, true));
        QVERIFY(hover.isValid());
        QVERIFY(hover.lightness() > title.lightness());
        // pressed: weaker than hover
        const QColor pressed(buttonBackground(false, true, true, title, true));
        QVERIFY(pressed.lightness() < hover.lightness());
        // day: the dark ink darkens
        const QColor dayTitle(0xF6, 0xF3, 0xFB);
        QVERIFY(buttonBackground(false, true, false, dayTitle, false).lightness() < dayTitle.lightness());
        // close: red, darker red pressed, white glyph
        QCOMPARE(buttonBackground(true, true, false, title, true), QColor(0xE5, 0x33, 0x4F));
        QCOMPARE(buttonBackground(true, true, true, title, true), QColor(0xC2, 0x2A, 0x44));
        QCOMPARE(closeGlyphOnRed(), QColor(Qt::white));
    }

    void outline()
    {
        // stroke.surface: white at 9 % on night, the dark ink at 10 % on day
        QCOMPARE(outlineColor(true, QColor(0x1A, 0x10, 0x28), QColor(0x7B, 0x3C, 0xFF), false).alpha(), qRound(0.09 * 255));
        QCOMPARE(outlineColor(false, QColor(0x1A, 0x10, 0x28), QColor(0x7B, 0x3C, 0xFF), false).alpha(), qRound(0.10 * 255));
        // accent border: the accent at 70 %
        const QColor accent(outlineColor(true, QColor(0x1A, 0x10, 0x28), QColor(0x7B, 0x3C, 0xFF), true));
        QCOMPARE(accent.rgb(), QColor(0x7B, 0x3C, 0xFF).rgb());
        QCOMPARE(accent.alpha(), qRound(0.70 * 255));
    }

    void shadows()
    {
        // night active 56/18/50 %, inactive 28/8/30 %
        const Shadow active(shadowFor(true, 1.0, 1.0));
        QCOMPARE(active.radius, 56.0);
        QCOMPARE(active.offsetY, 18.0);
        QCOMPARE(active.opacity, 0.5);
        const Shadow inactive(shadowFor(true, 0.0, 1.0));
        QCOMPARE(inactive.radius, 28.0);
        QCOMPARE(inactive.offsetY, 8.0);
        QCOMPARE(inactive.opacity, 0.3);
        // day active 40/16/22 %, inactive 22/6/14 %
        QCOMPARE(shadowFor(false, 1.0, 1.0).radius, 40.0);
        QCOMPARE(shadowFor(false, 0.0, 1.0).opacity, 0.14);
        // halfway through the focus animation, halfway between the two
        QCOMPARE(shadowFor(true, 0.5, 1.0).radius, 42.0);
        // the strength setting scales opacity only
        QCOMPARE(shadowFor(true, 1.0, 0.5).opacity, 0.25);
        QCOMPARE(shadowFor(true, 1.0, 0.5).radius, 56.0);
    }

    void snapHoverDelay()
    {
        QCOMPARE(SnapLayoutsDelayMs, 500);
        QCOMPARE(QString::fromLatin1(SnapService), QStringLiteral("org.raye.Snap"));
        QCOMPARE(QString::fromLatin1(SnapPath), QStringLiteral("/Snap"));
        QCOMPARE(QString::fromLatin1(SnapMethod), QStringLiteral("showLayouts"));
    }
};

QTEST_GUILESS_MAIN(TestRayeDecorationMetrics)

#include "tst_rayedecorationmetrics.moc"
