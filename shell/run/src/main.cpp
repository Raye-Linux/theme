// raye-run: the Run box (Win+R). One box at a time: a second start brings
// the open one back.

#include "runapp.h"
#include "runbox.h"

#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <LayerShellQt/Window>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

namespace
{
const QString s_service = QStringLiteral("org.raye.Run");
const QString s_path = QStringLiteral("/Run");

bool onWayland()
{
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

// Bottom left, just above the taskbar, like Windows. The window has room
// for its shadow around the box, so the margins are those of the shadow.
void placeOnScreen(QWindow *window)
{
    if (!onWayland())
        return;
    auto *layer = LayerShellQt::Window::get(window);
    layer->setScope(QStringLiteral("raye-run"));
    layer->setLayer(LayerShellQt::Window::LayerTop);
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft));
    layer->setMargins(QMargins(0, 0, 0, 0));
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    layer->setWantsToBeOnActiveScreen(true);
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("raye_shell_run");
    app.setApplicationName(QStringLiteral("raye-run"));
    app.setDesktopFileName(QStringLiteral("org.raye.run"));
    app.setApplicationDisplayName(i18nd("raye_shell_run", "Run"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    RunApp::setUp(app);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && !bus.registerService(s_service)) {
        bus.call(QDBusMessage::createMethodCall(s_service, s_path, s_service, QStringLiteral("activate")));
        return 0;
    }

    RunBox box;
    bus.registerObject(s_path, &box, QDBusConnection::ExportScriptableSlots);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.setInitialProperties({
        {QStringLiteral("runBox"), QVariant::fromValue(&box)},
        {QStringLiteral("shadowMargin"), onWayland() ? 16 : 0},
    });
    engine.loadFromModule("org.raye.shell.run", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window)
        return 1;
    box.setWindow(window);
    placeOnScreen(window);
    window->show();
    window->requestActivate();
    return app.exec();
}
