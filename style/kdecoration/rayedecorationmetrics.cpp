/*
 * SPDX-FileCopyrightText: 2026 techoraye
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rayedecorationmetrics.h"

#include <QtMath>

namespace Raye
{
namespace DecorationMetrics
{
namespace
{
qreal lerp(qreal from, qreal to, qreal t)
{
    return from + (to - from) * t;
}

qreal snap(qreal value, qreal scale)
{
    return scale > 0 ? qRound(value * scale) / scale : value;
}

QColor over(const QColor &base, const QColor &top, qreal alpha)
{
    return QColor::fromRgbF(base.redF() + (top.redF() - base.redF()) * alpha,
                            base.greenF() + (top.greenF() - base.greenF()) * alpha,
                            base.blueF() + (top.blueF() - base.blueF()) * alpha)
        .toRgb();
}

const QColor closeRed(0xE5, 0x33, 0x4F);
const QColor closeRedPressed(0xC2, 0x2A, 0x44);
const QColor dayInk(0x1A, 0x10, 0x28);
}

Corners cornersFor(qreal radius, bool maximized, Qt::Edges edges)
{
    Corners corners;
    if (maximized) {
        return corners;
    }
    const bool left(edges & Qt::LeftEdge);
    const bool right(edges & Qt::RightEdge);
    const bool top(edges & Qt::TopEdge);
    const bool bottom(edges & Qt::BottomEdge);
    corners.topLeft = (left || top) ? 0 : radius;
    corners.topRight = (right || top) ? 0 : radius;
    corners.bottomRight = (right || bottom) ? 0 : radius;
    corners.bottomLeft = (left || bottom) ? 0 : radius;
    return corners;
}

QRectF glyphRect(const QRectF &buttonRect, qreal scale)
{
    const QPointF centre(buttonRect.center());
    const qreal half(GlyphSize / 2.0);
    return QRectF(snap(centre.x() - half, scale), snap(centre.y() - half, scale), GlyphSize, GlyphSize);
}

QColor buttonBackground(bool close, bool hovered, bool pressed, const QColor &titleBar, bool dark)
{
    if (!hovered && !pressed) {
        return QColor();
    }
    if (close) {
        return pressed ? closeRedPressed : closeRed;
    }
    // fill.subtle.hover / pressed: white at 6 / 4 % on night, the dark ink at 4 / 2.5 % on day
    const QColor ink(dark ? QColor(Qt::white) : dayInk);
    const qreal alpha(pressed ? (dark ? 0.04 : 0.025) : (dark ? 0.06 : 0.04));
    return over(titleBar, ink, alpha);
}

QColor closeGlyphOnRed()
{
    return Qt::white;
}

QColor outlineColor(bool dark, const QColor &text, const QColor &accent, bool accentBorder)
{
    QColor color;
    if (accentBorder) {
        color = accent;
        color.setAlpha(qRound(0.70 * 255));
        return color;
    }
    color = dark ? QColor(Qt::white) : text;
    color.setAlpha(qRound((dark ? 0.09 : 0.10) * 255));
    return color;
}

Shadow shadowFor(bool dark, qreal activeness, qreal strength)
{
    activeness = qBound(0.0, activeness, 1.0);
    strength = qBound(0.0, strength, 1.0);
    // blur radius / y offset / opacity of the shadow colour (theming spec 3.3)
    const Shadow active = dark ? Shadow{56, 18, 0.50} : Shadow{40, 16, 0.22};
    const Shadow inactive = dark ? Shadow{28, 8, 0.30} : Shadow{22, 6, 0.14};
    Shadow shadow;
    shadow.radius = lerp(inactive.radius, active.radius, activeness);
    shadow.offsetY = lerp(inactive.offsetY, active.offsetY, activeness);
    shadow.opacity = lerp(inactive.opacity, active.opacity, activeness) * strength;
    return shadow;
}
}
}
