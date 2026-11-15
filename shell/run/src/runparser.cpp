#include "runparser.h"

#include <KApplicationTrader>
#include <KProtocolInfo>
#include <KService>
#include <KShell>

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace
{

// The names Windows users type in Run, and the Raye app behind each one.
struct Alias {
    const char *names;
    const char *desktopIds;
};

const Alias s_aliases[] = {
    {"cmd powershell pwsh wt terminal", "org.raye.terminal org.kde.konsole"},
    {"notepad wordpad write", "org.raye.notepad org.kde.kwrite org.kde.kate"},
    {"calc", "org.raye.calculator org.kde.kcalc"},
    {"control", "org.raye.controlpanel org.raye.settings"},
    {"ms-settings:", "org.raye.settings"},
    {"regedit regedt32", "org.raye.regedit"},
    {"explorer", "org.kde.dolphin"},
    {"taskmgr resmon perfmon", "org.raye.taskmanager org.kde.plasma-systemmonitor"},
    {"mspaint", "org.kde.kolourpaint"},
    {"snippingtool", "org.raye.snip org.kde.spectacle"},
    {"charmap", "org.kde.kcharselect"},
    {"diskmgmt.msc", "org.kde.partitionmanager"},
    {"eventvwr eventvwr.msc", "org.kde.ksystemlog"},
    {"msinfo32 devmgmt.msc", "org.kde.kinfocenter"},
    {"msedge iexplore", "firefox org.kde.falkon"},
};

// Control Panel applets, Settings addresses and "control <name>" words
// (written "control:<name>"), and the Raye Settings page behind each one.
struct Page {
    const char *names;
    const char *page;
};

const Page s_pages[] = {
    {"desk.cpl ms-settings:display ms-settings:screenrotation", "system/display"},
    {"ms-settings:nightlight", "system/night-light"},
    {"mmsys.cpl ms-settings:sound", "system/sound"},
    {"ms-settings:notifications", "system/notifications"},
    {"powercfg.cpl ms-settings:powersleep ms-settings:batterysaver", "system/power"},
    {"ms-settings:storagesense", "system/storage"},
    {"ms-settings:clipboard", "system/clipboard"},
    {"sysdm.cpl winver ms-settings:about", "system/about"},
    {"bthprops.cpl ms-settings:bluetooth", "devices/bluetooth"},
    {"control:printers ms-settings:printers", "devices/printers"},
    {"main.cpl control:mouse ms-settings:mousetouchpad", "devices/mouse"},
    {"ncpa.cpl ms-settings:network ms-settings:network-status", "network"},
    {"ms-settings:network-wifi", "network/wifi"},
    {"ms-settings:network-ethernet", "network/ethernet"},
    {"ms-settings:network-vpn", "network/vpn"},
    {"inetcpl.cpl ms-settings:network-proxy", "network/proxy"},
    {"control:desktop ms-settings:personalization ms-settings:personalization-background", "personalization/background"},
    {"control:color ms-settings:colors ms-settings:personalization-colors", "personalization/colors"},
    {"ms-settings:themes", "personalization/themes"},
    {"ms-settings:lockscreen", "personalization/lock-screen"},
    {"ms-settings:taskbar", "personalization/taskbar"},
    {"fonts ms-settings:fonts", "personalization/fonts"},
    {"appwiz.cpl ms-settings:appsfeatures ms-settings:installed-apps", "apps/installed"},
    {"ms-settings:defaultapps", "apps/default"},
    {"ms-settings:startupapps", "apps/startup"},
    {"ms-settings:yourinfo", "accounts/your-info"},
    {"ms-settings:signinoptions", "accounts/sign-in"},
    {"netplwiz control:userpasswords control:userpasswords2 ms-settings:otherusers", "accounts/other-users"},
    {"timedate.cpl ms-settings:dateandtime", "time/date-time"},
    {"intl.cpl ms-settings:regionlanguage", "time/language"},
    {"control:keyboard ms-settings:typing", "time/typing"},
    {"joy.cpl", "gaming/controllers"},
    {"ms-settings:gaming-gamemode", "gaming/game-mode"},
    {"ms-settings:easeofaccess", "accessibility"},
    {"ms-settings:privacy", "privacy"},
};

// Folder names of Windows ("%temp%", "shell:downloads") and the user
// folder behind each one. Names are split on "|": some hold a space.
struct Folder {
    const char *names;
    const char *folder;
};

const Folder s_folders[] = {
    {"%userprofile%|%homepath%|shell:profile", "home"},
    {"shell:desktop", "desktop"},
    {"shell:personal|shell:documents", "documents"},
    {"shell:downloads", "downloads"},
    {"shell:my pictures|shell:pictures", "pictures"},
    {"shell:my music|shell:music", "music"},
    {"shell:my video|shell:videos", "videos"},
    {"%temp%|%tmp%", "temp"},
    {"%appdata%", "config"},
    {"%localappdata%", "data"},
    {"shell:startup", "startup"},
};

// Common top-level domains: "example.com" opens the website, while
// "notes.txt" stays a file name.
const QStringList s_domains = {
    QStringLiteral("com"), QStringLiteral("org"), QStringLiteral("net"), QStringLiteral("edu"),
    QStringLiteral("gov"), QStringLiteral("io"), QStringLiteral("dev"), QStringLiteral("app"),
    QStringLiteral("info"), QStringLiteral("biz"), QStringLiteral("me"), QStringLiteral("co"),
    QStringLiteral("tv"), QStringLiteral("gg"), QStringLiteral("eu"), QStringLiteral("ca"),
    QStringLiteral("fr"), QStringLiteral("be"), QStringLiteral("ch"), QStringLiteral("de"),
    QStringLiteral("uk"), QStringLiteral("us"), QStringLiteral("it"), QStringLiteral("es"),
    QStringLiteral("nl"), QStringLiteral("jp"), QStringLiteral("cn"), QStringLiteral("ru"),
    QStringLiteral("br"), QStringLiteral("au"), QStringLiteral("in"), QStringLiteral("xyz"),
    QStringLiteral("ly"), QStringLiteral("so"), QStringLiteral("to"),
};

// Addresses that need no lookup to be recognised.
const QStringList s_webSchemes = {QStringLiteral("http"), QStringLiteral("https"), QStringLiteral("ftp"),
                                  QStringLiteral("mailto"), QStringLiteral("file")};

bool isWebAddress(const QString &text, QUrl *url)
{
    static const QRegularExpression hostLike(QStringLiteral(
        "^(?:www\\.)?((?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+([a-z]{2,24})|localhost)(?::\\d{1,5})?(?:[/?#]\\S*)?$"),
                                             QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = hostLike.match(text);
    if (!m.hasMatch())
        return false;
    const bool www = text.startsWith(QLatin1String("www."), Qt::CaseInsensitive);
    const bool local = m.captured(1).compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0;
    if (!www && !local && !s_domains.contains(m.captured(2).toLower()))
        return false;
    *url = QUrl(QStringLiteral("%1://%2").arg(local ? QStringLiteral("http") : QStringLiteral("https"), text));
    return url->isValid();
}

// "~" and "~/..." are the user's home; "~name/..." another user's.
QString expandTilde(const QString &path, const QString &home)
{
    if (path == QLatin1String("~") || path.startsWith(QLatin1String("~/")))
        return home + path.mid(1);
    return path.startsWith(QLatin1Char('~')) ? KShell::tildeExpand(path) : path;
}

QString absolutePath(const QString &path, const QString &home)
{
    QString expanded = expandTilde(path, home);
    if (!QDir::isAbsolutePath(expanded))
        expanded = home + QLatin1Char('/') + expanded;
    return QDir::cleanPath(expanded);
}

bool looksLikePath(const QString &text)
{
    return text.startsWith(QLatin1Char('/')) || text.startsWith(QLatin1Char('~')) || text.startsWith(QLatin1String("./"))
        || text.startsWith(QLatin1String("../"));
}

RunTarget notFound(const QString &name)
{
    RunTarget t;
    t.kind = RunTarget::NotFound;
    t.name = name;
    return t;
}

} // namespace

QStringList RunParser::windowsAlias(const QString &name)
{
    QString key = name.trimmed().toLower();
    if (key.endsWith(QLatin1String(".exe")))
        key.chop(4);
    if (key.isEmpty())
        return {};
    for (const Alias &alias : s_aliases) {
        if (QString::fromLatin1(alias.names).split(QLatin1Char(' ')).contains(key))
            return QString::fromLatin1(alias.desktopIds).split(QLatin1Char(' '));
    }
    return {};
}

QString RunParser::settingsPage(const QString &name)
{
    const QString key = name.trimmed().toLower();
    if (key.isEmpty())
        return {};
    for (const Page &page : s_pages) {
        if (QString::fromLatin1(page.names).split(QLatin1Char(' ')).contains(key))
            return QString::fromLatin1(page.page);
    }
    return {};
}

QString RunParser::textForPath(const QString &path)
{
    return KShell::quoteArg(path);
}

QStringList RunParser::folderNames()
{
    QStringList names;
    for (const Folder &folder : s_folders)
        names.append(QString::fromLatin1(folder.folder));
    return names;
}

namespace
{
// "%temp%", "shell:downloads\old", "%userprofile%/Music": the user's own
// folder, with what follows it; empty when the line does not start with
// one of those names.
QString windowsFolder(const QString &text, const RunParser::Environment &env)
{
    QString line = text;
    line.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const int slash = line.indexOf(QLatin1Char('/'));
    const QString head = (slash < 0 ? line : line.left(slash)).toLower();
    const QString rest = slash < 0 ? QString() : line.mid(slash);
    for (const Folder &folder : s_folders) {
        if (!QString::fromLatin1(folder.names).split(QLatin1Char('|')).contains(head))
            continue;
        const QString path = env.folder ? env.folder(QString::fromLatin1(folder.folder)) : QString();
        if (path.isEmpty())
            return {};
        return QDir::cleanPath(path + rest);
    }
    return {};
}

// A program of that name, unless it only stands in for a Windows tool
// that Raye has its own app for: "regedit" is Raye's, not the Windows layer's.
QString realProgram(const QString &name, const RunParser::Environment &env)
{
    const QString program = env.findExecutable(name);
    if (program.isEmpty() || !env.isWindowsShim || !env.isWindowsShim(program))
        return program;
    const QStringList ids = RunParser::windowsAlias(name);
    return ids.isEmpty() || env.installedApp(ids).isEmpty() ? program : QString();
}
} // namespace

RunTarget RunParser::parse(const QString &input, const Environment &env)
{
    const QString text = input.trimmed();
    RunTarget t;
    if (text.isEmpty())
        return t;

    const bool oneWord = !text.contains(QRegularExpression(QStringLiteral("\\s")));

    // A folder by its Windows name: %temp%, shell:downloads...
    const QString folder = windowsFolder(text, env);
    if (!folder.isEmpty()) {
        if (!env.exists(folder))
            return notFound(text);
        t.kind = RunTarget::Path;
        t.url = QUrl::fromLocalFile(folder);
        return t;
    }

    const bool hasSettings = !env.installedApp({QStringLiteral("org.raye.settings")}).isEmpty();

    // A Control Panel applet or a Settings address: its Raye Settings page.
    if (oneWord && hasSettings && env.findExecutable(text).isEmpty()) {
        const QString page = settingsPage(text);
        if (!page.isEmpty()) {
            t.kind = RunTarget::Url;
            t.url = QUrl(QStringLiteral("settings:") + page);
            return t;
        }
        // a Settings page Raye does not have: Settings itself
        if (text.startsWith(QLatin1String("ms-settings:"), Qt::CaseInsensitive)) {
            t.kind = RunTarget::Application;
            t.desktopId = QStringLiteral("org.raye.settings");
            return t;
        }
    }

    // A Windows program name, when nothing of that name is installed.
    if (oneWord && realProgram(text, env).isEmpty()) {
        const QStringList ids = windowsAlias(text);
        if (!ids.isEmpty()) {
            const QString id = env.installedApp(ids);
            if (!id.isEmpty()) {
                t.kind = RunTarget::Application;
                t.desktopId = id;
                return t;
            }
        }
    }

    // An address with a scheme: https://..., mailto:..., smb://...
    static const QRegularExpression schemeRe(QStringLiteral("^([a-zA-Z][a-zA-Z0-9+.-]+):(.*)$"));
    const QRegularExpressionMatch scheme = schemeRe.match(text);
    if (oneWord && scheme.hasMatch()) {
        const QString name = scheme.captured(1).toLower();
        if (s_webSchemes.contains(name) || env.isKnownScheme(name)) {
            const QUrl url(text);
            if (url.isValid()) {
                t.kind = RunTarget::Url;
                t.url = url;
                return t;
            }
        }
    }

    // A file or folder: absolute, from the home folder, or a name in the
    // home folder (Run starts there, as Windows starts in the profile).
    // A program of that name wins over a name in the home folder.
    const bool pathLike = looksLikePath(text);
    if (pathLike || (oneWord && !text.startsWith(QLatin1Char('-')) && env.findExecutable(text).isEmpty())) {
        const QString path = absolutePath(text, env.home);
        if (env.exists(path)) {
            if (pathLike && env.isExecutable(path)) {
                t.kind = RunTarget::Command;
                t.program = path;
                return t;
            }
            t.kind = RunTarget::Path;
            t.url = QUrl::fromLocalFile(path);
            return t;
        }
    }

    // A website without its scheme: www.example.com, example.org/page.
    QUrl web;
    if (oneWord && isWebAddress(text, &web)) {
        t.kind = RunTarget::Url;
        t.url = web;
        return t;
    }

    // A program and its arguments.
    KShell::Errors error = KShell::NoError;
    QStringList args = KShell::splitArgs(text, KShell::AbortOnMeta, &error);
    if (error == KShell::FoundMeta) {
        // Pipes, redirections, variables: the user wrote a shell line.
        t.kind = RunTarget::ShellCommand;
        t.text = text;
        return t;
    }
    if (error != KShell::NoError || args.isEmpty())
        return notFound(text);

    for (QString &arg : args)
        arg = expandTilde(arg, env.home);
    const QString first = args.first();
    if (first.contains(QLatin1Char('/'))) {
        const QString path = absolutePath(first, env.home);
        if (env.exists(path) && env.isExecutable(path)) {
            t.kind = RunTarget::Command;
            t.program = path;
            t.args = args.mid(1);
            return t;
        }
        if (args.size() == 1 && env.exists(path)) {
            // a quoted path, as Browse writes it
            t.kind = RunTarget::Path;
            t.url = QUrl::fromLocalFile(path);
            return t;
        }
        return notFound(text);
    }

    const QString program = realProgram(first, env);
    if (!program.isEmpty()) {
        t.kind = RunTarget::Command;
        t.program = program;
        t.args = args.mid(1);
        return t;
    }

    // "control mmsys.cpl", "control printers": the Settings page
    if (args.size() == 2 && first.compare(QLatin1String("control"), Qt::CaseInsensitive) == 0 && hasSettings) {
        QString page = settingsPage(QStringLiteral("control:") + args.at(1));
        if (page.isEmpty())
            page = settingsPage(args.at(1));
        if (!page.isEmpty()) {
            t.kind = RunTarget::Url;
            t.url = QUrl(QStringLiteral("settings:") + page);
            return t;
        }
    }

    // A Windows program name with what it opens: "notepad notes.txt",
    // "explorer Documents". The arguments are files, from the home folder.
    // Switches ("cmd /c dir", "notepad -x") are not files: those lines are
    // not understood rather than half run.
    if (args.size() > 1) {
        const QStringList ids = windowsAlias(first);
        const QString id = ids.isEmpty() ? QString() : env.installedApp(ids);
        if (!id.isEmpty()) {
            static const QRegularExpression windowsSwitch(QStringLiteral("^/[a-z?]{1,2}$"), QRegularExpression::CaseInsensitiveOption);
            t.kind = RunTarget::Application;
            t.desktopId = id;
            for (const QString &arg : args.mid(1)) {
                if (arg.startsWith(QLatin1Char('-')) || windowsSwitch.match(arg).hasMatch())
                    return notFound(text);
                const QString folder = windowsFolder(arg, env);
                t.urls.append(QUrl::fromLocalFile(folder.isEmpty() ? absolutePath(arg, env.home) : folder));
            }
            return t;
        }
    }

    if (args.size() == 1) {
        const QString id = env.installedApp({first});
        if (!id.isEmpty()) {
            t.kind = RunTarget::Application;
            t.desktopId = id;
            return t;
        }
    }
    return notFound(text);
}

bool RunParser::isWindowsLayerPath(const QString &canonicalPath)
{
    if (canonicalPath.isEmpty())
        return false;
    return canonicalPath.startsWith(QLatin1String("/usr/lib/raye/windows/")) || canonicalPath.startsWith(QLatin1String("/opt/raye-wine/"))
        || canonicalPath.contains(QLatin1String("/wine-")) || canonicalPath.contains(QLatin1String("/wine/"));
}

RunParser::Environment RunParser::systemEnvironment()
{
    Environment env;
    env.home = QDir::homePath();
    env.findExecutable = [](const QString &name) {
        return QStandardPaths::findExecutable(name);
    };
    env.exists = [](const QString &path) {
        return QFileInfo::exists(path);
    };
    env.isExecutable = [](const QString &path) {
        const QFileInfo info(path);
        return info.isFile() && info.isExecutable();
    };
    env.isWindowsShim = [](const QString &program) {
        return isWindowsLayerPath(QFileInfo(program).canonicalFilePath());
    };
    env.isKnownScheme = [](const QString &scheme) {
        // a KIO protocol, or a scheme an app says it opens (settings:)
        return KProtocolInfo::isKnownProtocol(scheme)
            || KApplicationTrader::preferredService(QStringLiteral("x-scheme-handler/") + scheme);
    };
    env.folder = [](const QString &name) -> QString {
        const QString home = QDir::homePath();
        if (name == QLatin1String("home"))
            return home;
        if (name == QLatin1String("desktop"))
            return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (name == QLatin1String("documents"))
            return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (name == QLatin1String("downloads"))
            return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (name == QLatin1String("pictures"))
            return QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (name == QLatin1String("music"))
            return QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        if (name == QLatin1String("videos"))
            return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        if (name == QLatin1String("temp"))
            return QDir::tempPath();
        if (name == QLatin1String("config"))
            return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        if (name == QLatin1String("data"))
            return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (name == QLatin1String("startup"))
            return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart");
        return QString();
    };
    env.installedApp = [](const QStringList &ids) {
        for (const QString &id : ids) {
            const KService::Ptr service = KService::serviceByDesktopName(id);
            if (service && !service->exec().isEmpty())
                return service->desktopEntryName();
        }
        return QString();
    };
    return env;
}
