// Command line of the gallery. Parsed before QApplication exists, because
// the scale has to be in QT_SCALE_FACTOR before Qt reads it.

#pragma once

#include <QSize>
#include <QString>
#include <QStringList>

enum class Scheme {
    Night,
    Day,
};

struct GalleryOptions {
    Scheme scheme = Scheme::Night;
    // 0 leaves QT_SCALE_FACTOR as the environment has it
    double scale = 0;
    QString page;
    // empty: the whole screen
    QSize size;
    QString shootFile;
    QString colorsDir;
    QString qmlImportDir;
    QString style;
    bool listPages = false;
    bool help = false;
};

struct ParsedOptions {
    GalleryOptions options;
    QString error;
    bool ok() const
    {
        return error.isEmpty();
    }
};

// arguments without the program name
ParsedOptions parseGalleryOptions(const QStringList &arguments);

QString schemeName(Scheme scheme);
bool parseScheme(const QString &text, Scheme *scheme);
// "1.25" or "125%"; accepted from 0.5 to 3
bool parseScale(const QString &text, double *scale);
bool parseSize(const QString &text, QSize *size);

// The same command line with another scale, for the scale switch, which
// has to start the gallery again.
QStringList relaunchArguments(const GalleryOptions &options, double scale);

QString galleryUsage();
