// Where the Raye views the shell reuses are: the files of the installed
// taskbar, Start, quick settings and clock applets, and the wallpaper.
// RAYE_PLASMOIDS_DIR points at another folder of applets (the source tree
// in tests).
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QSize>
#include <QUrl>
#include <qqmlregistration.h>

class Views : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // contents/ui/<file> of an applet, empty when it is not installed
    Q_INVOKABLE QUrl url(const QString &applet, const QString &file) const;
    // the Raye wallpaper's picture closest to (and not smaller than) size
    Q_INVOKABLE QUrl wallpaper(const QSize &size, const QString &name = QStringLiteral("Raye")) const;
};
