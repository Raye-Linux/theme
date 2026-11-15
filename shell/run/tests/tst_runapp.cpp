// Browse... used to close the whole box: the file dialog lets go of an
// event loop lock when it ends, and Qt quit on that. The box must outlive
// a file dialog opened and cancelled. Qt only quits that way inside
// exec(), so this runs the real event loop rather than QtTest's.
#include "runapp.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoopLocker>
#include <QFileDialog>
#include <QTimer>

#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    RunApp::setUp(app);

    bool shown = false;
    bool endedByTest = false;
    QFileDialog dialog;
    dialog.setOption(QFileDialog::DontUseNativeDialog);

    QTimer::singleShot(0, &app, [&] {
        // what the dialog does on the desktop: holds the lock while open
        auto *lock = new QEventLoopLocker;
        dialog.open();
        shown = dialog.isVisible();
        QTimer::singleShot(100, &app, [&, lock] {
            dialog.reject();
            delete lock;
        });
    });
    QTimer::singleShot(600, &app, [&] {
        endedByTest = true;
        app.exit(0);
    });

    app.exec();

    if (!shown) {
        std::puts("FAIL: the file dialog did not open");
        return 1;
    }
    if (dialog.isVisible()) {
        std::puts("FAIL: the file dialog did not close on Cancel");
        return 1;
    }
    if (!endedByTest) {
        std::puts("FAIL: the application quit when the dialog was cancelled");
        return 1;
    }
    std::puts("PASS: the box outlives a cancelled file dialog");
    return 0;
}
