// What the prompt says about the app that asks: its name and icon, the
// program that will run, and who published it.
//
// The parsing helpers are free functions so the tests can feed them text;
// resolve() is the part that reads /proc, the desktop files and the
// package database.
#pragma once

#include <QString>
#include <QStringList>

namespace AppInfo
{

struct DesktopEntry {
    QString id; // "org.kde.konsole"
    QString name;
    QString icon;
    QString program; // basename of the program in Exec (or the Flatpak id)
    bool hidden = false; // NoDisplay or Hidden
};

struct Caller {
    QString name;
    QString icon; // theme icon name or absolute path
    QString program; // full path of what runs, when known
    QString command; // the command line, for pkexec
    QString publisher; // "Raye Linux", "Mozilla"...; empty when unknown
};

// The desktop file id of the app a process was started as, from the text
// of /proc/<pid>/cgroup: "app-org.kde.konsole-12345.scope" ->
// "org.kde.konsole". Empty when the process is not in an app unit.
QString desktopIdFromCgroup(const QString &cgroup);

// A [Desktop Entry] group. lang is a locale name such as "fr_CA".
DesktopEntry parseDesktopEntry(const QString &text, const QString &lang);

// The program an Exec line starts: "env A=1 /usr/bin/foo --x %U" -> "foo",
// "flatpak run --branch=stable org.app.Id" -> "org.app.Id".
QString execProgram(const QString &exec);

// The package that owns a file, from `dpkg-query -S` output.
QString packageFromSearch(const QString &output);

// The apt sources of the installed version in `apt-cache policy <pkg>`
// output, as "url suite" pairs ("http://archive.ubuntu.com/ubuntu resolute").
// The dpkg status file is left out.
QStringList installedSources(const QString &policy);

// The file in /var/lib/apt/lists holding the Release of a source.
QString releaseFileName(const QString &url, const QString &suite);

// The "Origin:" of a Release file.
QString releaseOrigin(const QString &release);

// Who published a package. origins are the Origin fields (or hosts) of the
// apt sources its installed version came from; empty when it only comes
// from the dpkg status (installed by hand or from the install image).
// Returns "Raye Linux" for the Raye and Ubuntu archives, the origin of any
// other archive, and nothing (unknown) for a package that is not in an
// archive and was not built by Raye or Ubuntu.
QString publisherFor(const QString &package, const QStringList &sources, const QStringList &origins,
                     const QString &maintainer);

// What a pkexec command line runs: the program and its arguments, after
// pkexec's own options ("pkexec --user root /usr/bin/x -y" -> "/usr/bin/x -y").
// polkit does not pass pkexec's program on to the agent, so it is read
// from pkexec's process. Empty when argv is not pkexec's.
QStringList pkexecCommand(const QStringList &argv);

// Programs that run a script: the script names the app, not the interpreter.
bool isInterpreter(const QString &program);

// Looks up the caller. subjectPid is the process polkit asks about (0 when
// unknown); program and command come from pkexec's details.
Caller resolve(qint64 subjectPid, const QString &program, const QString &command);

// The command a pkexec process runs, from its pid (empty when it is not
// pkexec).
QStringList pkexecCommandOf(qint64 pid);

} // namespace AppInfo
