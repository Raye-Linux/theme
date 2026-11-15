// SPDX-License-Identifier: GPL-3.0-or-later

#include "devicesapplet.h"

#include <KConfigGroup>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KService>
#include <KSharedConfig>

#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/OpticalDrive>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

using namespace Qt::StringLiterals;
using namespace RayeDevices;

K_PLUGIN_CLASS_WITH_JSON(RayeDevicesApplet, "metadata.json")

static const QString s_notifyService = u"org.freedesktop.Notifications"_s;
static const QString s_notifyPath = u"/org/freedesktop/Notifications"_s;
static const QString s_udisks = u"org.freedesktop.UDisks2"_s;

// one applet answers plug-ins, even if a second tray has one too
static RayeDevicesApplet *s_greeter = nullptr;

RayeDevicesApplet::RayeDevicesApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Applet(parent, data, args)
{
}

void RayeDevicesApplet::init()
{
    Plasma::Applet::init();
    auto *notifier = Solid::DeviceNotifier::instance();
    connect(notifier, &Solid::DeviceNotifier::deviceAdded, this, &RayeDevicesApplet::added);
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved, this, &RayeDevicesApplet::refresh);
    if (!s_greeter) {
        s_greeter = this;
        connect(this, &QObject::destroyed, [] {
            s_greeter = nullptr;
        });
        QDBusConnection::sessionBus().connect(s_notifyService, s_notifyPath, s_notifyService, u"ActionInvoked"_s, this,
                                              SLOT(notificationAction(uint, QString)));
    }
    refresh();
    // the devices that are there at login were plugged in before: no toast
    for (const Device &d : std::as_const(m_devices))
        m_greeted.insert(d.udi);
}

static Kind kindOf(const Solid::StorageDrive *drive)
{
    switch (drive->driveType()) {
    case Solid::StorageDrive::CdromDrive:
        return Kind::Disc;
    case Solid::StorageDrive::MemoryStick:
    case Solid::StorageDrive::SdMmc:
    case Solid::StorageDrive::CompactFlash:
    case Solid::StorageDrive::SmartMedia:
    case Solid::StorageDrive::Xd:
        return Kind::MemoryCard;
    default:
        break;
    }
    return drive->bus() == Solid::StorageDrive::Usb ? Kind::UsbDrive : Kind::Drive;
}

void RayeDevicesApplet::refresh()
{
    QList<Device> devices;
    const auto volumes = Solid::Device::listFromType(Solid::DeviceInterface::StorageVolume);
    for (const Solid::Device &dev : volumes) {
        const auto *volume = dev.as<Solid::StorageVolume>();
        if (!volume || volume->isIgnored() || volume->usage() != Solid::StorageVolume::FileSystem)
            continue;
        Solid::Device parent = dev;
        while (parent.isValid() && !parent.is<Solid::StorageDrive>())
            parent = parent.parent();
        if (!parent.isValid())
            continue;
        const auto *drive = parent.as<Solid::StorageDrive>();
        if (!drive->isHotpluggable() && !drive->isRemovable())
            continue;
        const auto *access = dev.as<Solid::StorageAccess>();
        Volume v{dev.udi(), dev.description(), access && access->isAccessible(), access ? access->filePath() : QString()};
        auto it = std::find_if(devices.begin(), devices.end(), [&](const Device &d) {
            return d.udi == parent.udi();
        });
        if (it == devices.end()) {
            devices.append({parent.udi(), v.name, kindOf(drive), drive->size(), {}});
            it = devices.end() - 1;
        }
        it->volumes.append(v);
    }
    m_devices = devices;
    Q_EMIT devicesChanged();
}

const Device *RayeDevicesApplet::deviceOf(const QString &udi) const
{
    for (const Device &d : m_devices) {
        if (d.udi == udi)
            return &d;
        for (const Volume &v : d.volumes) {
            if (v.udi == udi)
                return &d;
        }
    }
    return nullptr;
}

void RayeDevicesApplet::added(const QString &udi)
{
    refresh();
    const Device *d = deviceOf(udi);
    if (!d || s_greeter != this || m_greeted.contains(d->udi))
        return;
    // a drive's volumes come one after the other: greet the drive once
    m_greeted.insert(d->udi);
    const Volume *first = d->volumes.isEmpty() ? nullptr : &d->volumes.first();
    if (!first)
        return;
    const KConfigGroup group = KSharedConfig::openConfig(u"rayeshellrc"_s)->group(u"AutoPlay"_s);
    if (!group.readEntry("Enabled", true))
        return;
    const QString drive = group.readEntry("RemovableDrive", QString());
    const QString choice = autoPlay(d->kind == Kind::MemoryCard ? group.readEntry("MemoryCard", drive) : drive);
    if (choice == u"nothing"_s)
        return;
    if (choice == u"open"_s) {
        open(first->udi);
        return;
    }
    notify(plugTitle(*d), plugBody(),
           {u"default"_s, QString(), u"open"_s, i18nc("@action:button notification", "Open folder to view files"),
            u"nothing"_s, i18nc("@action:button notification", "Take no action")},
           first->udi);
}

void RayeDevicesApplet::notificationAction(uint id, const QString &action)
{
    const QString udi = m_toasts.take(id);
    if (!udi.isEmpty() && (action == u"open"_s || action == u"default"_s))
        open(udi);
}

void RayeDevicesApplet::open(const QString &volumeUdi)
{
    Solid::Device dev(volumeUdi);
    auto *access = dev.as<Solid::StorageAccess>();
    if (!access)
        return;
    auto show = [](const QString &path) {
        auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(path), u"inode/directory"_s);
        job->start();
    };
    if (access->isAccessible()) {
        show(access->filePath());
        return;
    }
    connect(access, &Solid::StorageAccess::setupDone, this,
            [this, access, show](Solid::ErrorType error, const QVariant &, const QString &) {
                if (error == Solid::NoError)
                    show(access->filePath());
                refresh();
            }, Qt::SingleShotConnection);
    access->setup();
}

void RayeDevicesApplet::eject(const QString &driveUdi)
{
    const Device *d = deviceOf(driveUdi);
    if (!d || m_pending.contains(driveUdi))
        return;
    QSet<QString> waiting;
    for (const Volume &v : d->volumes) {
        if (v.mounted)
            waiting.insert(v.udi);
    }
    if (waiting.isEmpty()) {
        tornDown(driveUdi, true);
        return;
    }
    m_pending.insert(driveUdi, waiting);
    m_failed.insert(driveUdi, false);
    for (const QString &udi : std::as_const(waiting)) {
        Solid::Device dev(udi);
        auto *access = dev.as<Solid::StorageAccess>();
        if (!access) {
            m_pending[driveUdi].remove(udi);
            continue;
        }
        connect(access, &Solid::StorageAccess::teardownDone, this,
                [this, driveUdi, udi](Solid::ErrorType error, const QVariant &, const QString &) {
                    if (error != Solid::NoError)
                        m_failed[driveUdi] = true;
                    m_pending[driveUdi].remove(udi);
                    if (m_pending[driveUdi].isEmpty()) {
                        m_pending.remove(driveUdi);
                        tornDown(driveUdi, !m_failed.take(driveUdi));
                    }
                }, Qt::SingleShotConnection);
        access->teardown();
    }
}

// the command names of the user's own processes with files open on the
// device, as app names when an installed app has that name
QStringList RayeDevicesApplet::appsUsing(const Device &device) const
{
    QStringList mountPoints;
    for (const Volume &v : device.volumes) {
        if (!v.mountPoint.isEmpty())
            mountPoints << v.mountPoint;
    }
    if (mountPoints.isEmpty())
        return {};
    QList<Process> processes;
    const auto pids = QDir(u"/proc"_s).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &pid : pids) {
        bool number = false;
        const int n = pid.toInt(&number);
        if (!number || n == QCoreApplication::applicationPid())
            continue;
        const QString base = u"/proc/"_s + pid;
        Process p;
        p.pid = n;
        p.cwd = QFileInfo(base + u"/cwd"_s).symLinkTarget();
        const auto fds = QDir(base + u"/fd"_s).entryInfoList(QDir::Files | QDir::System | QDir::NoDotAndDotDot);
        for (const QFileInfo &fd : fds)
            p.files << fd.symLinkTarget();
        QFile comm(base + u"/comm"_s);
        if (comm.open(QIODevice::ReadOnly))
            p.command = QString::fromUtf8(comm.readAll()).trimmed();
        processes << p;
    }
    QStringList apps;
    const auto commands = processesUsing(processes, mountPoints);
    for (const QString &c : commands) {
        KService::Ptr s = KService::serviceByDesktopName(c);
        if (!s)
            s = KService::serviceByDesktopName(u"org.kde."_s + c);
        apps << (s ? s->name() : c);
    }
    return apps;
}

void RayeDevicesApplet::tornDown(const QString &driveUdi, bool ok)
{
    const Device *found = deviceOf(driveUdi);
    if (!found)
        return;
    const Device d = *found;
    if (!ok) {
        notify(busyTitle(d.kind), busyBody(appsUsing(d)), {});
        refresh();
        return;
    }
    powerOff(d);
}

void RayeDevicesApplet::powerOff(const Device &device)
{
    const QString name = device.name;
    auto done = [this, name] {
        notify(safeTitle(), safeBody(name), {});
        refresh();
    };
    Solid::Device dev(device.udi);
    if (auto *optical = dev.as<Solid::OpticalDrive>()) {
        connect(optical, &Solid::OpticalDrive::ejectDone, this, done, Qt::SingleShotConnection);
        optical->eject();
        return;
    }
    // udisks: the drive object behind the disk's block device; a drive
    // that cannot be powered off is still safe to unplug once unmounted
    QDBusMessage get = QDBusMessage::createMethodCall(s_udisks, device.udi, u"org.freedesktop.DBus.Properties"_s, u"Get"_s);
    get << u"org.freedesktop.UDisks2.Block"_s << u"Drive"_s;
    auto *watch = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(get), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, done](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusVariant> reply = *w;
        const QString drive = reply.isError() ? QString() : reply.value().variant().value<QDBusObjectPath>().path();
        if (drive.isEmpty() || drive == u"/"_s) {
            done();
            return;
        }
        QDBusMessage off = QDBusMessage::createMethodCall(s_udisks, drive, u"org.freedesktop.UDisks2.Drive"_s, u"PowerOff"_s);
        off << QVariantMap();
        auto *watchOff = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(off), this);
        connect(watchOff, &QDBusPendingCallWatcher::finished, this, [done](QDBusPendingCallWatcher *o) {
            o->deleteLater();
            done();
        });
    });
}

void RayeDevicesApplet::notify(const QString &title, const QString &body, const QStringList &actions, const QString &openUdi)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(s_notifyService, s_notifyPath, s_notifyService, u"Notify"_s);
    const QVariantMap hints{{u"desktop-entry"_s, u"org.raye.files"_s}};
    msg << i18nc("@title notification source", "Devices") << uint(0) << u"drive-removable-media"_s << title << body << actions
        << hints << int(-1);
    auto *watch = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watch, &QDBusPendingCallWatcher::finished, this, [this, openUdi](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<uint> reply = *w;
        if (!reply.isError() && !openUdi.isEmpty())
            m_toasts.insert(reply.value(), openUdi);
    });
}

#include "devicesapplet.moc"
