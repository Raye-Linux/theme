// The Windows 11 easing curves for the shell's animations, as
// QEasingCurve::BezierSpline values QML can put straight on an animation:
//
//   NumberAnimation { duration: ShellTokens.durationNormal; easing: Motion.entrance }
//
// The durations live in ShellTokens (they follow the user's animation
// speed); the curves never change.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QEasingCurve>
#include <QObject>
#include <qqmlregistration.h>

class Motion : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // something arrives: cubic-bezier(0, 0, 0, 1), with a fade in
    Q_PROPERTY(QEasingCurve entrance READ entrance CONSTANT)
    // something leaves: cubic-bezier(1, 0, 1, 1), always with a fade out
    Q_PROPERTY(QEasingCurve exit READ exit CONSTANT)
    // something already shown moves: cubic-bezier(0.55, 0.55, 0, 1)
    Q_PROPERTY(QEasingCurve pointToPoint READ pointToPoint CONSTANT)

public:
    explicit Motion(QObject *parent = nullptr);

    static QEasingCurve bezier(qreal x1, qreal y1, qreal x2, qreal y2);
    static QEasingCurve entrance();
    static QEasingCurve exit();
    static QEasingCurve pointToPoint();
};
