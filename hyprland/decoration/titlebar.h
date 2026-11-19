// The Raye title bar of Qt apps on Hyprland: where the parts sit, what a
// point hits and the colours, kept apart from the Wayland plugin so the
// tests can check them without a compositor.
//
// Same sizes as the KWin decoration (style/kdecoration): a 32 px bar,
// 46 x 32 caption buttons on the right, the 16 px icon 12 px from the
// left and the 12 px title after it.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>

namespace Raye
{
namespace TitleBar
{
enum class Part { None, Title, Minimize, Maximize, Close };

struct Buttons {
    bool minimize = true;
    bool maximize = true;
};

int height();

// dialogs get Close only; a window of fixed size cannot be maximized
Buttons buttonsFor(bool dialog, bool fixedSize);

// an empty rect for a button the window does not have
QRectF buttonRect(Part part, qreal width, Buttons buttons);
QRectF iconRect();
QRectF titleRect(qreal width, bool hasIcon, Buttons buttons);

Part hitTest(const QPointF &point, qreal width, Buttons buttons);

// the second press of a double click on the bar: close in time and place
bool isDoubleClick(qint64 firstMs, const QPointF &first, qint64 secondMs, const QPointF &second, int intervalMs);

bool isDark(const QColor &bar);
// the title and glyphs: the text colour, faded on an inactive window
QColor textColor(const QColor &windowText, bool active);
// the backplate of a button, invalid when nothing is drawn
QColor buttonBackground(Part part, bool hovered, bool pressed, const QColor &bar);
QColor glyphColor(Part part, bool hovered, bool pressed, const QColor &text);
}
}
