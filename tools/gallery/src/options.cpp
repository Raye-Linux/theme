#include "options.h"

#include <QRegularExpression>

namespace
{
constexpr double minScale = 0.5;
constexpr double maxScale = 3.0;
constexpr int minSide = 320;
constexpr int maxSide = 7680;

// options that take a value, and where the value goes
bool takesValue(const QString &name)
{
    static const QStringList names = {
        QStringLiteral("--scheme"),
        QStringLiteral("--scale"),
        QStringLiteral("--page"),
        QStringLiteral("--size"),
        QStringLiteral("--shoot"),
        QStringLiteral("--colors-dir"),
        QStringLiteral("--qml-import"),
        QStringLiteral("--style"),
    };
    return names.contains(name);
}
}

QString schemeName(Scheme scheme)
{
    return scheme == Scheme::Day ? QStringLiteral("day") : QStringLiteral("night");
}

bool parseScheme(const QString &text, Scheme *scheme)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("night")) {
        *scheme = Scheme::Night;
        return true;
    }
    if (t == QLatin1String("day")) {
        *scheme = Scheme::Day;
        return true;
    }
    return false;
}

bool parseScale(const QString &text, double *scale)
{
    QString t = text.trimmed();
    double divisor = 1;
    if (t.endsWith(QLatin1Char('%'))) {
        t.chop(1);
        divisor = 100;
    }
    bool ok = false;
    const double value = t.toDouble(&ok) / divisor;
    // written this way round so that nan and inf fail too
    if (!ok || !(value >= minScale && value <= maxScale)) {
        return false;
    }
    *scale = value;
    return true;
}

bool parseSize(const QString &text, QSize *size)
{
    static const QRegularExpression re(QStringLiteral("^([0-9]{3,4})x([0-9]{3,4})$"));
    const auto m = re.match(text.trimmed());
    if (!m.hasMatch()) {
        return false;
    }
    const int w = m.captured(1).toInt();
    const int h = m.captured(2).toInt();
    if (w < minSide || h < minSide || w > maxSide || h > maxSide) {
        return false;
    }
    *size = QSize(w, h);
    return true;
}

ParsedOptions parseGalleryOptions(const QStringList &arguments)
{
    ParsedOptions result;
    GalleryOptions &o = result.options;

    for (int i = 0; i < arguments.size(); ++i) {
        QString name = arguments.at(i);
        QString value;
        bool hasValue = false;
        // --name=value is the same as --name value
        const int eq = name.indexOf(QLatin1Char('='));
        if (name.startsWith(QLatin1String("--")) && eq > 2) {
            value = name.mid(eq + 1);
            name = name.left(eq);
            hasValue = true;
        }

        if (name == QLatin1String("--help") || name == QLatin1String("-h")) {
            o.help = true;
            continue;
        }
        if (name == QLatin1String("--list-pages")) {
            o.listPages = true;
            continue;
        }
        if (!takesValue(name)) {
            result.error = QStringLiteral("unknown option: %1").arg(arguments.at(i));
            return result;
        }
        if (!hasValue) {
            if (i + 1 >= arguments.size()) {
                result.error = QStringLiteral("%1 needs a value").arg(name);
                return result;
            }
            value = arguments.at(++i);
        }
        if (value.isEmpty()) {
            result.error = QStringLiteral("%1 needs a value").arg(name);
            return result;
        }

        if (name == QLatin1String("--scheme")) {
            if (!parseScheme(value, &o.scheme)) {
                result.error = QStringLiteral("--scheme is night or day, not %1").arg(value);
                return result;
            }
        } else if (name == QLatin1String("--scale")) {
            if (!parseScale(value, &o.scale)) {
                result.error = QStringLiteral("--scale goes from 0.5 to 3 (or 50% to 300%), not %1").arg(value);
                return result;
            }
        } else if (name == QLatin1String("--size")) {
            if (!parseSize(value, &o.size)) {
                result.error = QStringLiteral("--size is WIDTHxHEIGHT, 320 to 7680 each, not %1").arg(value);
                return result;
            }
        } else if (name == QLatin1String("--page")) {
            o.page = value;
        } else if (name == QLatin1String("--shoot")) {
            o.shootFile = value;
        } else if (name == QLatin1String("--colors-dir")) {
            o.colorsDir = value;
        } else if (name == QLatin1String("--qml-import")) {
            o.qmlImportDir = value;
        } else if (name == QLatin1String("--style")) {
            o.style = value;
        }
    }
    return result;
}

QStringList relaunchArguments(const GalleryOptions &o, double scale)
{
    QStringList args;
    args << QStringLiteral("--scheme") << schemeName(o.scheme);
    args << QStringLiteral("--scale") << QString::number(scale);
    if (!o.page.isEmpty()) {
        args << QStringLiteral("--page") << o.page;
    }
    if (o.size.isValid()) {
        args << QStringLiteral("--size") << QStringLiteral("%1x%2").arg(o.size.width()).arg(o.size.height());
    }
    if (!o.colorsDir.isEmpty()) {
        args << QStringLiteral("--colors-dir") << o.colorsDir;
    }
    if (!o.qmlImportDir.isEmpty()) {
        args << QStringLiteral("--qml-import") << o.qmlImportDir;
    }
    if (!o.style.isEmpty()) {
        args << QStringLiteral("--style") << o.style;
    }
    // a relaunched gallery is for looking at, never a capture
    return args;
}

QString galleryUsage()
{
    return QStringLiteral(
        "Usage: raye-gallery [options]\n"
        "Every control and state, drawn by the style in use, for side by side checks.\n"
        "\n"
        "  --scheme night|day   colour scheme (default night)\n"
        "  --scale N            display scale, 0.5 to 3, also 125% (default: environment)\n"
        "  --page ID            page to open (see --list-pages)\n"
        "  --size WxH           window size (default: the whole screen)\n"
        "  --style NAME         widget style (default Raye, else Breeze, else Fusion)\n"
        "  --colors-dir DIR     folder with RayeNight.colors and RayeDay.colors\n"
        "  --qml-import DIR     extra QML import folder (default: the repo's qml/)\n"
        "  --shoot FILE.png     capture the screen once the page has settled, then quit\n"
        "  --list-pages         print the page ids and quit\n");
}
