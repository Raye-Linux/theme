#include "appinfo.h"

#include <QTest>

using namespace AppInfo;

class TestAppInfo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void desktopIdFromCgroup_data()
    {
        QTest::addColumn<QString>("cgroup");
        QTest::addColumn<QString>("id");

        QTest::newRow("scope") << "0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-org.kde.konsole-12345.scope"
                               << "org.kde.konsole";
        QTest::newRow("escaped dash") << "0::/user.slice/user@1000.service/app.slice/app-raye\\x2dsettings-4021.scope\n"
                                      << "raye-settings";
        QTest::newRow("transient service") << "0::/user.slice/user@1000.service/app.slice/app-org.raye.Store@0f3a2b.service"
                                           << "org.raye.Store";
        QTest::newRow("launcher prefix") << "0::/user.slice/user@1000.service/app.slice/app-flatpak-org.mozilla.firefox-5521.scope"
                                         << "org.mozilla.firefox";
        QTest::newRow("random suffix") << "0::/user.slice/user@1000.service/app.slice/app-org.kde.dolphin-a1b2c3d4e5f6.scope"
                                       << "org.kde.dolphin";
        QTest::newRow("plasma service") << "0::/user.slice/user@1000.service/session.slice/plasma-plasmashell.service"
                                        << "";
        QTest::newRow("system") << "0::/system.slice/packagekit.service" << "";
        QTest::newRow("empty") << "" << "";
    }

    void desktopIdFromCgroup()
    {
        QFETCH(QString, cgroup);
        QFETCH(QString, id);
        QCOMPARE(AppInfo::desktopIdFromCgroup(cgroup), id);
    }

    void parseDesktopEntry()
    {
        const QString text = QStringLiteral(
            "# comment\n"
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Settings\n"
            "Name[fr]=Paramètres\n"
            "Name[fr_CA]=Réglages\n"
            "Icon=raye-settings\n"
            "Exec=env QT_X=1 /usr/bin/raye-settings %u\n"
            "\n"
            "[Desktop Action New]\n"
            "Name=Something else\n"
            "Exec=other\n");

        DesktopEntry e = AppInfo::parseDesktopEntry(text, QStringLiteral("en_US"));
        QCOMPARE(e.name, QStringLiteral("Settings"));
        QCOMPARE(e.icon, QStringLiteral("raye-settings"));
        QCOMPARE(e.program, QStringLiteral("raye-settings"));
        QVERIFY(!e.hidden);

        QCOMPARE(AppInfo::parseDesktopEntry(text, QStringLiteral("fr_FR")).name, QStringLiteral("Paramètres"));
        QCOMPARE(AppInfo::parseDesktopEntry(text, QStringLiteral("fr_CA")).name, QStringLiteral("Réglages"));
        QCOMPARE(AppInfo::parseDesktopEntry(text, QStringLiteral("fr_CA.UTF-8")).name, QStringLiteral("Réglages"));

        e = AppInfo::parseDesktopEntry(QStringLiteral("[Desktop Entry]\nName=X\nNoDisplay=true\nExec=x\n"), QString());
        QVERIFY(e.hidden);
    }

    void execProgram_data()
    {
        QTest::addColumn<QString>("exec");
        QTest::addColumn<QString>("program");

        QTest::newRow("plain") << "konsole" << "konsole";
        QTest::newRow("path and codes") << "/usr/bin/raye-store %U" << "raye-store";
        QTest::newRow("env") << "env LANG=C FOO=bar /opt/app/bin/app --x" << "app";
        QTest::newRow("quoted") << "\"/opt/My App/app\" %f" << "app";
        QTest::newRow("flatpak") << "/usr/bin/flatpak run --branch=stable --arch=x86_64 --command=firefox org.mozilla.firefox @@u %u @@"
                                 << "org.mozilla.firefox";
        QTest::newRow("empty") << "" << "";
    }

    void execProgram()
    {
        QFETCH(QString, exec);
        QFETCH(QString, program);
        QCOMPARE(AppInfo::execProgram(exec), program);
    }

    void packageFromSearch()
    {
        QCOMPARE(AppInfo::packageFromSearch(QStringLiteral("coreutils: /usr/bin/true\n")), QStringLiteral("coreutils"));
        QCOMPARE(AppInfo::packageFromSearch(QStringLiteral("libfoo1:amd64: /usr/lib/x86_64-linux-gnu/libfoo.so.1\n")),
                 QStringLiteral("libfoo1"));
        QCOMPARE(AppInfo::packageFromSearch(QStringLiteral("diversion by raye-shell from: /usr/x\n"
                                                           "diversion by raye-shell to: /usr/x.stock\n"
                                                           "plasma-workspace, raye-shell: /usr/x\n")),
                 QStringLiteral("plasma-workspace"));
        QCOMPARE(AppInfo::packageFromSearch(QString()), QString());
    }

    void installedSources()
    {
        const QString policy = QStringLiteral(
            "firefox:\n"
            "  Installed: 1:146.0\n"
            "  Candidate: 1:146.0\n"
            "  Version table:\n"
            " *** 1:146.0 1000\n"
            "       1000 https://packages.mozilla.org/apt mozilla/main amd64 Packages\n"
            "        100 /var/lib/dpkg/status\n"
            "     1:1snap1-0ubuntu7 500\n"
            "        500 http://archive.ubuntu.com/ubuntu resolute/main amd64 Packages\n");
        QCOMPARE(AppInfo::installedSources(policy), QStringList{QStringLiteral("https://packages.mozilla.org/apt mozilla")});

        const QString local = QStringLiteral(
            "raye-settings:\n"
            "  Installed: 0.1\n"
            "  Version table:\n"
            " *** 0.1 100\n"
            "        100 /var/lib/dpkg/status\n");
        QVERIFY(AppInfo::installedSources(local).isEmpty());
        QVERIFY(AppInfo::installedSources(QString()).isEmpty());
    }

    void releaseFileName()
    {
        QCOMPARE(AppInfo::releaseFileName(QStringLiteral("https://packages.mozilla.org/apt"), QStringLiteral("mozilla")),
                 QStringLiteral("packages.mozilla.org_apt_dists_mozilla_InRelease"));
        QCOMPARE(AppInfo::releaseFileName(QStringLiteral("http://archive.ubuntu.com/ubuntu/"), QStringLiteral("resolute-updates")),
                 QStringLiteral("archive.ubuntu.com_ubuntu_dists_resolute-updates_InRelease"));
    }

    void releaseOrigin()
    {
        QCOMPARE(AppInfo::releaseOrigin(QStringLiteral("-----BEGIN PGP SIGNED MESSAGE-----\nHash: SHA512\n\nOrigin: mozilla\nLabel: x\n")),
                 QStringLiteral("mozilla"));
        QCOMPARE(AppInfo::releaseOrigin(QStringLiteral("Label: x\n")), QString());
    }

    void publisherFor_data()
    {
        QTest::addColumn<QString>("package");
        QTest::addColumn<QStringList>("sources");
        QTest::addColumn<QStringList>("origins");
        QTest::addColumn<QString>("maintainer");
        QTest::addColumn<QString>("publisher");

        const QString ubuntu = QStringLiteral("Ubuntu Developers <ubuntu-devel-discuss@lists.ubuntu.com>");
        const QString raye = QStringLiteral("techoraye <190692880+techoraye@users.noreply.github.com>");

        QTest::newRow("no package") << "" << QStringList() << QStringList() << "" << "";
        QTest::newRow("ubuntu archive") << "coreutils" << QStringList{"http://archive.ubuntu.com/ubuntu resolute"}
                                        << QStringList{"Ubuntu"} << ubuntu << "Raye Linux";
        QTest::newRow("ubuntu mirror") << "coreutils" << QStringList{"http://ca.archive.ubuntu.com/ubuntu resolute"}
                                       << QStringList{"Ubuntu"} << ubuntu << "Raye Linux";
        QTest::newRow("raye archive") << "raye-store" << QStringList{"https://dl.rayelinux.com/apt resolute"}
                                      << QStringList{"Raye"} << raye << "Raye Linux";
        QTest::newRow("raye second mirror") << "raye-store" << QStringList{"https://raye-linux.github.io/packages resolute"}
                                            << QStringList{""} << raye << "Raye Linux";
        QTest::newRow("other archive") << "firefox" << QStringList{"https://packages.mozilla.org/apt mozilla"}
                                       << QStringList{"mozilla"} << "Mozilla <x@mozilla.com>" << "Mozilla";
        QTest::newRow("other archive, no origin") << "foo" << QStringList{"https://repo.example.org/deb stable"}
                                                  << QStringList{""} << "x" << "repo.example.org";
        QTest::newRow("raye, from the image") << "raye-settings" << QStringList() << QStringList() << raye << "Raye Linux";
        QTest::newRow("ubuntu, from the image") << "plasma-workspace" << QStringList() << QStringList()
                                                << "Kubuntu Developers <kubuntu-devel@lists.ubuntu.com>" << "Raye Linux";
        QTest::newRow("deb installed by hand") << "someapp" << QStringList() << QStringList()
                                               << "Some Vendor <deb@vendor.example>" << "";
    }

    void publisherFor()
    {
        QFETCH(QString, package);
        QFETCH(QStringList, sources);
        QFETCH(QStringList, origins);
        QFETCH(QString, maintainer);
        QFETCH(QString, publisher);
        QCOMPARE(AppInfo::publisherFor(package, sources, origins, maintainer), publisher);
    }

    void pkexecCommand()
    {
        QCOMPARE(AppInfo::pkexecCommand({"pkexec", "true"}), QStringList{"true"});
        QCOMPARE(AppInfo::pkexecCommand({"/usr/bin/pkexec", "--disable-internal-agent", "/usr/libexec/raye/firewall", "enable"}),
                 (QStringList{"/usr/libexec/raye/firewall", "enable"}));
        QCOMPARE(AppInfo::pkexecCommand({"pkexec", "--user", "root", "--keep-cwd", "apt", "install", "x"}),
                 (QStringList{"apt", "install", "x"}));
        QCOMPARE(AppInfo::pkexecCommand({"pkexec", "-u", "root", "id"}), QStringList{"id"});
        QVERIFY(AppInfo::pkexecCommand({"bash", "-c", "x"}).isEmpty());
        QVERIFY(AppInfo::pkexecCommand({}).isEmpty());
    }

    void isInterpreter()
    {
        QVERIFY(AppInfo::isInterpreter(QStringLiteral("python3")));
        QVERIFY(AppInfo::isInterpreter(QStringLiteral("python3.14")));
        QVERIFY(AppInfo::isInterpreter(QStringLiteral("bash")));
        QVERIFY(AppInfo::isInterpreter(QStringLiteral("perl")));
        QVERIFY(!AppInfo::isInterpreter(QStringLiteral("raye-store")));
        QVERIFY(!AppInfo::isInterpreter(QStringLiteral("pythonista")));
    }

    void resolveSelf()
    {
        // the test itself: no desktop file, but a program and a name
        const Caller c = AppInfo::resolve(QCoreApplication::applicationPid(), QString(), QString());
        QVERIFY(!c.name.isEmpty());
        QVERIFY(c.program.endsWith(QLatin1String("tst_appinfo")));
        QVERIFY(c.publisher.isEmpty());
    }

    void resolvePkexecProgram()
    {
        const Caller c = AppInfo::resolve(0, QStringLiteral("/no/such/program"), QStringLiteral("/no/such/program --flag"));
        QCOMPARE(c.program, QStringLiteral("/no/such/program"));
        QCOMPARE(c.command, QStringLiteral("/no/such/program --flag"));
        QCOMPARE(c.name, QStringLiteral("program"));
        QVERIFY(c.publisher.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAppInfo)
#include "tst_appinfo.moc"
