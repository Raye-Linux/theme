// raye-polkit-agent: answers polkit for the signed-in session with the
// Raye admin prompt. Started by raye-polkit-agent.service.
#include "listener.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QGuiApplication>

#include <cstdio>

int main(int argc, char **argv)
{
    QGuiApplication::setDesktopSettingsAware(true);
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("raye-polkit-agent"));
    app.setQuitOnLastWindowClosed(false);

    // polkit takes one agent per session; the bus name keeps a second copy
    // of this one from even trying
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.raye.PolkitAgent"))) {
        std::fprintf(stderr, "raye-polkit-agent: already running\n");
        return 0;
    }

    Listener listener;
    QString error;
    if (!listener.registerAgent(&error)) {
        std::fprintf(stderr, "raye-polkit-agent: polkit refused the agent: %s\n", qPrintable(error));
        return 1;
    }
    return app.exec();
}
