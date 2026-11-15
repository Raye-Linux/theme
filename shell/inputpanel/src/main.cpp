// raye-inputpanel: emoji, kaomoji, symbols and clipboard history in one
// panel (Win+. and Win+V). One panel at a time: a second start asks the
// running one to open.
//   raye-inputpanel [--clipboard | --emoji | --kaomoji | --symbols]

#include "panel.h"

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStandardPaths>

namespace
{
const QString s_service = QStringLiteral("org.raye.InputPanel");
const QString s_path = QStringLiteral("/InputPanel");

// Pinned clipboard items live in this file: only the user may read it.
KSharedConfigPtr openState()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/raye-inputpanelstaterc");
    QFile file(path);
    if (file.open(QIODevice::ReadWrite))
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    return KSharedConfig::openConfig(path, KConfig::SimpleConfig);
}
} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("raye_shell_inputpanel");
    app.setApplicationName(QStringLiteral("raye-inputpanel"));
    app.setDesktopFileName(QStringLiteral("org.raye.inputpanel"));
    app.setApplicationDisplayName(i18nd("raye_shell_inputpanel", "Emoji and clipboard"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("face-smile")));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    const QCommandLineOption clipboard(QStringLiteral("clipboard"), i18nd("raye_shell_inputpanel", "Open on the clipboard history"));
    const QCommandLineOption emoji(QStringLiteral("emoji"), i18nd("raye_shell_inputpanel", "Open on the emoji"));
    const QCommandLineOption kaomoji(QStringLiteral("kaomoji"), i18nd("raye_shell_inputpanel", "Open on the kaomoji"));
    const QCommandLineOption symbols(QStringLiteral("symbols"), i18nd("raye_shell_inputpanel", "Open on the symbols"));
    parser.addOptions({clipboard, emoji, kaomoji, symbols});
    parser.addHelpOption();
    parser.process(app);
    const QString mode = parser.isSet(clipboard) ? QStringLiteral("clipboard")
        : parser.isSet(kaomoji)                  ? QStringLiteral("kaomoji")
        : parser.isSet(symbols)                  ? QStringLiteral("symbols")
                                                 : QStringLiteral("emoji");

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() && !bus.registerService(s_service)) {
        bus.call(QDBusMessage::createMethodCall(s_service, s_path, s_service, QStringLiteral("open")) << mode);
        return 0;
    }

    Panel panel(openState());
    bus.registerObject(s_path, &panel, QDBusConnection::ExportScriptableSlots);

    const bool wayland = QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
    const int shadowMargin = wayland ? 16 : 0;
    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.setInitialProperties({
        {QStringLiteral("panel"), QVariant::fromValue(&panel)},
        {QStringLiteral("shadowMargin"), shadowMargin},
    });
    engine.loadFromModule("org.raye.shell.inputpanel", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window)
        return 1;
    panel.setWindow(window, shadowMargin);
    panel.open(mode);
    return app.exec();
}
