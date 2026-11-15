#include "scheme.h"

#include <KColorScheme>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace
{
// how far up from the program the repo root may be (tools/gallery/build/...)
constexpr int maxLevelsUp = 8;

// A cursor theme also has an index.theme in icons/, but no icon folders.
bool hasIcons(const QString &indexTheme)
{
    QFile file(indexTheme);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    while (!file.atEnd()) {
        if (file.readLine().trimmed().startsWith("Directories=")) {
            return true;
        }
    }
    return false;
}

QString walkUpFor(const QString &startDir, const QString &relative)
{
    QDir dir(startDir);
    for (int i = 0; i <= maxLevelsUp; ++i) {
        if (QFileInfo::exists(dir.filePath(relative))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return {};
}
}

SchemeManager::SchemeManager(const QString &colorsDir, QObject *parent)
    : QObject(parent)
    , m_colorsDir(colorsDir)
{
}

QString SchemeManager::findColorsDir(const QString &startDir)
{
    const QString name = fileName(Scheme::Night);
    const QString root = walkUpFor(startDir, QStringLiteral("colors/") + name);
    return root.isEmpty() ? QString() : QDir(root).filePath(QStringLiteral("colors"));
}

QString SchemeManager::installedColorsDir()
{
    const QString installed = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                     QStringLiteral("color-schemes/") + fileName(Scheme::Night));
    return installed.isEmpty() ? QString() : QFileInfo(installed).absolutePath();
}

QString SchemeManager::fileName(Scheme scheme)
{
    return scheme == Scheme::Day ? QStringLiteral("RayeDay.colors") : QStringLiteral("RayeNight.colors");
}

QString SchemeManager::colorsDir() const
{
    return m_colorsDir;
}

QString SchemeManager::fileFor(Scheme scheme) const
{
    return QDir(m_colorsDir).filePath(fileName(scheme));
}

bool SchemeManager::apply(Scheme scheme, QString *error)
{
    const QString path = fileFor(scheme);
    const QFileInfo info(path);
    if (m_colorsDir.isEmpty() || !info.isFile() || !info.isReadable()) {
        if (error) {
            *error = QStringLiteral("no colour scheme file %1").arg(path);
        }
        return false;
    }
    auto config = KSharedConfig::openConfig(info.absoluteFilePath(), KConfig::SimpleConfig);
    if (!config->hasGroup(QStringLiteral("Colors:Window"))) {
        if (error) {
            *error = QStringLiteral("%1 is not a colour scheme").arg(path);
        }
        return false;
    }

    m_config = config;
    m_current = scheme;
    m_applied = true;
    // what KColorSchemeManager does: the Qt Quick style reads the file
    // through this property, the widgets through the palette
    qApp->setProperty("KDE_COLOR_SCHEME_PATH", info.absoluteFilePath());
    QApplication::setPalette(KColorScheme::createApplicationPalette(config));
    Q_EMIT schemeChanged(scheme);
    return true;
}

Scheme SchemeManager::current() const
{
    return m_current;
}

bool SchemeManager::applied() const
{
    return m_applied;
}

KSharedConfigPtr SchemeManager::config() const
{
    return m_config;
}

QString pickStyle(const QString &requested, const QStringList &available)
{
    auto find = [&available](const QString &name) -> QString {
        for (const QString &key : available) {
            if (key.compare(name, Qt::CaseInsensitive) == 0) {
                return key;
            }
        }
        return {};
    };
    if (!requested.isEmpty()) {
        return find(requested);
    }
    for (const auto *name : {"Raye", "Breeze", "Fusion"}) {
        const QString key = find(QLatin1String(name));
        if (!key.isEmpty()) {
            return key;
        }
    }
    return available.isEmpty() ? QString() : available.first();
}

QString pickIconTheme(Scheme scheme, const QStringList &installed)
{
    const bool night = scheme == Scheme::Night;
    const QStringList order = night ? QStringList{QStringLiteral("Raye-Dark"), QStringLiteral("breeze-dark")}
                                    : QStringList{QStringLiteral("Raye"), QStringLiteral("breeze")};
    for (const QString &name : order) {
        if (installed.contains(name)) {
            return name;
        }
    }
    return {};
}

QStringList installedIconThemes()
{
    QStringList names;
    const auto dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons"),
                                                QStandardPaths::LocateDirectory);
    for (const QString &base : dirs) {
        const auto entries = QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &name : entries) {
            if (!names.contains(name) && hasIcons(QDir(base).filePath(name + QStringLiteral("/index.theme")))) {
                names << name;
            }
        }
    }
    return names;
}

QString findQmlImportDir(const QString &startDir)
{
    const QString root = walkUpFor(startDir, QStringLiteral("qml/org/raye/ui"));
    return root.isEmpty() ? QString() : QDir(root).filePath(QStringLiteral("qml"));
}
