/*
 * SPDX-FileCopyrightText: 2026 techoraye
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

// Sizes and colours of the Raye window frame (theming spec 1.10, 3.3,
// 3.12), as plain functions the decoration and its tests share. All sizes
// are logical pixels at 100 %; KWin scales them.

#include <QColor>
#include <QRectF>

namespace Raye
{
namespace DecorationMetrics
{
constexpr int TitleBarHeight = 32;
constexpr int ButtonWidth = 46;
constexpr int ButtonHeight = 32;
constexpr int CornerRadius = 8;
constexpr int IconSize = 16;
//* the icon sits 12 px from the left edge, the title 12 px after it
constexpr int IconMargin = 12;
constexpr int GlyphSize = 10;
//* hovering Maximize this long opens Snap layouts
constexpr int SnapLayoutsDelayMs = 500;

//* the Snap layouts service of the shell, called if it runs
constexpr const char *SnapService = "org.raye.Snap";
constexpr const char *SnapPath = "/Snap";
constexpr const char *SnapInterface = "org.raye.Snap";
constexpr const char *SnapMethod = "showLayouts";

struct Corners {
    qreal topLeft = 0;
    qreal topRight = 0;
    qreal bottomRight = 0;
    qreal bottomLeft = 0;
};

//* corner radii: square on any side that touches a screen edge, and when maximized
Corners cornersFor(qreal radius, bool maximized, Qt::Edges adjacentScreenEdges);

//* the 10 px glyph area centred in a caption button, on the device pixel grid
QRectF glyphRect(const QRectF &buttonRect, qreal scale);

//* backplate of a caption button; invalid when nothing is drawn
QColor buttonBackground(bool close, bool hovered, bool pressed, const QColor &titleBar, bool dark);

//* glyph colour on the red close button
QColor closeGlyphOnRed();

//* 1 px window outline: stroke.surface, or the accent at 70 % with the accent border on
QColor outlineColor(bool dark, const QColor &text, const QColor &accent, bool accentBorder);

struct Shadow {
    qreal radius = 0;
    qreal offsetY = 0;
    qreal opacity = 0;
};

//* window shadow, from inactive (0) to active (1); strength 0..1 scales the opacity
Shadow shadowFor(bool dark, qreal activeness, qreal strength);
}
}
