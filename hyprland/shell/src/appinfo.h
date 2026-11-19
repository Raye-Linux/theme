// Which app a window belongs to: its desktop entry, found from the class
// Hyprland reports (the Wayland app id, or the X11 class).
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <qqmlregistration.h>

class AppInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit AppInfo(QObject *parent = nullptr);

    // { id: "org.kde.dolphin.desktop", name, icon }, empty when no entry fits
    Q_INVOKABLE QVariantMap forClass(const QString &windowClass, const QString &initialClass = QString());
    // name and icon of an entry by its desktop id
    Q_INVOKABLE QVariantMap forDesktopId(const QString &desktopId);
    // { address: entry } for hyprctl's client list
    Q_INVOKABLE QVariantMap forClients(const QVariantList &clients);

    // the desktop id for a class, without the cache (tests)
    static QString lookup(const QString &windowClass);

private:
    QHash<QString, QVariantMap> m_cache;
};
