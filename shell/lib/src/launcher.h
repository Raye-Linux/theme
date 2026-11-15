// Starting things from the shell: an app by its desktop id, a URL, a page
// of Raye Settings, or a global shortcut of another component (the way a
// button can do what a key does). Every call returns at once.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <qqmlregistration.h>

class Launcher : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Launcher(QObject *parent = nullptr);

    // starts an installed app ("org.kde.kate.desktop" or "org.kde.kate");
    // false when there is no such app
    Q_INVOKABLE bool launch(const QString &desktopId);
    // opens a file, folder or web address with the app that handles it
    Q_INVOKABLE bool openUrl(const QUrl &url);
    // opens Raye Settings on a page ("system/display"; empty: the home page)
    Q_INVOKABLE bool openSettings(const QString &page = QString());
    // runs a global shortcut action, e.g. ("kwin", "Overview")
    Q_INVOKABLE bool invokeShortcut(const QString &component, const QString &name);
    // "End task": stops one of the user's own processes at once (SIGKILL);
    // false for any other process
    Q_INVOKABLE bool endProcess(qint64 pid);

    // a process the user may end: his own, never the session's first ones
    static bool mayEnd(qint64 pid, uint ownerUid, uint myUid);

    // the checks and the command, shared with the tests
    static bool isPageId(const QString &page);
    static bool isComponentName(const QString &component);
    static QStringList settingsArguments(const QString &page);
    // A layer-shell process (the Ctrl+Alt+Del screen, ksmserver's greeter)
    // has QT_WAYLAND_SHELL_INTEGRATION=layer-shell in its environment, and
    // an app it starts would inherit it: no title bar, no taskbar button,
    // no Alt+F4. Called before anything is started.
    static void forgetShellIntegration();

Q_SIGNALS:
    // something did not start; what: the desktop id, URL, page or action
    void failed(const QString &what);
};
