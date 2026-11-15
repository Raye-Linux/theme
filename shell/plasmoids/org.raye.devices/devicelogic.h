// What the USB icon of the taskbar shows and says, kept apart from Solid
// and D-Bus so it can be tested: one row per removable device ("Eject
// iodd", its size and kind), "Open in Files" when it is mounted, and the
// words of the toasts.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace RayeDevices
{

enum class Kind {
    UsbDrive,
    MemoryCard,
    Disc,
    Drive, // any other removable drive
};

struct Volume {
    QString udi;
    QString name; // what the system calls it: its label, or its size
    bool mounted = false;
    QString mountPoint;
};

struct Device {
    QString udi; // the drive
    QString name; // the first volume's name, else the drive's
    Kind kind = Kind::Drive;
    qulonglong size = 0;
    QList<Volume> volumes;
};

// "7.5 GB USB drive"
QString detail(const Device &device);

// The menu, top to bottom: [{ kind: "eject" | "open" | "separator", udi,
// text, detail }]. "eject" acts on the device, "open" on a volume.
QVariantList menuRows(const QList<Device> &devices);

// The processes keeping files open under a mount point: from the
// /proc/<pid> entries given as (pid, cwd, open file paths, command name),
// the command names of those inside it, each once, sorted.
struct Process {
    int pid = 0;
    QString cwd;
    QStringList files;
    QString command;
};
QStringList processesUsing(const QList<Process> &processes, const QStringList &mountPoints);

// The toasts
QString safeTitle();
QString safeBody(const QString &name);
QString busyTitle(Kind kind);
QString busyBody(const QStringList &apps);
QString plugTitle(const Device &device);
QString plugBody();

// rayeshellrc [AutoPlay] RemovableDrive: "ask" (the toast), "open"
// (open the files at once), "nothing". Anything else is "ask".
QString autoPlay(const QString &value);

} // namespace RayeDevices
