#include "gallery.h"
#include "scheme.h"
#include "window.h"

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QLibraryInfo>
#include <QPixmap>
#include <QQuickStyle>
#include <QScreen>
#include <QStyleFactory>
#include <QTextStream>
#include <QTimer>

namespace
{
// 3.4: body text is Inter 10 pt
constexpr int bodyPointSize = 10;
// when the page gets its focus, popups and pointer after it is shown
constexpr int prepareDelayMs = 500;
// pointer after popups: a menu item can only be hovered once the menu is open
constexpr int hoverDelayMs = 400;
// tooltips wake up after 500-700 ms, animations take 167-250 ms
constexpr int settleDelayMs = 1300;

bool hasQuickStyle(const QString &name)
{
    const QString dir = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    return QFileInfo::exists(QDir(dir).filePath(QString(name).replace(QLatin1Char('.'), QLatin1Char('/')) + QStringLiteral("/qmldir")));
}
}

void prepareEnvironment(const GalleryOptions &options)
{
    if (options.scale > 0) {
        qputenv("QT_SCALE_FACTOR", QByteArray::number(options.scale));
    }
    // the desktop's Qt Quick style draws with the widget style, so both
    // halves of the gallery show the same style
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        qputenv("QT_QUICK_CONTROLS_STYLE", "org.kde.desktop");
    }
}

GallerySetup setupApplication(const GalleryOptions &options)
{
    GallerySetup setup;

    setup.style = pickStyle(options.style, QStyleFactory::keys());
    if (setup.style.isEmpty()) {
        setup.error = QStringLiteral("no widget style named %1 (there are: %2)")
                          .arg(options.style, QStyleFactory::keys().join(QStringLiteral(", ")));
        return setup;
    }
    QApplication::setStyle(setup.style);
    QApplication::setFont(QFont(QStringLiteral("Inter"), bodyPointSize));

    const QString quick = QQuickStyle::name().isEmpty() ? qEnvironmentVariable("QT_QUICK_CONTROLS_STYLE") : QQuickStyle::name();
    if (!quick.isEmpty() && !hasQuickStyle(quick)) {
        QQuickStyle::setStyle(QStringLiteral("Fusion"));
    }

    // next to the program when it is built in the repo, else the source
    // folder it was built from
    const QString start = QCoreApplication::applicationDirPath();
    const QString source = QStringLiteral(RAYE_GALLERY_SOURCE_DIR);
    QString colorsDir = options.colorsDir;
    for (const QString &dir : {start, source}) {
        if (colorsDir.isEmpty()) {
            colorsDir = SchemeManager::findColorsDir(dir);
        }
    }
    if (colorsDir.isEmpty()) {
        colorsDir = SchemeManager::installedColorsDir();
    }
    setup.schemes = new SchemeManager(colorsDir, qApp);
    QString error;
    if (!setup.schemes->apply(options.scheme, &error)) {
        setup.error = error;
        return setup;
    }
    QIcon::setThemeName(pickIconTheme(options.scheme, installedIconThemes()));

    setup.context.schemes = setup.schemes;
    QString imports = options.qmlImportDir;
    if (imports.isEmpty()) {
        imports = findQmlImportDir(start);
    }
    if (imports.isEmpty()) {
        imports = findQmlImportDir(source);
    }
    if (!imports.isEmpty()) {
        setup.context.qmlImportDirs << imports;
    }
    return setup;
}

void scheduleShot(GalleryWindow *window, const QString &file, const std::function<void(int)> &done)
{
    QTimer::singleShot(prepareDelayMs, window, [window, file, done] {
        QScreen *screen = window->screen();
        // park the pointer where nothing reacts to it: the bottom of the page list
        QCursor::setPos(screen, window->mapToGlobal(QPoint(2, window->height() - 2)));
        window->activateWindow();
        GalleryPage *page = window->currentPage();
        if (page) {
            page->focusSample();
            page->openPopups();
        }
        QTimer::singleShot(hoverDelayMs, window, [window, file, done, screen] {
            GalleryPage *page = window->currentPage();
            // again, now that the window is active: activation clears the
            // keyboard-focus flag the focus ring depends on
            if (page) {
                page->focusSample();
            }
            const QRect hover = page ? page->hoverRect() : QRect();
            if (hover.isValid()) {
                QCursor::setPos(screen, hover.center());
            }
            QTimer::singleShot(settleDelayMs, window, [window, file, done, screen] {
                GalleryPage *page = window->currentPage();
                QTextStream err(stderr);
                const QStringList problems = page ? page->problems() : QStringList{QStringLiteral("no page")};
                for (const QString &p : problems) {
                    err << "problem: " << p << '\n';
                }
                // shown for whoever works on the style or the QML pieces, not a failure
                for (const QString &w : page ? page->warnings() : QStringList()) {
                    err << "warning: " << w << '\n';
                }
                const QPixmap shot = screen->grabWindow(0);
                if (shot.isNull() || !shot.save(file, "PNG")) {
                    err << "could not save " << file << '\n';
                    done(ShotNotSaved);
                    return;
                }
                done(problems.isEmpty() ? ShotSaved : ShotPageProblems);
            });
        });
    });
}
