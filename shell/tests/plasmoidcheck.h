// Checks run on every applet of the shell before it ships: metadata,
// French strings, QML that compiles, and the log of a real load.

#pragma once

#include <QHash>
#include <QList>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

class QQmlEngine;

namespace RayeShellCheck
{

struct Problem {
    QString file; // relative to the applet folder, empty when it is about the applet
    int line = 0;
    QString message;

    QString toString() const;
};

QString describe(const QList<Problem> &problems);

// One call to a KDE i18n function found in QML or JS source.
struct I18nCall {
    QString function; // i18nd, i18ndc, xi18ndp...
    int line = 0;
    bool hasDomain = false; // the d variants
    bool domainLiteral = false;
    QString domain;
    bool textLiteral = false; // context, text and plural are plain string literals
    QString context;
    QString text;
    QString plural;
};

QList<I18nCall> scanI18nCalls(const QString &source);

// Lines of qsTr() and friends: the shell translates with i18nd only.
QList<int> scanQtTranslateCalls(const QString &source);

struct PoEntry {
    QString context;
    QString msgid;
    QString plural;
    QStringList msgstr;
    bool fuzzy = false;
};

QString poKey(const QString &context, const QString &msgid);
// Keyed by poKey(). Obsolete (#~) entries and the header are left out.
QHash<QString, PoEntry> parsePo(const QString &text, QString *error = nullptr);

// The %1 %2... a string uses.
QList<int> placeholders(const QString &text);

QList<Problem> checkMetadata(const QString &appletDir);
QList<Problem> checkTranslations(const QString &appletDir, const QString &poDir, const QString &lang = QStringLiteral("fr"));
QList<Problem> checkConfigXml(const QString &appletDir);
QList<Problem> compileQml(const QString &appletDir, QQmlEngine *engine);

// An applet with a CMakeLists.txt is compiled: its QML sits in its plugin.
bool isCompiledApplet(const QString &appletDir);

// Everything above. QML of a compiled applet is left to the load test:
// its C++ types only exist inside its own plugin.
QList<Problem> checkApplet(const QString &appletDir, const QString &poDir, QQmlEngine *engine);

// Problems in the output of plasmawindowed for applet id.
QList<Problem> classifyRunLog(const QString &log, const QString &id);

// Runs plasmawindowed <id> for a while on its own D-Bus session (no service
// is started on demand, so nothing outlives the run) with a throwaway
// home, and gives back what it printed.
struct RunOptions {
    QStringList dataDirs; // put before the system ones, each holds plasma/plasmoids/<id>
    QStringList pluginDirs; // QT_PLUGIN_PATH, for compiled applets
    QStringList qmlDirs; // QML_IMPORT_PATH
    QString language; // LANGUAGE, empty for English
    int milliseconds = 4000;
};

struct RunResult {
    bool started = false;
    bool exitedEarly = false; // it stopped before we stopped it
    int exitCode = 0;
    QString log;
};

RunResult runApplet(const QString &id, const RunOptions &options);

// classifyRunLog() plus a word when the applet did not stay up.
QList<Problem> checkRun(const RunResult &result, const QString &id);

} // namespace RayeShellCheck
