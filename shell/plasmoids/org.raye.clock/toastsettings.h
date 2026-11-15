// Two small decisions of the toasts that need no Plasma to be tested: which
// clock draws them, and what "turn off notifications from this app" writes.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>
#include <KSharedConfig>
#include <QList>
#include <QString>

namespace RayeToasts
{

// The clock on the lowest screen (0 is the main one); a clock whose panel
// waits for its screen only if nothing else is left. -1: there is no clock.
inline int ownerOf(const QList<int> &screens)
{
    int best = -1;
    for (int i = 0; i < screens.size(); ++i) {
        if (best < 0) {
            best = i;
            continue;
        }
        const int mine = screens.at(i);
        const int theirs = screens.at(best);
        if (mine >= 0 && (theirs < 0 || mine < theirs))
            best = i;
    }
    return best;
}

// The same keys Raye Settings > Notifications reads and writes, so the app
// shows as off there and can be turned back on. An app goes by its desktop
// entry; a service with none by its notifyrc name.
inline void turnOff(const KSharedConfigPtr &config, const QString &desktopEntry, const QString &notifyRcName)
{
    if (desktopEntry.isEmpty() && notifyRcName.isEmpty())
        return;
    KConfigGroup all = config->group(desktopEntry.isEmpty() ? QStringLiteral("Services") : QStringLiteral("Applications"));
    KConfigGroup app = all.group(desktopEntry.isEmpty() ? notifyRcName : desktopEntry);
    app.writeEntry("ShowPopups", false, KConfig::Notify);
    app.writeEntry("ShowInHistory", false, KConfig::Notify);
    config->sync();
}

}
