// The USB icon of the tray: the removable drives Solid sees, ejecting one
// (unmount every volume, then power the drive off through udisks), and
// the toasts Windows shows for both and for a drive plugged in.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devicelogic.h"

#include <Plasma/Applet>

#include <QHash>
#include <QSet>

class RayeDevicesApplet : public Plasma::Applet
{
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY devicesChanged)
    Q_PROPERTY(bool hasDevices READ hasDevices NOTIFY devicesChanged)

public:
    RayeDevicesApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args);

    void init() override;

    QVariantList rows() const { return RayeDevices::menuRows(m_devices); }
    bool hasDevices() const { return !m_devices.isEmpty(); }

    // unmount every volume of a drive, then power it off
    Q_INVOKABLE void eject(const QString &driveUdi);
    // mount a volume if needed, then open it in Files
    Q_INVOKABLE void open(const QString &volumeUdi);

Q_SIGNALS:
    void devicesChanged();

private Q_SLOTS:
    void notificationAction(uint id, const QString &action);

private:
    void refresh();
    void added(const QString &udi);
    void tornDown(const QString &driveUdi, bool ok);
    void powerOff(const RayeDevices::Device &device);
    const RayeDevices::Device *deviceOf(const QString &udi) const;
    void notify(const QString &title, const QString &body, const QStringList &actions, const QString &openUdi = QString());
    QStringList appsUsing(const RayeDevices::Device &device) const;

    QList<RayeDevices::Device> m_devices;
    // volumes still being unmounted, by drive
    QHash<QString, QSet<QString>> m_pending;
    QHash<QString, bool> m_failed;
    // drives a plug-in toast was shown for (once each)
    QSet<QString> m_greeted;
    // plug-in toasts waiting for a click: notification id -> volume
    QHash<uint, QString> m_toasts;
};
