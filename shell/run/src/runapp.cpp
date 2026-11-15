#include "runapp.h"

#include <QGuiApplication>

void RunApp::setUp(QGuiApplication &app)
{
    app.setQuitOnLastWindowClosed(false);
    app.setQuitLockEnabled(false);
}
