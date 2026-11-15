// SPDX-License-Identifier: GPL-3.0-or-later

#include "motion.h"

#include <QPointF>

Motion::Motion(QObject *parent)
    : QObject(parent)
{
}

QEasingCurve Motion::bezier(qreal x1, qreal y1, qreal x2, qreal y2)
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(QPointF(x1, y1), QPointF(x2, y2), QPointF(1, 1));
    return curve;
}

QEasingCurve Motion::entrance()
{
    return bezier(0, 0, 0, 1);
}

QEasingCurve Motion::exit()
{
    return bezier(1, 0, 1, 1);
}

QEasingCurve Motion::pointToPoint()
{
    return bezier(0.55, 0.55, 0, 1);
}
