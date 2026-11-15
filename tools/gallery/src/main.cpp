// raye-gallery: every Qt control and state, drawn by the style in use,
// in Raye Night or Raye Day, at any scale. The reference board for the
// Raye style and QML pieces; tools/shoot.sh captures every page.

#include "gallery.h"
#include "window.h"

#include <QApplication>
#include <QScreen>
#include <QTextStream>

int main(int argc, char **argv)
{
    QStringList args;
    for (int i = 1; i < argc; ++i) {
        args << QString::fromLocal8Bit(argv[i]);
    }
    const ParsedOptions parsed = parseGalleryOptions(args);
    QTextStream out(stdout);
    QTextStream err(stderr);
    if (!parsed.ok()) {
        err << "raye-gallery: " << parsed.error << "\n\n" << galleryUsage();
        return 1;
    }
    const GalleryOptions &options = parsed.options;
    if (options.help) {
        out << galleryUsage();
        return 0;
    }
    if (options.listPages) {
        for (const PageInfo &page : galleryPages()) {
            out << page.id << '\t' << page.section << '\t' << page.title << '\n';
        }
        return 0;
    }
    const QString pageId = options.page.isEmpty() ? galleryPages().first().id : options.page;
    if (!findPage(pageId)) {
        err << "raye-gallery: no page " << pageId << " (see --list-pages)\n";
        return 1;
    }

    prepareEnvironment(options);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("raye-gallery"));

    const GallerySetup setup = setupApplication(options);
    if (!setup.error.isEmpty()) {
        err << "raye-gallery: " << setup.error << '\n';
        return 1;
    }

    GalleryWindow window(options, setup.schemes, setup.context);
    window.showPage(pageId);
    if (options.size.isValid()) {
        window.setGeometry(QRect(QPoint(0, 0), options.size / app.devicePixelRatio()));
    } else {
        window.setGeometry(window.screen()->geometry());
    }
    window.show();

    if (!options.shootFile.isEmpty()) {
        scheduleShot(&window, options.shootFile, [](int code) {
            QApplication::exit(code);
        });
    }
    return app.exec();
}
