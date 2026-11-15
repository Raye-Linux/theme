// SPDX-License-Identifier: GPL-3.0-or-later

#include "devicelogic.h"

#include <KFormat>
#include <KLocalizedString>

#include <QVariantMap>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace RayeDevices
{

static QString kindName(Kind kind)
{
    switch (kind) {
    case Kind::UsbDrive:
        return i18nc("@info a kind of device", "USB drive");
    case Kind::MemoryCard:
        return i18nc("@info a kind of device", "Memory card");
    case Kind::Disc:
        return i18nc("@info a kind of device", "Disc");
    case Kind::Drive:
        break;
    }
    return i18nc("@info a kind of device", "Removable drive");
}

QString detail(const Device &device)
{
    if (device.size == 0)
        return kindName(device.kind);
    const QString size = KFormat().formatByteSize(double(device.size), 1, KFormat::MetricBinaryDialect);
    return i18nc("@info size and kind of a device: 7.5 GB USB drive", "%1 %2", size, kindName(device.kind));
}

QVariantList menuRows(const QList<Device> &devices)
{
    QVariantList rows;
    for (const Device &d : devices) {
        if (!rows.isEmpty())
            rows << QVariantMap{{u"kind"_s, u"separator"_s}};
        rows << QVariantMap{{u"kind"_s, u"eject"_s},
                            {u"udi"_s, d.udi},
                            {u"text"_s, i18nc("@action:inmenu", "Eject %1", d.name)},
                            {u"detail"_s, detail(d)}};
        const int mounted = int(std::count_if(d.volumes.cbegin(), d.volumes.cend(), [](const Volume &v) {
            return v.mounted;
        }));
        for (const Volume &v : d.volumes) {
            if (!v.mounted)
                continue;
            // one volume: "Open in Files"; several: say which
            const QString text = mounted > 1 ? i18nc("@action:inmenu", "Open %1 in Files", v.name)
                                             : i18nc("@action:inmenu", "Open in Files");
            rows << QVariantMap{{u"kind"_s, u"open"_s}, {u"udi"_s, v.udi}, {u"text"_s, text}, {u"detail"_s, QString()}};
        }
    }
    return rows;
}

static bool inside(const QString &path, const QString &mountPoint)
{
    if (mountPoint.isEmpty() || path.isEmpty())
        return false;
    const QString root = mountPoint.endsWith(u'/') ? mountPoint : mountPoint + u'/';
    return path == mountPoint || path.startsWith(root);
}

QStringList processesUsing(const QList<Process> &processes, const QStringList &mountPoints)
{
    QStringList out;
    for (const Process &p : processes) {
        bool uses = false;
        for (const QString &m : mountPoints) {
            if (inside(p.cwd, m) || std::any_of(p.files.cbegin(), p.files.cend(), [&](const QString &f) {
                    return inside(f, m);
                })) {
                uses = true;
                break;
            }
        }
        if (uses && !p.command.isEmpty() && !out.contains(p.command))
            out << p.command;
    }
    std::sort(out.begin(), out.end());
    return out;
}

QString safeTitle()
{
    return i18nc("@title notification", "Safe to remove hardware");
}

QString safeBody(const QString &name)
{
    return i18nc("@info notification", "The '%1' device can now be safely removed from the computer.", name);
}

QString busyTitle(Kind kind)
{
    switch (kind) {
    case Kind::UsbDrive:
        return i18nc("@title notification", "Problem ejecting USB device");
    case Kind::MemoryCard:
        return i18nc("@title notification", "Problem ejecting memory card");
    case Kind::Disc:
        return i18nc("@title notification", "Problem ejecting disc");
    case Kind::Drive:
        break;
    }
    return i18nc("@title notification", "Problem ejecting device");
}

QString busyBody(const QStringList &apps)
{
    QString body = i18nc("@info notification",
                         "This device is currently in use. Close any programs or windows that might be using the device, and then try again.");
    if (!apps.isEmpty())
        body += u"\n"_s + i18nc("@info notification: the apps keeping a device busy", "In use by: %1", apps.join(u", "_s));
    return body;
}

QString plugTitle(const Device &device)
{
    return i18nc("@title notification: a USB drive was plugged in", "%1 (%2)", kindName(device.kind), device.name);
}

QString plugBody()
{
    return i18nc("@info notification", "Select to choose what happens with removable drives.");
}

QString autoPlay(const QString &value)
{
    if (value == u"open"_s || value == u"nothing"_s)
        return value;
    return u"ask"_s;
}

} // namespace RayeDevices
