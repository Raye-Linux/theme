// Answers for the stock display switcher on the session bus. The desktop
// calls it when a screen is plugged in; here that opens the Project panel.
// The service only lives long enough to pass the call on, and the bus
// starts it again the next time.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCoreApplication>
#include <QObject>

class SwitcherService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kscreen.osdService")

public:
    using QObject::QObject;

public Q_SLOTS:
    Q_SCRIPTABLE void showActionSelector();
    Q_SCRIPTABLE void hideOsd();
};

// Takes the switcher's name on the bus; returns the exit code.
int runSwitcherService(QCoreApplication &app);
