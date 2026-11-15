// Runs harness.qml with the shell's translation functions, so the pictures
// are in English or French:
//
//   quicksettings_harness [--day] [--edit] [--no-player] [--laptop] [--vpn]
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QUrl>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("raye_shell_quicksettings");

    // the pictures show the glyphs of the session, not a blank fallback
    QIcon::setThemeName(QStringLiteral("breeze-dark"));

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.load(QUrl::fromLocalFile(QStringLiteral(RAYE_QS_TESTS "/harness.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
