// org.raye.taskbarpins: loaded with every panel (an [ActionPlugins][1]
// entry no mouse button triggers), it does two things inside Plasma:
//
// - one list of pinned apps for all taskbars: pinning, unpinning or moving
//   an app on any taskbar shows on the others at once (pinrules.h);
// - settings written from outside (raye-taskbar-sync, Raye Settings, a
//   desktop script) reach the applets at once. Plasma's scripting writes
//   an applet's settings but does not tell its QML, which then only sees
//   them at the next start of Plasma;
// - the menu of an app button speaks of the taskbar ("Pin to taskbar",
//   "Unpin from taskbar") and has no Plasma settings entries: the
//   taskbar is set up in Raye Settings (its own right-click menu, from
//   org.raye.desktopmenu, has "Taskbar settings").
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Plasma/ContainmentActions>

#include <QPointer>

namespace Plasma
{
class Applet;
class Containment;
}

class TaskbarPins : public Plasma::ContainmentActions
{
    Q_OBJECT

public:
    TaskbarPins(QObject *parent, const QVariantList &args);

    void restore(const KConfigGroup &config) override;
    QList<QAction *> contextualActions() override;

private:
    void watch(Plasma::Applet *applet);
    void changed(Plasma::Applet *applet);
    void share(Plasma::Applet *source);
    void hideSettingsEntries(Plasma::Applet *applet);

    QPointer<Plasma::Containment> m_watched;
};
