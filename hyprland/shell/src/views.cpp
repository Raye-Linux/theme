// SPDX-License-Identifier: GPL-3.0-or-later

#include "views.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace
{
const QString appletsrcName = QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc");
}

Views::Views(QObject *parent)
    : QObject(parent)
{
    // Plasma writes its settings through a new file, so the folder is
    // watched, not the file
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (!dir.isEmpty())
        m_watcher.addPath(dir);
    QObject::connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        const QFileInfo info(appletsrc());
        const QDateTime now = info.exists() ? info.lastModified() : QDateTime();
        if (now == m_lastChange)
            return;
        m_lastChange = now;
        ++m_revision;
        Q_EMIT backgroundChanged();
    });
}

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

QString Views::pickPicture(const QString &dir, const QSize &size)
{
    // the pictures are named WIDTHxHEIGHT.png
    QString best;
    int bestWidth = 0;
    const QStringList files = QDir(dir).entryList({QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                                                   QStringLiteral("*.webp")},
                                                  QDir::Files);
    for (const QString &f : files) {
        const int w = f.section(QLatin1Char('x'), 0, 0).toInt();
        const bool fits = w >= size.width();
        const bool bestFits = bestWidth >= size.width();
        if (best.isEmpty() || (fits && (!bestFits || w < bestWidth)) || (!fits && !bestFits && w > bestWidth)) {
            best = f;
            bestWidth = w;
        }
    }
    return best.isEmpty() ? QString() : dir + QLatin1Char('/') + best;
}

QUrl Views::wallpaper(const QSize &size, const QString &name) const
{
    const QString dir = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                               QStringLiteral("wallpapers/") + name + QStringLiteral("/contents/images"),
                                               QStandardPaths::LocateDirectory);
    if (dir.isEmpty())
        return {};
    const QString best = pickPicture(dir, size);
    return best.isEmpty() ? QUrl() : QUrl::fromLocalFile(best);
}

QVariantMap Views::readBackground(const QString &appletsrc, int screen, const QSize &size)
{
    if (!QFileInfo::exists(appletsrc))
        return {};
    // the same choice as Raye Settings: desktops only (form factor 0),
    // this screen's, else the first screen's, else the first one
    KConfig file(appletsrc, KConfig::SimpleConfig);
    const KConfigGroup containments = file.group(QStringLiteral("Containments"));
    QStringList ids = containments.groupList();
    std::sort(ids.begin(), ids.end(), [](const QString &a, const QString &b) { return a.toInt() < b.toInt(); });
    KConfigGroup pick;
    int pickScreen = -2;
    for (const QString &id : std::as_const(ids)) {
        const KConfigGroup g = containments.group(id);
        if (!g.hasKey("wallpaperplugin") || g.readEntry("formfactor", 0) != 0)
            continue;
        const int last = g.readEntry("lastScreen", -1);
        const auto rank = [screen](int s) { return s == screen ? 2 : (s == 0 ? 1 : 0); };
        if (!pick.isValid() || rank(last) > rank(pickScreen)) {
            pick = g;
            pickScreen = last;
        }
    }
    if (!pick.isValid())
        return {};

    const QString plugin = pick.readEntry("wallpaperplugin", QStringLiteral("org.kde.image"));
    const KConfigGroup general = pick.group(QStringLiteral("Wallpaper")).group(plugin).group(QStringLiteral("General"));
    if (plugin == QLatin1String("org.kde.color")) {
        // "R,G,B" (KConfigGui reads colours, ConfigCore does not)
        const QStringList rgb = general.readEntry("Color", QString()).split(QLatin1Char(','));
        const QColor colour = rgb.size() >= 3 ? QColor(rgb[0].trimmed().toInt(), rgb[1].trimmed().toInt(), rgb[2].trimmed().toInt())
                                              : QColor(general.readEntry("Color", QString()).trimmed());
        return colour.isValid() ? QVariantMap{{QStringLiteral("color"), colour}} : QVariantMap();
    }
    if (plugin != QLatin1String("org.kde.image"))
        return {};
    const QString image = general.readEntry("Image");
    if (image.isEmpty())
        return {};
    const QString path = QUrl::fromUserInput(image, {}, QUrl::AssumeLocalFile).toLocalFile();
    const QFileInfo info(path);
    if (info.isDir()) {
        const QString best = pickPicture(QDir(path).filePath(QStringLiteral("contents/images")), size);
        return best.isEmpty() ? QVariantMap() : QVariantMap{{QStringLiteral("source"), QUrl::fromLocalFile(best)}};
    }
    if (!info.isFile())
        return {};
    return {{QStringLiteral("source"), QUrl::fromLocalFile(info.absoluteFilePath())}};
}

QString Views::appletsrc() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + appletsrcName;
}

QVariantMap Views::background(int screen, const QSize &size) const
{
    return readBackground(appletsrc(), screen, size);
}
