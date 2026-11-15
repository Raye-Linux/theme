// The run part of the checks: plasmawindowed on a private bus, and what
// its log says about the applet. See plasmoidcheck.h.

#include "plasmoidcheck.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>
#include <csignal>
#include <unistd.h>

namespace RayeShellCheck
{

QList<Problem> classifyRunLog(const QString &log, const QString &id)
{
    QList<Problem> problems;
    QSet<QString> seen;
    auto add = [&](const QString &file, int line, const QString &message) {
        const QString key = file + QLatin1Char('\n') + QString::number(line) + QLatin1Char('\n') + message;
        if (!seen.contains(key)) {
            seen.insert(key);
            problems << Problem{file, line, message};
        }
    };

    // Where the applet's own files show up in messages: its package folder,
    // or the resource path of a compiled applet.
    const QString packagePath = QStringLiteral("/plasmoids/") + id + QLatin1Char('/');
    const QString resourcePath = QLatin1Char('/') + QString(id).replace(QLatin1Char('.'), QLatin1Char('/')) + QLatin1Char('/');
    const QString loadError = QStringLiteral("error when loading applet \"") + id + QLatin1Char('"');
    static const QRegularExpression qmlWarning(QStringLiteral("((?:file|qrc):\\S+?\\.(?:qml|js|mjs)):(\\d+)(?::\\d+)?: (.*)$"));

    for (const QString &raw : log.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.contains(loadError)) {
            add(QString(), 0, QStringLiteral("Plasma could not load the applet: %1").arg(line.mid(line.indexOf(loadError) + loadError.size()).trimmed()));
            continue;
        }
        if (line.startsWith(QLatin1String("kf.package:")) && line.contains(packagePath)) {
            add(QString(), 0, line.mid(int(qstrlen("kf.package:"))).trimmed());
            continue;
        }
        const QRegularExpressionMatch m = qmlWarning.match(line);
        if (!m.hasMatch()) {
            continue;
        }
        const QString url = m.captured(1);
        int at = url.indexOf(packagePath);
        int skip = packagePath.size();
        if (at < 0 && url.startsWith(QLatin1String("qrc:"))) {
            at = url.indexOf(resourcePath);
            skip = resourcePath.size();
        }
        if (at < 0) {
            continue;
        }
        add(url.mid(at + skip), m.captured(2).toInt(), m.captured(3));
    }
    return problems;
}

RunResult runApplet(const QString &id, const RunOptions &options)
{
    RunResult result;
    QTemporaryDir tmp(QDir::tempPath() + QStringLiteral("/raye-shell-run-XXXXXX"));
    if (!tmp.isValid()) {
        result.log = QStringLiteral("cannot make a temporary folder");
        return result;
    }
    const QString root = tmp.path();
    for (const char *sub : {"home", "config", "cache", "data", "runtime"}) {
        QDir().mkpath(root + QLatin1Char('/') + QLatin1String(sub));
    }
    QFile::setPermissions(root + QStringLiteral("/runtime"), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    // A bus of our own with no service directory: nothing gets started on
    // demand, so nothing is left running once the applet stops.
    QFile bus(root + QStringLiteral("/bus.conf"));
    if (!bus.open(QIODevice::WriteOnly)) {
        result.log = QStringLiteral("cannot write the bus configuration");
        return result;
    }
    bus.write(QStringLiteral("<!DOCTYPE busconfig PUBLIC \"-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN\"\n"
                             " \"http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd\">\n"
                             "<busconfig>\n"
                             "  <type>session</type>\n"
                             "  <listen>unix:dir=%1</listen>\n"
                             "  <policy context=\"default\">\n"
                             "    <allow send_destination=\"*\" eavesdrop=\"true\"/>\n"
                             "    <allow eavesdrop=\"true\"/>\n"
                             "    <allow own=\"*\"/>\n"
                             "  </policy>\n"
                             "</busconfig>\n")
                  .arg(root + QStringLiteral("/runtime"))
                  .toUtf8());
    bus.close();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("HOME"), root + QStringLiteral("/home"));
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), root + QStringLiteral("/config"));
    env.insert(QStringLiteral("XDG_CACHE_HOME"), root + QStringLiteral("/cache"));
    env.insert(QStringLiteral("XDG_DATA_HOME"), root + QStringLiteral("/data"));
    env.insert(QStringLiteral("XDG_RUNTIME_DIR"), root + QStringLiteral("/runtime"));
    env.remove(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"));

    auto prepend = [&env](const QString &name, const QStringList &dirs, const QString &fallback) {
        QStringList all = dirs;
        const QString current = env.value(name, fallback);
        if (!current.isEmpty()) {
            all << current;
        }
        if (!all.isEmpty()) {
            env.insert(name, all.join(QLatin1Char(':')));
        }
    };
    prepend(QStringLiteral("XDG_DATA_DIRS"), options.dataDirs, QStringLiteral("/usr/local/share:/usr/share"));
    prepend(QStringLiteral("QT_PLUGIN_PATH"), options.pluginDirs, QString());
    prepend(QStringLiteral("QML_IMPORT_PATH"), options.qmlDirs, QString());

    if (options.language.isEmpty()) {
        env.remove(QStringLiteral("LANGUAGE"));
        env.remove(QStringLiteral("LC_ALL"));
        env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    } else {
        env.insert(QStringLiteral("LANGUAGE"), options.language);
    }
    if (!env.contains(QStringLiteral("QT_QPA_PLATFORM")) && !env.contains(QStringLiteral("DISPLAY")) && !env.contains(QStringLiteral("WAYLAND_DISPLAY"))) {
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    }
    if (!env.contains(QStringLiteral("QT_QUICK_BACKEND"))) {
        env.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
    }

    const QString runSession = QStandardPaths::findExecutable(QStringLiteral("dbus-run-session"));
    const QString timeout = QStandardPaths::findExecutable(QStringLiteral("timeout"));
    const QString windowed = QStandardPaths::findExecutable(QStringLiteral("plasmawindowed"));
    if (runSession.isEmpty() || timeout.isEmpty() || windowed.isEmpty()) {
        result.log = QStringLiteral("dbus-run-session, timeout and plasmawindowed must all be installed");
        return result;
    }

    const int ms = std::max(500, options.milliseconds);
    QProcess process;
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(root);
    process.setProcessChannelMode(QProcess::MergedChannels);
    // its own process group, so a run that hangs can be stopped whole
    process.setChildProcessModifier([] {
        ::setsid();
    });
    // timeout stops the applet; dbus-run-session then stops the bus
    process.start(runSession,
                  {QStringLiteral("--config-file=") + bus.fileName(),
                   QStringLiteral("--"),
                   timeout,
                   QStringLiteral("--kill-after=3"),
                   QString::number(ms / 1000.0, 'f', 1),
                   windowed,
                   id});
    if (!process.waitForStarted(10000)) {
        result.log = QStringLiteral("could not start the run: %1").arg(process.errorString());
        return result;
    }
    result.started = true;
    const qint64 group = process.processId();
    if (!process.waitForFinished(ms + 20000)) {
        ::kill(-pid_t(group), SIGKILL);
        process.waitForFinished(5000);
        result.log = QString::fromUtf8(process.readAll()) + QStringLiteral("\n(the run did not stop by itself, it was killed)\n");
        result.exitedEarly = false;
        return result;
    }
    // what the bus may have left in the group
    ::kill(-pid_t(group), SIGKILL);
    result.log = QString::fromUtf8(process.readAll());
    result.exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    // 124: timeout stopped it, 137: it had to be killed after that
    result.exitedEarly = result.exitCode != 124 && result.exitCode != 137;
    return result;
}

QList<Problem> checkRun(const RunResult &result, const QString &id)
{
    QList<Problem> problems;
    if (!result.started) {
        problems << Problem{QString(), 0, QStringLiteral("the applet could not be run: %1").arg(result.log.trimmed())};
        return problems;
    }
    if (result.exitedEarly) {
        problems << Problem{QString(), 0, QStringLiteral("plasmawindowed stopped on its own (exit code %1) before the end of the run").arg(result.exitCode)};
    }
    problems += classifyRunLog(result.log, id);
    return problems;
}

} // namespace RayeShellCheck
