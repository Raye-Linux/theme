// The stock notifications item of a system tray, next to the Raye clock:
// it would draw its own pop-ups next to Raye's, and in a tray it cannot
// place them, so they land in the top left corner. The clock takes it out
// as soon as the tray is there, the way raye-taskbar-sync does later
// (sync.js, dropTrayBell), and with the same mark so going back to the
// stock clock puts it back.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace RayeTrayBell
{

inline const QString &id()
{
    static const QString s = QStringLiteral("org.kde.plasma.notifications");
    return s;
}

struct Lists {
    QStringList extra; // extraItems: the tray's own items that are on
    QStringList known; // knownItems: the ones it has seen once
    QStringList hidden;
    QStringList shown;
};

// Takes the item out of the lists. Returns true when the tray had it or
// was about to turn it on (never seen yet), so the caller marks it as
// taken; false when it was already off, by choice or by an earlier run.
inline bool take(Lists &l)
{
    l.hidden.removeAll(id());
    l.shown.removeAll(id());
    const bool on = l.extra.removeAll(id()) > 0;
    if (!on && l.known.contains(id()))
        return false;
    // a tray adds an item it has never seen: known, it stays off
    if (!l.known.contains(id()))
        l.known.append(id());
    return true;
}

} // namespace RayeTrayBell
