// SPDX-License-Identifier: GPL-3.0-or-later

#include "titlebar.h"

#include "rayedecorationmetrics.h"

#include <QtMath>

namespace Raye
{
namespace TitleBar
{
using namespace DecorationMetrics;

int height()
{
    return TitleBarHeight;
}

Buttons buttonsFor(bool dialog, bool fixedSize)
{
    Buttons b;
    b.minimize = !dialog;
    b.maximize = !dialog && !fixedSize;
    return b;
}

QRectF buttonRect(Part part, qreal width, Buttons buttons)
{
    int slot = -1;
    switch (part) {
    case Part::Close:
        slot = 0;
        break;
    case Part::Maximize:
        slot = buttons.maximize ? 1 : -1;
        break;
    case Part::Minimize:
        if (buttons.minimize)
            slot = buttons.maximize ? 2 : 1;
        break;
    default:
        break;
    }
    if (slot < 0)
        return QRectF();
    return QRectF(width - (slot + 1) * ButtonWidth, 0, ButtonWidth, ButtonHeight);
}

QRectF iconRect()
{
    return QRectF(IconMargin, (TitleBarHeight - IconSize) / 2.0, IconSize, IconSize);
}

QRectF titleRect(qreal width, bool hasIcon, Buttons buttons)
{
    const qreal left = hasIcon ? IconMargin + IconSize + IconMargin : IconMargin;
    int count = 1 + (buttons.maximize ? 1 : 0) + (buttons.minimize ? 1 : 0);
    const qreal right = width - count * ButtonWidth - 8;
    return QRectF(left, 0, qMax<qreal>(0, right - left), TitleBarHeight);
}

Part hitTest(const QPointF &point, qreal width, Buttons buttons)
{
    if (point.y() < 0 || point.y() >= TitleBarHeight || point.x() < 0 || point.x() >= width)
        return Part::None;
    for (Part part : {Part::Close, Part::Maximize, Part::Minimize}) {
        const QRectF r = buttonRect(part, width, buttons);
        if (r.isValid() && r.contains(point))
            return part;
    }
    return Part::Title;
}

bool isDoubleClick(qint64 firstMs, const QPointF &first, qint64 secondMs, const QPointF &second, int intervalMs)
{
    if (firstMs < 0 || secondMs < firstMs || secondMs - firstMs > intervalMs)
        return false;
    return qAbs(second.x() - first.x()) <= 4 && qAbs(second.y() - first.y()) <= 4;
}

bool isDark(const QColor &bar)
{
    return qGray(bar.rgb()) < 128;
}

QColor textColor(const QColor &windowText, bool active)
{
    QColor c = windowText;
    if (!active)
        c.setAlphaF(c.alphaF() * 0.55);
    return c;
}

QColor buttonBackground(Part part, bool hovered, bool pressed, const QColor &bar)
{
    if (part != Part::Close && part != Part::Maximize && part != Part::Minimize)
        return QColor();
    return DecorationMetrics::buttonBackground(part == Part::Close, hovered, pressed, bar, isDark(bar));
}

QColor glyphColor(Part part, bool hovered, bool pressed, const QColor &text)
{
    if (part == Part::Close && (hovered || pressed))
        return closeGlyphOnRed();
    return text;
}
}
}
