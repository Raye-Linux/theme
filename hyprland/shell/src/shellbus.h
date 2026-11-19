// org.raye.HyprShell on the session bus: what the keys bound in
// hyprland.conf ask the running shell for (raye-hypr-shell --toggle start).
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QObject>
#include <qqmlregistration.h>

class ShellBus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.raye.HyprShell")
    QML_ELEMENT
    QML_SINGLETON

public:
    static constexpr const char *service = "org.raye.HyprShell";
    static constexpr const char *path = "/Shell";

    // made by the shell's QML the first time it is used: only then is
    // anything there to answer the keys
    explicit ShellBus(QObject *parent = nullptr)
        : QObject(parent)
    {
        QDBusConnection session = QDBusConnection::sessionBus();
        session.registerObject(QString::fromLatin1(path), this, QDBusConnection::ExportAllSlots);
        if (!session.registerService(QString::fromLatin1(service)))
            qWarning("raye-hypr-shell: org.raye.HyprShell is taken");
    }

public Q_SLOTS:
    void Toggle(const QString &what) { Q_EMIT toggle(what); }
    void Snap(const QString &direction) { Q_EMIT snap(direction); }
    void Desktop() { Q_EMIT desktop(); }
    void Task(int number) { Q_EMIT task(number); }
    void CloseDesktop() { Q_EMIT closeDesktop(); }

Q_SIGNALS:
    void toggle(const QString &what);
    void snap(const QString &direction);
    void desktop();
    void task(int number);
    void closeDesktop();
};
