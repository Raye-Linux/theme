// raye-project: the Project panel (Win+P). One panel at a time: a second
// start tells the open one to move on to the next choice.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "displaycontroller.h"
#include "switcherservice.h"

#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <LayerShellQt/Window>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

namespace
{
const QString s_service = QStringLiteral("org.raye.Project");
const QString s_path = QStringLiteral("/Project");

bool onWayland()
{
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

// Bottom right of the screen in use, just above the taskbar. The window
// has room for its shadow around the panel, so the margins are its own.
void placeOnScreen(QWindow *window)
{
    if (!onWayland())
        return;
    auto *layer = LayerShellQt::Window::get(window);
    layer->setScope(QStringLiteral("raye-project"));
    layer->setLayer(LayerShellQt::Window::LayerTop);
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorRight));
    layer->setMargins(QMargins(0, 0, 0, 0));
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    layer->setWantsToBeOnActiveScreen(true);
}
} // namespace

int main(int argc, char **argv)
{
    // started by the bus in place of the stock display switcher: no window,
    // so no graphics stack either
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--display-switcher") == 0) {
            QCoreApplication service(argc, argv);
            return runSwitcherService(service);
        }
    }

    QGuiApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("raye_shell_project");
    app.setApplicationName(QStringLiteral("raye-project"));
    app.setDesktopFileName(QStringLiteral("org.raye.project"));
    app.setApplicationDisplayName(i18nd("raye_shell_project", "Project"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("video-display")));

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && !bus.registerService(s_service)) {
        bus.call(QDBusMessage::createMethodCall(s_service, s_path, s_service, QStringLiteral("activate")));
        return 0;
    }

    DisplayController controller;
    bus.registerObject(s_path, &controller, QDBusConnection::ExportScriptableSlots);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.setInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(&controller)},
    });
    engine.loadFromModule("org.raye.shell.project", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window)
        return 1;
    placeOnScreen(window);
    window->show();
    window->requestActivate();
    return app.exec();
}
