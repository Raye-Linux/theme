// Where the Raye views the shell reuses are: the files of the installed
// taskbar, Start, quick settings and clock applets, and the wallpaper.
// RAYE_PLASMOIDS_DIR points at another folder of applets (the source tree
// in tests).
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QObject>
#include <QSize>
#include <QUrl>
#include <QVariantMap>
#include <qqmlregistration.h>

class Views : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // changes when the Plasma desktop's wallpaper settings are written
    Q_PROPERTY(int backgroundRevision READ backgroundRevision NOTIFY backgroundChanged)

public:
    explicit Views(QObject *parent = nullptr);

    // contents/ui/<file> of an applet, empty when it is not installed
    Q_INVOKABLE QUrl url(const QString &applet, const QString &file) const;
    // the Raye wallpaper's picture closest to (and not smaller than) size
    Q_INVOKABLE QUrl wallpaper(const QSize &size, const QString &name = QStringLiteral("Raye")) const;
    // what the user picked for the Plasma desktop of this screen:
    // { source: url } for a picture, { color } for a plain colour, empty
    // when there is nothing to follow (no Plasma settings, another kind
    // of wallpaper, a picture that is gone)
    Q_INVOKABLE QVariantMap background(int screen, const QSize &size) const;

    int backgroundRevision() const { return m_revision; }

    static QVariantMap readBackground(const QString &appletsrc, int screen, const QSize &size);
    // in a wallpaper package's contents/images, or a folder of WIDTHxHEIGHT pictures
    static QString pickPicture(const QString &dir, const QSize &size);

Q_SIGNALS:
    void backgroundChanged();

private:
    QString appletsrc() const;

    QFileSystemWatcher m_watcher;
    QDateTime m_lastChange;
    int m_revision = 0;
};
