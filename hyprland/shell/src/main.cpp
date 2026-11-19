// raye-hypr-shell: the Raye taskbar, Start, quick settings, notifications
// and wallpaper on Hyprland. Plasma's own shell cannot run there, so this
// host loads the same Raye views (from the installed applets) into
// layer-shell windows and feeds them from Hyprland instead of KWin.
//
//   raye-hypr-shell                     run the shell
//   raye-hypr-shell --toggle WHAT       start, search, quick, centre, quicklinks
//   raye-hypr-shell --snap DIR          left, right, up, down (Win+arrows)
//   raye-hypr-shell --desktop           Win+D
//   raye-hypr-shell --task N            Win+1...9
//   raye-hypr-shell --launch ID         start an app by its desktop id
//   raye-hypr-shell --close-desktop     Win+Ctrl+F4
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shellbus.h"

#include <KIO/ApplicationLauncherJob>
#include <KLocalizedQmlContext>
#include <KService>

#include <notificationmanager/server.h>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QUrl>

namespace
{
int callShell(const QString &method, const QVariantList &args)
{
    QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(ShellBus::service),
                                                       QString::fromLatin1(ShellBus::path),
                                                       QStringLiteral("org.raye.HyprShell"), method);
    call.setArguments(args);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 2000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning("raye-hypr-shell: the shell is not running (%s)", qPrintable(reply.errorMessage()));
        return 1;
    }
    return 0;
}

int launch(QGuiApplication &app, const QString &desktopId)
{
    QString name = desktopId;
    if (name.endsWith(QLatin1String(".desktop")))
        name.chop(8);
    const KService::Ptr service = KService::serviceByDesktopName(name);
    if (!service) {
        qWarning("raye-hypr-shell: no app %s", qPrintable(desktopId));
        return 1;
    }
    auto *job = new KIO::ApplicationLauncherJob(service);
    int code = 0;
    QObject::connect(job, &KJob::result, &app, [&code, &app](KJob *j) {
        code = j->error() ? 1 : 0;
        app.quit();
    });
    job->start();
    app.exec();
    return code;
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("raye-hypr-shell"));
    app.setDesktopFileName(QStringLiteral("org.raye.hyprshell"));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption toggle(QStringLiteral("toggle"), QStringLiteral("Open or close a part of the shell."), QStringLiteral("what"));
    const QCommandLineOption snap(QStringLiteral("snap"), QStringLiteral("Snap the active window."), QStringLiteral("direction"));
    const QCommandLineOption desktop(QStringLiteral("desktop"), QStringLiteral("Show the desktop, or the windows again."));
    const QCommandLineOption task(QStringLiteral("task"), QStringLiteral("Open the app at this place on the taskbar."), QStringLiteral("number"));
    const QCommandLineOption launchOption(QStringLiteral("launch"), QStringLiteral("Start an app."), QStringLiteral("desktop id"));
    const QCommandLineOption closeDesktop(QStringLiteral("close-desktop"), QStringLiteral("Close the desktop in use."));
    // the shell's QML from another folder (tests, previews)
    const QCommandLineOption qml(QStringLiteral("qml"), QStringLiteral("Load this file instead of the shell."), QStringLiteral("file"));
    parser.addOptions({toggle, snap, desktop, task, launchOption, closeDesktop, qml});
    parser.process(app);

    if (parser.isSet(toggle))
        return callShell(QStringLiteral("Toggle"), {parser.value(toggle)});
    if (parser.isSet(snap))
        return callShell(QStringLiteral("Snap"), {parser.value(snap)});
    if (parser.isSet(desktop))
        return callShell(QStringLiteral("Desktop"), {});
    if (parser.isSet(task))
        return callShell(QStringLiteral("Task"), {parser.value(task).toInt()});
    if (parser.isSet(closeDesktop))
        return callShell(QStringLiteral("CloseDesktop"), {});
    if (parser.isSet(launchOption))
        return launch(app, parser.value(launchOption));

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QString::fromLatin1(ShellBus::service))) {
        qWarning("raye-hypr-shell: already running");
        return 0;
    }

    // the notifications of every app come to this shell
    NotificationManager::Server::self().init();

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(1);
    }, Qt::QueuedConnection);
    if (parser.isSet(qml))
        engine.load(QUrl::fromUserInput(parser.value(qml), QDir::currentPath()));
    else
        engine.loadFromModule("org.raye.hypr", "Shell");
    return app.exec();
}
