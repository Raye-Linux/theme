// What a line typed in the Run box means: a website, a file or folder,
// a program with its arguments, a shell line, or an app known by its
// Windows name. No side effect: everything the parser needs to know about
// the system comes through Environment, so tests can fake it.
#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

struct RunTarget {
    enum Kind {
        Empty, // nothing typed
        Url, // a website or any address KIO knows (url)
        Path, // an existing file or folder, opened with its app (url)
        Command, // a program, run directly without a shell (program, args)
        ShellCommand, // a line with pipes, redirections or variables (text)
        Application, // an installed app, by desktop id (desktopId), with
                     // the files it opens (urls)
        NotFound, // nothing matches (name: what to quote in the message)
    };

    Kind kind = Empty;
    QUrl url;
    QString program;
    QStringList args;
    QString text;
    QString desktopId;
    QList<QUrl> urls;
    QString name;
};

class RunParser
{
public:
    struct Environment {
        QString home;
        // full path of a program found in PATH, or empty
        std::function<QString(const QString &)> findExecutable;
        // true when a file or folder exists at this absolute path
        std::function<bool(const QString &)> exists;
        // true when this absolute path is a file the user may run
        std::function<bool(const QString &)> isExecutable;
        // true when KIO can open this scheme (smb, sftp, settings...)
        std::function<bool(const QString &)> isKnownScheme;
        // true when this program only stands in for a Windows tool (the
        // Windows layer's notepad, regedit): Raye's own app wins over it
        std::function<bool(const QString &)> isWindowsShim;
        // the first of these desktop ids that is installed, or empty
        std::function<QString(const QStringList &)> installedApp;
        // a folder of the user by its name (see RunParser::folderNames):
        // "documents" -> /home/ana/Documents; empty when unknown
        std::function<QString(const QString &)> folder;
    };

    static RunTarget parse(const QString &text, const Environment &env);
    static Environment systemEnvironment();

    // Desktop ids to try for a Windows program name ("notepad", "cmd.exe"),
    // most Raye-like first; empty when the name is not one of them.
    static QStringList windowsAlias(const QString &name);

    // True when a program, followed through its links, belongs to the
    // Windows layer: Wine itself or Raye's Windows commands.
    static bool isWindowsLayerPath(const QString &canonicalPath);

    // The Raye Settings page for a Control Panel or Settings name
    // ("mmsys.cpl", "ms-settings:display", "winver"), or empty.
    static QString settingsPage(const QString &name);

    // The text for a file picked with Browse: the path, quoted when it
    // holds spaces or other characters a command line would split on.
    static QString textForPath(const QString &path);

    // The folder names Environment::folder knows.
    static QStringList folderNames();
};
