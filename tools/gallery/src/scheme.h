// Night and Day for the whole gallery: the KDE colour scheme files the
// theme generates, applied to the widgets (palette) and to Qt Quick
// (qqc2-desktop-style and Kirigami read KDE_COLOR_SCHEME_PATH), plus the
// choices that depend on what is installed: widget style, icon theme.

#pragma once

#include "options.h"

#include <KSharedConfig>
#include <QObject>

class SchemeManager : public QObject
{
    Q_OBJECT

public:
    explicit SchemeManager(const QString &colorsDir, QObject *parent = nullptr);

    // The repo's colors/ (the folder that holds RayeNight.colors), found by
    // walking up from startDir; empty when there is none.
    static QString findColorsDir(const QString &startDir);
    // The installed color-schemes folder with RayeNight.colors, or empty.
    static QString installedColorsDir();
    static QString fileName(Scheme scheme);

    QString colorsDir() const;
    QString fileFor(Scheme scheme) const;

    // Applies the scheme to the running application. On failure nothing
    // changes and error says why.
    bool apply(Scheme scheme, QString *error = nullptr);

    Scheme current() const;
    bool applied() const;
    KSharedConfigPtr config() const;

Q_SIGNALS:
    void schemeChanged(Scheme scheme);

private:
    QString m_colorsDir;
    Scheme m_current = Scheme::Night;
    bool m_applied = false;
    KSharedConfigPtr m_config;
};

// requested if it exists (any case); else Raye, then Breeze, then Fusion.
// Empty when a requested style is missing.
QString pickStyle(const QString &requested, const QStringList &available);

// Raye's own icon theme when it is installed, else Breeze; empty: keep Qt's.
QString pickIconTheme(Scheme scheme, const QStringList &installed);

// names of the icon themes that have an index.theme in the data folders
QStringList installedIconThemes();

// The repo's qml/ folder above startDir, empty when there is none.
QString findQmlImportDir(const QString &startDir);
