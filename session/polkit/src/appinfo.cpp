#include "appinfo.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace AppInfo
{

namespace
{

QString unescapeUnit(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (int i = 0; i < s.size(); ++i) {
        if (s.at(i) == QLatin1Char('\\') && i + 3 < s.size() && s.at(i + 1) == QLatin1Char('x')) {
            bool ok = false;
            const int c = s.mid(i + 2, 2).toInt(&ok, 16);
            if (ok) {
                out += QChar(c);
                i += 3;
                continue;
            }
        }
        out += s.at(i);
    }
    return out;
}

QStringList splitExec(const QString &exec)
{
    QStringList words;
    QString word;
    bool quoted = false;
    bool inWord = false;
    for (int i = 0; i < exec.size(); ++i) {
        const QChar c = exec.at(i);
        if (quoted) {
            if (c == QLatin1Char('\\') && i + 1 < exec.size()) {
                word += exec.at(++i);
            } else if (c == QLatin1Char('"')) {
                quoted = false;
            } else {
                word += c;
            }
        } else if (c == QLatin1Char('"')) {
            quoted = true;
            inWord = true;
        } else if (c.isSpace()) {
            if (inWord)
                words << word;
            word.clear();
            inWord = false;
        } else {
            word += c;
            inWord = true;
        }
    }
    if (inWord)
        words << word;
    return words;
}

QString baseName(const QString &path)
{
    return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
}

QString run(const QString &program, const QStringList &args, int timeoutMs)
{
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    p.setProcessEnvironment(env);
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start(program, args, QIODevice::ReadOnly);
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(500);
        return QString();
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        return QString();
    return QString::fromUtf8(p.readAllStandardOutput());
}

QString readSmall(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.read(256 * 1024));
}

QStringList cmdline(qint64 pid)
{
    QFile f(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QStringList args;
    for (const QByteArray &a : f.readAll().split('\0')) {
        if (!a.isEmpty())
            args << QString::fromLocal8Bit(a);
    }
    return args;
}

qint64 parentPid(qint64 pid)
{
    // the name in /proc/<pid>/stat may hold spaces and parentheses
    const QString stat = readSmall(QStringLiteral("/proc/%1/stat").arg(pid));
    const int end = stat.lastIndexOf(QLatin1Char(')'));
    if (end < 0)
        return 0;
    const QStringList fields = stat.mid(end + 2).split(QLatin1Char(' '));
    return fields.size() > 1 ? fields.at(1).toLongLong() : 0;
}

QString exePath(qint64 pid)
{
    const QString exe = QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(pid));
    if (!exe.isEmpty())
        return exe;
    // a process of another user: its command line is still readable
    const QStringList args = cmdline(pid);
    if (args.isEmpty())
        return QString();
    const QString first = args.first();
    if (first.startsWith(QLatin1Char('/')))
        return first;
    return QStandardPaths::findExecutable(first);
}

// Processes of the session itself: walking up past them finds no app.
bool isSessionProcess(const QString &program)
{
    static const QStringList names = {
        QStringLiteral("systemd"),          QStringLiteral("plasmashell"),
        QStringLiteral("kwin_wayland"),     QStringLiteral("kwin_x11"),
        QStringLiteral("ksmserver"),        QStringLiteral("startplasma-wayland"),
        QStringLiteral("startplasma-x11"),  QStringLiteral("plasma_session"),
        QStringLiteral("sddm"),             QStringLiteral("sddm-helper"),
        QStringLiteral("kwin_wayland_wrapper"),
    };
    return names.contains(program);
}

class DesktopIndex
{
public:
    explicit DesktopIndex(const QString &lang)
    {
        QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
        for (const QString &dir : std::as_const(dirs)) {
            QDirIterator it(dir, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                QString id = QDir(dir).relativeFilePath(path);
                id.chop(8);
                id.replace(QLatin1Char('/'), QLatin1Char('-'));
                if (m_byId.contains(id))
                    continue;
                DesktopEntry e = parseDesktopEntry(readSmall(path), lang);
                e.id = id;
                m_byId.insert(id, m_entries.size());
                m_entries << e;
            }
        }
    }

    const DesktopEntry *byId(const QString &id) const
    {
        const auto it = m_byId.constFind(id);
        return it == m_byId.constEnd() ? nullptr : &m_entries.at(*it);
    }

    const DesktopEntry *byProgram(const QString &program) const
    {
        if (program.isEmpty())
            return nullptr;
        const DesktopEntry *found = nullptr;
        for (const DesktopEntry &e : m_entries) {
            if (e.program != program || e.name.isEmpty())
                continue;
            if (!found || (found->hidden && !e.hidden)
                || (found->hidden == e.hidden && e.id.endsWith(program, Qt::CaseInsensitive)
                    && !found->id.endsWith(program, Qt::CaseInsensitive)))
                found = &e;
        }
        return found;
    }

private:
    QList<DesktopEntry> m_entries;
    QHash<QString, qsizetype> m_byId;
};

QString publisherOfFile(const QString &path)
{
    if (path.isEmpty())
        return QString();

    QStringList candidates = {path};
    const QString canonical = QFileInfo(path).canonicalFilePath();
    if (!canonical.isEmpty() && canonical != path)
        candidates << canonical;
    // dpkg knows a file by the path its package ships, before or after the
    // merge of /bin into /usr/bin
    for (const QString &p : QStringList(candidates)) {
        static const QList<QPair<QString, QString>> merged = {
            {QStringLiteral("/usr/bin/"), QStringLiteral("/bin/")},
            {QStringLiteral("/usr/sbin/"), QStringLiteral("/sbin/")},
            {QStringLiteral("/usr/lib/"), QStringLiteral("/lib/")},
        };
        for (const auto &[usr, root] : merged) {
            if (p.startsWith(usr))
                candidates << root + p.mid(usr.size());
            else if (p.startsWith(root))
                candidates << usr + p.mid(root.size());
        }
    }

    QString package;
    for (const QString &p : std::as_const(candidates)) {
        package = packageFromSearch(run(QStringLiteral("dpkg-query"), {QStringLiteral("-S"), p}, 2000));
        if (!package.isEmpty())
            break;
    }
    if (package.isEmpty())
        return QString();

    const QString maintainer =
        run(QStringLiteral("dpkg-query"), {QStringLiteral("-W"), QStringLiteral("-f=${Maintainer}"), package}, 2000);
    const QStringList sources =
        installedSources(run(QStringLiteral("apt-cache"), {QStringLiteral("policy"), package}, 4000));
    QStringList origins;
    for (const QString &source : sources) {
        const QString url = source.section(QLatin1Char(' '), 0, 0);
        const QString suite = source.section(QLatin1Char(' '), 1, 1);
        QString name = releaseFileName(url, suite);
        QString release = readSmall(QStringLiteral("/var/lib/apt/lists/") + name);
        if (release.isEmpty()) {
            name.replace(QLatin1String("_InRelease"), QLatin1String("_Release"));
            release = readSmall(QStringLiteral("/var/lib/apt/lists/") + name);
        }
        origins << releaseOrigin(release);
    }
    return publisherFor(package, sources, origins, maintainer);
}

} // namespace

QString desktopIdFromCgroup(const QString &cgroup)
{
    QString path;
    const QStringList lines = cgroup.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("0::")))
            path = line.mid(3);
    }
    QString unit = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1).trimmed();
    if (!unit.startsWith(QLatin1String("app-")))
        return QString();
    unit = unit.mid(4);

    if (unit.endsWith(QLatin1String(".scope"))) {
        unit.chop(6);
        const int dash = unit.lastIndexOf(QLatin1Char('-'));
        if (dash > 0)
            unit.truncate(dash);
    } else if (unit.endsWith(QLatin1String(".service"))) {
        unit.chop(8);
        const int at = unit.indexOf(QLatin1Char('@'));
        if (at > 0)
            unit.truncate(at);
    } else {
        return QString();
    }

    // launchers that put their own name first
    for (const QLatin1String prefix : {QLatin1String("flatpak-"), QLatin1String("gnome-"), QLatin1String("kde-")}) {
        if (unit.startsWith(prefix)) {
            unit = unit.mid(prefix.size());
            break;
        }
    }
    return unescapeUnit(unit);
}

DesktopEntry parseDesktopEntry(const QString &text, const QString &lang)
{
    QString l = lang.section(QLatin1Char('.'), 0, 0).section(QLatin1Char('@'), 0, 0);
    const QString full = QStringLiteral("Name[%1]").arg(l);
    const QString shortName = QStringLiteral("Name[%1]").arg(l.section(QLatin1Char('_'), 0, 0));

    DesktopEntry e;
    QString name;
    QString nameShort;
    QString nameFull;
    bool inGroup = false;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inGroup = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inGroup || line.startsWith(QLatin1Char('#')))
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if (key == QLatin1String("Name"))
            name = value;
        else if (!l.isEmpty() && key == full)
            nameFull = value;
        else if (!l.isEmpty() && key == shortName)
            nameShort = value;
        else if (key == QLatin1String("Icon"))
            e.icon = value;
        else if (key == QLatin1String("Exec"))
            e.program = execProgram(value);
        else if ((key == QLatin1String("NoDisplay") || key == QLatin1String("Hidden")) && value == QLatin1String("true"))
            e.hidden = true;
    }
    e.name = !nameFull.isEmpty() ? nameFull : !nameShort.isEmpty() ? nameShort : name;
    return e;
}

QString execProgram(const QString &exec)
{
    const QStringList words = splitExec(exec);
    int i = 0;
    if (i < words.size() && baseName(words.at(i)) == QLatin1String("env")) {
        ++i;
        while (i < words.size() && words.at(i).contains(QLatin1Char('=')) && !words.at(i).startsWith(QLatin1Char('-')))
            ++i;
    }
    if (i >= words.size())
        return QString();
    const QString program = baseName(words.at(i));
    if (program == QLatin1String("flatpak") && i + 1 < words.size() && words.at(i + 1) == QLatin1String("run")) {
        for (int j = i + 2; j < words.size(); ++j) {
            if (!words.at(j).startsWith(QLatin1Char('-')))
                return words.at(j);
        }
    }
    return program;
}

QString packageFromSearch(const QString &output)
{
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("diversion by ")))
            continue;
        const int sep = line.indexOf(QLatin1String(": "));
        if (sep <= 0)
            continue;
        QString package = line.left(sep).section(QLatin1Char(','), 0, 0).trimmed();
        return package.section(QLatin1Char(':'), 0, 0);
    }
    return QString();
}

QStringList installedSources(const QString &policy)
{
    QStringList sources;
    bool inInstalled = false;
    const QStringList lines = policy.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QStringList words = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!inInstalled) {
            inInstalled = !words.isEmpty() && words.first() == QLatin1String("***");
            continue;
        }
        bool number = false;
        if (words.size() < 2 || (words.first().toInt(&number), !number))
            break;
        if (words.at(1).contains(QLatin1String("://")) && words.size() >= 3)
            sources << words.at(1) + QLatin1Char(' ') + words.at(2).section(QLatin1Char('/'), 0, 0);
    }
    return sources;
}

QString releaseFileName(const QString &url, const QString &suite)
{
    QString base = url.mid(url.indexOf(QLatin1String("://")) + 3);
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    QString name = base + QLatin1String("/dists/") + suite + QLatin1String("/InRelease");
    return name.replace(QLatin1Char('/'), QLatin1Char('_'));
}

QString releaseOrigin(const QString &release)
{
    static const QRegularExpression re(QStringLiteral("^Origin:\\s*(.+)$"), QRegularExpression::MultilineOption);
    const auto m = re.match(release);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

QString publisherFor(const QString &package, const QStringList &sources, const QStringList &origins,
                     const QString &maintainer)
{
    static const QString raye = QStringLiteral("Raye Linux");
    if (package.isEmpty())
        return QString();

    static const QStringList ourHosts = {
        QStringLiteral("archive.ubuntu.com"), QStringLiteral("security.ubuntu.com"),
        QStringLiteral("ports.ubuntu.com"),   QStringLiteral("dl.rayelinux.com"),
        QStringLiteral("raye-linux.github.io"),
    };
    for (qsizetype i = 0; i < sources.size(); ++i) {
        const QString host = QUrl(sources.at(i).section(QLatin1Char(' '), 0, 0)).host();
        const QString origin = i < origins.size() ? origins.at(i) : QString();
        if (ourHosts.contains(host) || host.endsWith(QLatin1String(".archive.ubuntu.com"))
            || origin.compare(QLatin1String("Ubuntu"), Qt::CaseInsensitive) == 0
            || origin.compare(QLatin1String("Raye"), Qt::CaseInsensitive) == 0)
            return raye;
    }
    if (!sources.isEmpty()) {
        QString origin = origins.value(0);
        if (origin.isEmpty())
            return QUrl(sources.first().section(QLatin1Char(' '), 0, 0)).host();
        if (origin == origin.toLower())
            origin[0] = origin.at(0).toUpper();
        return origin;
    }

    // Not in any archive apt knows: installed from the install image, or a
    // .deb opened by hand. Only Raye's and Ubuntu's own builds count.
    static const QStringList ours = {
        QStringLiteral("lists.ubuntu.com"), QStringLiteral("@ubuntu.com"),
        QStringLiteral("@canonical.com"),   QStringLiteral("techoraye"),
    };
    for (const QString &mark : ours) {
        if (maintainer.contains(mark, Qt::CaseInsensitive))
            return raye;
    }
    return QString();
}

QStringList pkexecCommand(const QStringList &argv)
{
    if (argv.isEmpty() || baseName(argv.first()) != QLatin1String("pkexec"))
        return {};
    int i = 1;
    while (i < argv.size() && argv.at(i).startsWith(QLatin1String("--"))) {
        if (argv.at(i) == QLatin1String("--user") || argv.at(i) == QLatin1String("-u"))
            ++i;
        ++i;
    }
    if (i < argv.size() && argv.at(i) == QLatin1String("-u"))
        i += 2;
    return argv.mid(i);
}

QStringList pkexecCommandOf(qint64 pid)
{
    return pid > 0 ? pkexecCommand(cmdline(pid)) : QStringList();
}

bool isInterpreter(const QString &program)
{
    static const QRegularExpression re(
        QStringLiteral("^(python[0-9.]*|perl[0-9.]*|ruby[0-9.]*|lua[0-9.]*|node|nodejs|gjs|bash|sh|dash|zsh)$"));
    return re.match(program).hasMatch();
}

Caller resolve(qint64 subjectPid, const QString &program, const QString &command)
{
    Caller c;
    c.program = program;
    c.command = command;

    const DesktopIndex index(QLocale::system().name());
    const DesktopEntry *entry = program.isEmpty() ? nullptr : index.byProgram(baseName(program));

    // Up from the process polkit asks about to the first one that is an
    // app: a pkexec typed in the terminal finds the terminal.
    QString firstExe;
    qint64 pid = subjectPid;
    for (int depth = 0; !entry && pid > 1 && depth < 12; ++depth) {
        const QString exe = exePath(pid);
        if (firstExe.isEmpty())
            firstExe = exe;
        const QString id = desktopIdFromCgroup(readSmall(QStringLiteral("/proc/%1/cgroup").arg(pid)));
        if (!id.isEmpty())
            entry = index.byId(id);
        QString name = baseName(exe);
        if (!entry && isInterpreter(name)) {
            const QStringList args = cmdline(pid);
            for (int i = 1; i < args.size(); ++i) {
                if (!args.at(i).startsWith(QLatin1Char('-'))) {
                    name = baseName(args.at(i));
                    break;
                }
            }
        }
        if (!entry)
            entry = index.byProgram(name);
        if (entry) {
            if (c.program.isEmpty())
                c.program = exe;
            break;
        }
        if (isSessionProcess(name))
            break;
        pid = parentPid(pid);
    }
    if (c.program.isEmpty())
        c.program = firstExe;

    c.publisher = publisherOfFile(c.program);
    // A program nobody vouches for is named itself, not after the app that
    // started it: "Terminal, Publisher: Unknown" would blame the terminal.
    const bool ownEntry = !program.isEmpty() && entry && entry->program == baseName(program);
    if (entry && (ownEntry || program.isEmpty() || !c.publisher.isEmpty())) {
        c.name = entry->name;
        c.icon = entry->icon;
    } else {
        c.name = baseName(c.program);
    }
    return c;
}

} // namespace AppInfo
