#include "runparser.h"

#include <QTest>

using Kind = RunTarget::Kind;

class TestRunParser : public QObject
{
    Q_OBJECT

private:
    // A made-up system: a few programs, files and apps.
    static RunParser::Environment fake()
    {
        RunParser::Environment env;
        env.home = QStringLiteral("/home/ana");
        env.findExecutable = [](const QString &name) {
            static const QStringList programs = {QStringLiteral("firefox"), QStringLiteral("kate"), QStringLiteral("ls"),
                                                 QStringLiteral("konsole"), QStringLiteral("Documents-tool")};
            return programs.contains(name) ? QStringLiteral("/usr/bin/") + name : QString();
        };
        env.exists = [](const QString &path) {
            static const QStringList paths = {QStringLiteral("/etc"), QStringLiteral("/etc/fstab"), QStringLiteral("/home/ana/Documents"),
                                              QStringLiteral("/home/ana/Documents/My report.odt"), QStringLiteral("/home/ana/bin/tool.sh"),
                                              QStringLiteral("/home/ana/notes.txt"), QStringLiteral("/opt/game/run"),
                                              QStringLiteral("/home/ana/Downloads/setup.exe"), QStringLiteral("/tmp"),
                                              QStringLiteral("/home/ana"), QStringLiteral("/home/ana/Téléchargements"),
                                              QStringLiteral("/home/ana/Téléchargements/old")};
            return paths.contains(path);
        };
        env.isExecutable = [](const QString &path) {
            return path == QLatin1String("/home/ana/bin/tool.sh") || path == QLatin1String("/opt/game/run");
        };
        env.isKnownScheme = [](const QString &scheme) {
            return scheme == QLatin1String("smb") || scheme == QLatin1String("settings");
        };
        env.installedApp = [](const QStringList &ids) {
            static const QStringList installed = {QStringLiteral("org.kde.konsole"), QStringLiteral("org.kde.kwrite"),
                                                  QStringLiteral("org.raye.settings"), QStringLiteral("org.kde.dolphin"),
                                                  QStringLiteral("org.kde.gwenview")};
            for (const QString &id : ids) {
                if (installed.contains(id))
                    return id;
            }
            return QString();
        };
        env.folder = [](const QString &name) {
            if (name == QLatin1String("home"))
                return QStringLiteral("/home/ana");
            if (name == QLatin1String("downloads"))
                return QStringLiteral("/home/ana/Téléchargements");
            if (name == QLatin1String("temp"))
                return QStringLiteral("/tmp");
            if (name == QLatin1String("documents"))
                return QStringLiteral("/home/ana/Documents");
            return QString();
        };
        return env;
    }

private Q_SLOTS:
    void empty()
    {
        QCOMPARE(RunParser::parse(QString(), fake()).kind, Kind::Empty);
        QCOMPARE(RunParser::parse(QStringLiteral("   "), fake()).kind, Kind::Empty);
    }

    void urls_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("url");
        QTest::newRow("https") << "https://raye.example.org/a?b=c" << "https://raye.example.org/a?b=c";
        QTest::newRow("http, spaces around") << "  http://example.com  " << "http://example.com";
        QTest::newRow("mailto") << "mailto:ana@example.com" << "mailto:ana@example.com";
        QTest::newRow("kio scheme") << "smb://nas/share" << "smb://nas/share";
        QTest::newRow("app scheme") << "settings:display" << "settings:display";
        QTest::newRow("www") << "www.example.com" << "https://www.example.com";
        QTest::newRow("domain") << "example.org" << "https://example.org";
        QTest::newRow("domain with path") << "wikipedia.org/wiki/Paris" << "https://wikipedia.org/wiki/Paris";
        QTest::newRow("country domain") << "lemonde.fr" << "https://lemonde.fr";
        QTest::newRow("localhost") << "localhost:8080" << "http://localhost:8080";
    }

    void urls()
    {
        QFETCH(QString, text);
        QFETCH(QString, url);
        const RunTarget t = RunParser::parse(text, fake());
        QCOMPARE(t.kind, Kind::Url);
        QCOMPARE(t.url, QUrl(url));
    }

    void notAWebsite()
    {
        // a file name is not a domain, and an unknown scheme is not an address
        QCOMPARE(RunParser::parse(QStringLiteral("report.pdf"), fake()).kind, Kind::NotFound);
        QCOMPARE(RunParser::parse(QStringLiteral("bogus:thing"), fake()).kind, Kind::NotFound);
        QCOMPARE(RunParser::parse(QStringLiteral("C:\\Windows"), fake()).kind, Kind::NotFound);
    }

    void paths_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("path");
        QTest::newRow("folder") << "/etc" << "/etc";
        QTest::newRow("file") << "/etc/fstab" << "/etc/fstab";
        QTest::newRow("trailing slash") << "/etc/" << "/etc";
        QTest::newRow("tilde") << "~/Documents" << "/home/ana/Documents";
        QTest::newRow("tilde, space in name") << "~/Documents/My report.odt" << "/home/ana/Documents/My report.odt";
        QTest::newRow("name in home") << "Documents" << "/home/ana/Documents";
        QTest::newRow("file in home") << "notes.txt" << "/home/ana/notes.txt";
        QTest::newRow("dot slash") << "./notes.txt" << "/home/ana/notes.txt";
        QTest::newRow("windows program") << "~/Downloads/setup.exe" << "/home/ana/Downloads/setup.exe";
        QTest::newRow("quoted, from Browse") << "'/home/ana/Documents/My report.odt'" << "/home/ana/Documents/My report.odt";
    }

    void paths()
    {
        QFETCH(QString, text);
        QFETCH(QString, path);
        const RunTarget t = RunParser::parse(text, fake());
        QCOMPARE(t.kind, Kind::Path);
        QCOMPARE(t.url, QUrl::fromLocalFile(path));
    }

    void missingPath()
    {
        const RunTarget t = RunParser::parse(QStringLiteral("/nowhere/at/all"), fake());
        QCOMPARE(t.kind, Kind::NotFound);
        QCOMPARE(t.name, QStringLiteral("/nowhere/at/all"));
    }

    void commands_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("program");
        QTest::addColumn<QStringList>("args");
        QTest::newRow("program") << "firefox" << "/usr/bin/firefox" << QStringList();
        QTest::newRow("program and url") << "firefox https://example.com"
                                         << "/usr/bin/firefox" << QStringList{QStringLiteral("https://example.com")};
        QTest::newRow("quoted argument") << "kate \"My report.odt\" --line 3" << "/usr/bin/kate"
                                         << QStringList{QStringLiteral("My report.odt"), QStringLiteral("--line"), QStringLiteral("3")};
        QTest::newRow("tilde argument") << "kate ~/notes.txt" << "/usr/bin/kate" << QStringList{QStringLiteral("/home/ana/notes.txt")};
        QTest::newRow("executable path") << "/opt/game/run" << "/opt/game/run" << QStringList();
        QTest::newRow("executable path, args") << "/opt/game/run --windowed" << "/opt/game/run" << QStringList{QStringLiteral("--windowed")};
        QTest::newRow("script in home") << "~/bin/tool.sh" << "/home/ana/bin/tool.sh" << QStringList();
        QTest::newRow("program beats home name") << "Documents-tool" << "/usr/bin/Documents-tool" << QStringList();
    }

    void commands()
    {
        QFETCH(QString, text);
        QFETCH(QString, program);
        QFETCH(QStringList, args);
        const RunTarget t = RunParser::parse(text, fake());
        QCOMPARE(t.kind, Kind::Command);
        QCOMPARE(t.program, program);
        QCOMPARE(t.args, args);
    }

    void shellLines_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("pipe") << "ls | sort";
        QTest::newRow("redirection") << "ls > /tmp/list";
        QTest::newRow("variable") << "kate $HOME/notes.txt";
        QTest::newRow("and") << "ls && kate";
    }

    void shellLines()
    {
        QFETCH(QString, text);
        const RunTarget t = RunParser::parse(text, fake());
        QCOMPARE(t.kind, Kind::ShellCommand);
        QCOMPARE(t.text, text);
    }

    void windowsNames_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("desktopId");
        QTest::newRow("cmd") << "cmd" << "org.kde.konsole";
        QTest::newRow("cmd.exe, any case") << "CMD.EXE" << "org.kde.konsole";
        QTest::newRow("notepad falls back") << "notepad" << "org.kde.kwrite";
        QTest::newRow("control") << "control" << "org.raye.settings";
        QTest::newRow("explorer") << "explorer.exe" << "org.kde.dolphin";
        QTest::newRow("desktop id") << "org.kde.gwenview" << "org.kde.gwenview";
    }

    void windowsNames()
    {
        QFETCH(QString, text);
        QFETCH(QString, desktopId);
        const RunTarget t = RunParser::parse(text, fake());
        QCOMPARE(t.kind, Kind::Application);
        QCOMPARE(t.desktopId, desktopId);
    }

    void windowsLayerToolsGiveWay()
    {
        RunParser::Environment env = fake();
        const auto plain = env.findExecutable;
        env.findExecutable = [plain](const QString &name) {
            static const QStringList layer = {QStringLiteral("regedit"), QStringLiteral("notepad"), QStringLiteral("winecfg")};
            return layer.contains(name) ? QStringLiteral("/usr/bin/") + name : plain(name);
        };
        env.isWindowsShim = [](const QString &program) {
            return program.startsWith(QLatin1String("/usr/bin/regedit")) || program.startsWith(QLatin1String("/usr/bin/notepad"))
                || program == QLatin1String("/usr/bin/winecfg");
        };
        const auto apps = env.installedApp;
        env.installedApp = [apps](const QStringList &ids) {
            return ids.contains(QStringLiteral("org.raye.regedit")) ? QStringLiteral("org.raye.regedit") : apps(ids);
        };

        RunTarget t = RunParser::parse(QStringLiteral("regedit"), env);
        QCOMPARE(t.kind, Kind::Application);
        QCOMPARE(t.desktopId, QStringLiteral("org.raye.regedit"));
        t = RunParser::parse(QStringLiteral("notepad notes.txt"), env);
        QCOMPARE(t.kind, Kind::Application);
        QCOMPARE(t.desktopId, QStringLiteral("org.kde.kwrite"));
        // nothing of Raye's by that name: the program itself runs
        t = RunParser::parse(QStringLiteral("winecfg"), env);
        QCOMPARE(t.kind, Kind::Command);
        QCOMPARE(t.program, QStringLiteral("/usr/bin/winecfg"));
    }

    void windowsLayerPaths_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("layer");
        QTest::newRow("raye command") << "/usr/lib/raye/windows/raye-windows-command" << true;
        QTest::newRow("raye command link") << "/usr/lib/raye/windows/commands/notepad" << true;
        QTest::newRow("raye wine") << "/opt/raye-wine/bin/wine" << true;
        QTest::newRow("winehq") << "/opt/wine-stable/bin/wine" << true;
        QTest::newRow("debian wine") << "/usr/lib/wine/wine64" << true;
        QTest::newRow("kwrite") << "/usr/bin/kwrite" << false;
        QTest::newRow("raye tool") << "/usr/lib/raye/settings/raye-settings" << false;
        QTest::newRow("nothing") << "" << false;
    }

    void windowsLayerPaths()
    {
        QFETCH(QString, path);
        QFETCH(bool, layer);
        QCOMPARE(RunParser::isWindowsLayerPath(path), layer);
    }

    void windowsNameWithoutApp()
    {
        // calc has no app on this system: say so, do not guess
        const RunTarget t = RunParser::parse(QStringLiteral("calc"), fake());
        QCOMPARE(t.kind, Kind::NotFound);
        QCOMPARE(t.name, QStringLiteral("calc"));
    }

    void unknown()
    {
        const RunTarget t = RunParser::parse(QStringLiteral("frobnicate --now"), fake());
        QCOMPARE(t.kind, Kind::NotFound);
        QCOMPARE(t.name, QStringLiteral("frobnicate --now"));
        QCOMPARE(RunParser::parse(QStringLiteral("kate \"unclosed"), fake()).kind, Kind::NotFound);
    }

    // Control Panel and Settings names open their Raye Settings page
    void settingsPages_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("url");
        QTest::newRow("sound applet") << "mmsys.cpl" << "settings:system/sound";
        QTest::newRow("any case") << "NCPA.CPL" << "settings:network";
        QTest::newRow("programs") << "appwiz.cpl" << "settings:apps/installed";
        QTest::newRow("winver") << "winver" << "settings:system/about";
        QTest::newRow("ms-settings page") << "ms-settings:display" << "settings:system/display";
        QTest::newRow("wifi") << "ms-settings:network-wifi" << "settings:network/wifi";
        QTest::newRow("date and time") << "timedate.cpl" << "settings:time/date-time";
        QTest::newRow("control with an applet") << "control mmsys.cpl" << "settings:system/sound";
        QTest::newRow("control printers") << "control printers" << "settings:devices/printers";
        QTest::newRow("control mouse") << "control mouse" << "settings:devices/mouse";
    }

    void settingsPages()
    {
        QFETCH(QString, text);
        QFETCH(QString, url);
        const RunTarget t = RunParser::parse(text, fake());
        QCOMPARE(t.kind, Kind::Url);
        QCOMPARE(t.url, QUrl(url));
    }

    void settingsPageRayeLacks()
    {
        // a page Raye has no match for: Settings itself
        const RunTarget t = RunParser::parse(QStringLiteral("ms-settings:holographic"), fake());
        QCOMPARE(t.kind, Kind::Application);
        QCOMPARE(t.desktopId, QStringLiteral("org.raye.settings"));
    }

    void controlWordsOnlyAfterControl()
    {
        // "printers" alone is not the Control Panel word
        const RunTarget t = RunParser::parse(QStringLiteral("printers"), fake());
        QCOMPARE(t.kind, Kind::NotFound);
    }

    // %temp%, shell:downloads...: the user's folder, even when its real
    // name is in another language
    void windowsFolders_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("path");
        QTest::newRow("temp") << "%temp%" << "/tmp";
        QTest::newRow("TMP, any case") << "%TMP%" << "/tmp";
        QTest::newRow("profile") << "%USERPROFILE%" << "/home/ana";
        QTest::newRow("downloads") << "shell:downloads" << "/home/ana/Téléchargements";
        QTest::newRow("with a backslash") << "shell:Downloads\\old" << "/home/ana/Téléchargements/old";
        QTest::newRow("my documents") << "shell:personal" << "/home/ana/Documents";
    }

    void windowsFolders()
    {
        QFETCH(QString, text);
        QFETCH(QString, path);
        const RunTarget t = RunParser::parse(text, fake());
        QCOMPARE(t.kind, Kind::Path);
        QCOMPARE(t.url, QUrl::fromLocalFile(path));
    }

    void windowsFolderMissing()
    {
        // known name, no such folder here
        QCOMPARE(RunParser::parse(QStringLiteral("shell:startup"), fake()).kind, Kind::NotFound);
        QCOMPARE(RunParser::parse(QStringLiteral("%temp%/nope"), fake()).kind, Kind::NotFound);
    }

    void windowsNameWithFiles()
    {
        RunTarget t = RunParser::parse(QStringLiteral("notepad notes.txt"), fake());
        QCOMPARE(t.kind, Kind::Application);
        QCOMPARE(t.desktopId, QStringLiteral("org.kde.kwrite"));
        QCOMPARE(t.urls, QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/home/ana/notes.txt"))});

        t = RunParser::parse(QStringLiteral("explorer ~/Documents"), fake());
        QCOMPARE(t.kind, Kind::Application);
        QCOMPARE(t.desktopId, QStringLiteral("org.kde.dolphin"));
        QCOMPARE(t.urls, QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/home/ana/Documents"))});

        // a Windows folder name as the argument
        t = RunParser::parse(QStringLiteral("explorer %temp%"), fake());
        QCOMPARE(t.kind, Kind::Application);
        QCOMPARE(t.urls, QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/tmp"))});

        // switches are not files
        QCOMPARE(RunParser::parse(QStringLiteral("cmd /c dir"), fake()).kind, Kind::NotFound);
        QCOMPARE(RunParser::parse(QStringLiteral("powershell -noexit"), fake()).kind, Kind::NotFound);

        // a real program of that name keeps its arguments as they are
        t = RunParser::parse(QStringLiteral("kate notes.txt"), fake());
        QCOMPARE(t.kind, Kind::Command);
        QCOMPARE(t.args, QStringList{QStringLiteral("notes.txt")});
    }

    // Browse: a path with spaces comes back quoted, and the parser reads
    // it back as that one file
    void browsedPaths()
    {
        QCOMPARE(RunParser::textForPath(QStringLiteral("/opt/game/run")), QStringLiteral("/opt/game/run"));
        const QString spaced = RunParser::textForPath(QStringLiteral("/home/ana/Documents/My report.odt"));
        QVERIFY(spaced != QLatin1String("/home/ana/Documents/My report.odt"));
        const RunTarget t = RunParser::parse(spaced, fake());
        QCOMPARE(t.kind, Kind::Path);
        QCOMPARE(t.url, QUrl::fromLocalFile(QStringLiteral("/home/ana/Documents/My report.odt")));
        QCOMPARE(RunParser::parse(RunParser::textForPath(QStringLiteral("/opt/game/run")), fake()).kind, Kind::Command);
    }

    void aliasTable()
    {
        QCOMPARE(RunParser::windowsAlias(QStringLiteral("taskmgr")).first(), QStringLiteral("org.raye.taskmanager"));
        // control opens the Control Panel, and Settings where it is not installed
        QCOMPARE(RunParser::windowsAlias(QStringLiteral("control")),
                 (QStringList{QStringLiteral("org.raye.controlpanel"), QStringLiteral("org.raye.settings")}));
        QCOMPARE(RunParser::windowsAlias(QStringLiteral("ms-settings:")), QStringList{QStringLiteral("org.raye.settings")});
        QCOMPARE(RunParser::windowsAlias(QStringLiteral("regedit")).first(), QStringLiteral("org.raye.regedit"));
        QCOMPARE(RunParser::windowsAlias(QStringLiteral("regedt32")).first(), QStringLiteral("org.raye.regedit"));
        QVERIFY(RunParser::windowsAlias(QStringLiteral("firefox")).isEmpty());
        QVERIFY(RunParser::windowsAlias(QStringLiteral(".exe")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestRunParser)
#include "tst_runparser.moc"
