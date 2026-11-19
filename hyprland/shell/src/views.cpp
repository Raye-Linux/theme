// SPDX-License-Identifier: GPL-3.0-or-later

#include "views.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

QUrl Views::url(const QString &applet, const QString &file) const
{
    const QString relative = applet + QStringLiteral("/contents/ui/") + file;
    const QString forced = qEnvironmentVariable("RAYE_PLASMOIDS_DIR");
    QString path;
    if (!forced.isEmpty()) {
        path = forced + QLatin1Char('/') + relative;
        if (!QFileInfo::exists(path))
            path.clear();
    } else {
        path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("plasma/plasmoids/") + relative);
    }
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

QUrl Views::wallpaper(const QSize &size, const QString &name) const
{
    const QString dir = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                               QStringLiteral("wallpapers/") + name + QStringLiteral("/contents/images"),
                                               QStandardPaths::LocateDirectory);
    if (dir.isEmpty())
        return {};
    // the pictures are named WIDTHxHEIGHT.png
    QString best;
    int bestWidth = 0;
    const QStringList files = QDir(dir).entryList({QStringLiteral("*.png"), QStringLiteral("*.jpg")}, QDir::Files);
    for (const QString &f : files) {
        const int w = f.section(QLatin1Char('x'), 0, 0).toInt();
        const bool fits = w >= size.width();
        const bool bestFits = bestWidth >= size.width();
        if (best.isEmpty() || (fits && (!bestFits || w < bestWidth)) || (!fits && !bestFits && w > bestWidth)) {
            best = f;
            bestWidth = w;
        }
    }
    return best.isEmpty() ? QUrl() : QUrl::fromLocalFile(dir + QLatin1Char('/') + best);
}
