// The screen in use: the one KWin calls its active output (where the
// pointer is), asked each time, so a window shown for a moment (the
// on-screen display) opens there and not on the screen it opened on last.
//
//   import org.raye.shell
//   const area = ActiveOutput.geometry()     // a rect in desktop coordinates
//
// Without KWin (a test, another compositor) it is the primary screen.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QRect>
#include <QScreen>
#include <QStringList>
#include <qqmlregistration.h>


namespace ActiveOutputLogic
{
// the index of the screen called `active` in `names`, else `fallback`
int pick(const QStringList &names, const QString &active, int fallback);
}

class ActiveOutput : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit ActiveOutput(QObject *parent = nullptr);

    Q_INVOKABLE QScreen *screen() const;
    Q_INVOKABLE QRect geometry() const;

    // KWin's answer, "" when there is none (a quick call: a moment at most)
    static QString kwinActiveOutput();
};
