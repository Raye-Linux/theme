// Runs harness.qml (or another QML file given first) with the shell's
// translation functions, so the pictures are in English or French:
//
//   start_harness [file.qml] [--day] [--open] [--search]
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("raye_shell_start");

    QString file = QStringLiteral(RAYE_START_TESTS "/harness.qml");
    const QStringList args = app.arguments();
    if (args.size() > 1 && args.at(1).endsWith(QLatin1String(".qml")))
        file = args.at(1);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.load(QUrl::fromLocalFile(file));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
