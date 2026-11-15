// The four choices of the Project panel, worked out on plain values: which
// screen is "the PC screen", which choice the screens are in right now, and
// what each screen has to become for another one. No window and no display
// server, so all of it can be tested.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QPoint>
#include <QSize>
#include <QString>

namespace DisplayPlan
{
enum class Mode {
    Unknown,
    PcOnly,
    Duplicate,
    Extend,
    SecondOnly,
};

struct Screen {
    QString name;
    bool embedded = false;
    bool enabled = false;
    QPoint pos;
    // the size in use, and every size the screen can do, biggest first
    QSize size;
    QSize preferred;
    QList<QSize> sizes;
    double scale = 1.0;
    bool sideways = false;
};

struct Target {
    QString name;
    bool enabled = false;
    QPoint pos;
    // what to set; empty: leave the screen's mode alone
    QSize size;
    // 1 is the main screen, 0 for a screen that is off
    int priority = 0;
};

// The screen that stays when you pick "PC screen only". The built-in panel
// on a laptop. On a desktop, the first by name, never "the main one right
// now": after "Second screen only" that would turn into the other screen,
// and "PC screen only" would keep the wrong one.
int mainScreen(const QList<Screen> &screens);

Mode current(const QList<Screen> &screens);

// With one screen there is nothing to choose.
bool canChoose(const QList<Screen> &screens);

// What every screen has to become. Empty when nothing has to change.
QList<Target> plan(Mode mode, const QList<Screen> &screens);

QString modeName(Mode mode);
Mode modeFromName(const QString &name);
}
