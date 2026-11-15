// Where the panel goes, and which keys paste into the window it opened over.
#pragma once

#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

namespace Placement
{
// Gap between the panel and the screen edges or the taskbar.
constexpr int Gap = 12;

// The top-left corner of the panel for a pointer position: below and a
// little left of the pointer, above it when there is no room below, and
// always on the screen and clear of the taskbar.
QPoint near(const QRect &screen, const QPoint &pointer, const QSize &panel, int taskbarHeight);

// Bottom right of the screen, above the taskbar (no pointer position).
QPoint corner(const QRect &screen, const QSize &panel, int taskbarHeight);
}

namespace PasteKeys
{
// Linux input key codes (linux/input-event-codes.h).
constexpr quint32 LeftCtrl = 29;
constexpr quint32 LeftShift = 42;
constexpr quint32 V = 47;

// Terminals paste with Ctrl+Shift+V, everything else with Ctrl+V.
bool isTerminal(const QString &windowClass);
// The keys to press in order (and release in reverse) for that window.
QList<quint32> forWindow(const QString &windowClass);
}
