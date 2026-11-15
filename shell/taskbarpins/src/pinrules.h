// What the pinned apps of the taskbars become when one of them changes:
// one list for all taskbars, like Windows. The plugin (taskbarpins.cpp)
// applies it to Plasma; the tests call it directly.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace RayePins
{

// ~/.config/raye-taskbarrc [Taskbar] MultiDisplayButtons
enum class Buttons {
    All, // every taskbar shows the pins
    MainAndWindow, // only the main taskbar has pins
    WindowOnly, // same for the pins
};

Buttons buttonsFrom(const QString &value);

struct Bar {
    QString id; // the task manager applet
    bool main = false; // on the main taskbar, else on a copy (raye-taskbar-sync)
    QStringList launchers;
};

// `changed` was just given new launchers (already in `bars`). Returns the
// launchers to write, by applet id, for the bars that must follow.
QHash<QString, QStringList> afterChange(const QList<Bar> &bars, const QString &changed, Buttons buttons);

} // namespace RayePins
