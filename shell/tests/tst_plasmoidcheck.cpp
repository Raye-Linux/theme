// Tests of the checks themselves, on the applets in fixtures/: each one
// is broken in one known way, and the checks must find exactly that.

#include "plasmoidcheck.h"

#include <QDir>
#include <QFile>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QTest>

using namespace RayeShellCheck;

namespace
{

QString fixtures()
{
    return QStringLiteral(RAYE_SHELL_FIXTURES_DIR);
}

QString applet(const QString &name)
{
    return fixtures() + QStringLiteral("/plasma/plasmoids/org.raye.fixture.") + name;
}

QString poDir()
{
    return fixtures() + QStringLiteral("/po");
}

QString readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool mentions(const QList<Problem> &problems, const QString &text, int line = -1)
{
    for (const Problem &p : problems) {
        if (p.message.contains(text, Qt::CaseInsensitive) && (line < 0 || p.line == line)) {
            return true;
        }
    }
    return false;
}

RunOptions fixtureRun()
{
    RunOptions options;
    options.dataDirs = {fixtures()};
    options.milliseconds = 3000;
    return options;
}

} // namespace

class PlasmoidCheckTest : public QObject
{
    Q_OBJECT

private:
    QQmlEngine *m_engine = nullptr;

private Q_SLOTS:
    void initTestCase()
    {
        m_engine = new QQmlEngine(this);
    }

    // --- the i18n scanner ---------------------------------------------

    void scanFindsEveryForm()
    {
        const QString source = QStringLiteral(
            "Text { text: i18nd(\"raye_shell_a\", \"Open\") }\n"
            "Text { text: i18ndc(\"raye_shell_a\", \"menu\", 'Close %1', name) }\n"
            "Text { text: i18ndp(\"raye_shell_a\", \"%1 app\", \"%1 apps\", n) }\n"
            "Text { text: i18ndcp(\"raye_shell_a\", \"ctx\", \"%1 file\", \"%1 files\", n) }\n"
            "Text { text: xi18nd(\"raye_shell_a\", \"<b>Bold</b>\") }\n"
            "Text { text: i18n(\"Bare\") }\n");
        const QList<I18nCall> calls = scanI18nCalls(source);
        QCOMPARE(calls.size(), 6);

        QCOMPARE(calls[0].function, QStringLiteral("i18nd"));
        QCOMPARE(calls[0].line, 1);
        QVERIFY(calls[0].hasDomain && calls[0].domainLiteral && calls[0].textLiteral);
        QCOMPARE(calls[0].domain, QStringLiteral("raye_shell_a"));
        QCOMPARE(calls[0].text, QStringLiteral("Open"));

        QCOMPARE(calls[1].context, QStringLiteral("menu"));
        QCOMPARE(calls[1].text, QStringLiteral("Close %1"));
        QCOMPARE(calls[1].line, 2);

        QCOMPARE(calls[2].text, QStringLiteral("%1 app"));
        QCOMPARE(calls[2].plural, QStringLiteral("%1 apps"));

        QCOMPARE(calls[3].context, QStringLiteral("ctx"));
        QCOMPARE(calls[3].plural, QStringLiteral("%1 files"));

        QCOMPARE(calls[4].function, QStringLiteral("xi18nd"));
        QCOMPARE(calls[4].text, QStringLiteral("<b>Bold</b>"));

        QCOMPARE(calls[5].function, QStringLiteral("i18n"));
        QVERIFY(!calls[5].hasDomain);
        QCOMPARE(calls[5].text, QStringLiteral("Bare"));
    }

    void scanSkipsCommentsAndStrings()
    {
        const QString source = QStringLiteral(
            "// i18n(\"in a comment\")\n"
            "/* i18nd(\"raye_shell_a\", \"in a block\")\n"
            "   still the block */\n"
            "property string s: \"i18n('in a string')\"\n"
            "property string t: `i18n(\"in a template\")`\n"
            "function myi18n(x) { return x }\n"
            "property string u: myi18n(\"not ours\")\n"
            "property string v: obj.i18nd(\"raye_shell_a\", \"Member\")\n"
            "property string w: i18nd ( \"raye_shell_a\" , \"Spaced\" )\n");
        const QList<I18nCall> calls = scanI18nCalls(source);
        QCOMPARE(calls.size(), 1);
        QCOMPARE(calls[0].text, QStringLiteral("Spaced"));
        QCOMPARE(calls[0].line, 9);
    }

    void scanReadsEscapesAndJoinedLiterals()
    {
        const QString source = QStringLiteral(
            "i18nd(\"raye_shell_a\", \"Say \\\"hi\\\"\\n\")\n"
            "i18nd(\"raye_shell_a\", \"One \" +\n"
            "      \"line\")\n"
            "i18nd(\"raye_shell_a\", 'It\\'s')\n");
        const QList<I18nCall> calls = scanI18nCalls(source);
        QCOMPARE(calls.size(), 3);
        QCOMPARE(calls[0].text, QStringLiteral("Say \"hi\"\n"));
        QCOMPARE(calls[1].text, QStringLiteral("One line"));
        QVERIFY(calls[1].textLiteral);
        QCOMPARE(calls[2].text, QStringLiteral("It's"));
        QCOMPARE(calls[2].line, 4);
    }

    void scanMarksWhatIsNotLiteral()
    {
        const QString source = QStringLiteral(
            "i18nd(domain, \"Text\")\n"
            "i18nd(\"raye_shell_a\", model.label)\n"
            "i18nd(\"raye_shell_a\", \"A\" + name)\n");
        const QList<I18nCall> calls = scanI18nCalls(source);
        QCOMPARE(calls.size(), 3);
        QVERIFY(!calls[0].domainLiteral);
        QVERIFY(calls[0].textLiteral);
        QVERIFY(calls[1].domainLiteral);
        QVERIFY(!calls[1].textLiteral);
        QVERIFY(!calls[2].textLiteral);
    }

    void scanFindsQtTranslate()
    {
        const QString source = QStringLiteral(
            "Text { text: qsTr(\"Hello\") }\n"
            "Text { text: \"qsTr in a string\" }\n"
            "Text { text: qsTranslate(\"ctx\", \"Hi\") }\n");
        QCOMPARE(scanQtTranslateCalls(source), (QList<int>{1, 3}));
    }

    // --- po files ------------------------------------------------------

    void parsePoReadsEntries()
    {
        QString error;
        const auto entries = parsePo(readAll(poDir() + QStringLiteral("/fr/raye_shell_fixture.po")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(entries.size(), 5);

        const PoEntry passed = entries.value(poKey({}, QStringLiteral("Load test passed")));
        QCOMPARE(passed.msgstr, QStringList{QStringLiteral("Test de chargement réussi")});
        QVERIFY(!passed.fuzzy);

        const PoEntry ran = entries.value(poKey({}, QStringLiteral("%1 check ran")));
        QCOMPARE(ran.plural, QStringLiteral("%1 checks ran"));
        QCOMPARE(ran.msgstr.size(), 2);
        QCOMPARE(ran.msgstr[1], QStringLiteral("%1 vérifications faites"));

        QVERIFY(entries.contains(poKey(QStringLiteral("button that closes the test"), QStringLiteral("Close"))));
        QVERIFY(!entries.contains(poKey({}, QStringLiteral("Close"))));
        QVERIFY(entries.value(poKey({}, QStringLiteral("Only a guess"))).fuzzy);
        QVERIFY(entries.value(poKey({}, QStringLiteral("Never translated"))).msgstr.value(0).isEmpty());
        QVERIFY(!entries.contains(poKey({}, QStringLiteral("Old string"))));
        QVERIFY(!entries.contains(poKey({}, QString())));
    }

    void parsePoJoinsLinesAndEscapes()
    {
        const QString text = QStringLiteral(
            "msgid \"\"\nmsgstr \"Language: fr\\n\"\n\n"
            "msgid \"\"\n\"Two \"\n\"parts\"\nmsgstr \"Deux \"\n\"parties\\t\\\"x\\\"\"\n");
        QString error;
        const auto entries = parsePo(text, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.value(poKey({}, QStringLiteral("Two parts"))).msgstr.value(0), QStringLiteral("Deux parties\t\"x\""));
    }

    void parsePoReportsGarbage()
    {
        QString error;
        parsePo(QStringLiteral("msgid \"a\"\nmsgstr \"b\"\nnonsense here\n"), &error);
        QVERIFY(error.contains(QStringLiteral("line 3")));
        parsePo(QStringLiteral("msgid \"a\nmsgstr \"b\"\n"), &error);
        QVERIFY(!error.isEmpty());
    }

    void placeholdersAreFound()
    {
        QCOMPARE(placeholders(QStringLiteral("%2 of %1, 100% done, %1 again, %10")), (QList<int>{1, 2, 10}));
        QCOMPARE(placeholders(QStringLiteral("no number")), QList<int>{});
    }

    // --- the applet checks ---------------------------------------------

    void goodAppletPasses()
    {
        const QList<Problem> problems = checkApplet(applet(QStringLiteral("good")), poDir(), m_engine);
        QVERIFY2(problems.isEmpty(), qPrintable(describe(problems)));
    }

    void badMetadataIsFound()
    {
        const QList<Problem> problems = checkMetadata(applet(QStringLiteral("badmeta")));
        QVERIFY2(mentions(problems, QStringLiteral("org.raye.fixture.other")), qPrintable(describe(problems)));
        for (const QString &key : {QStringLiteral("Name[fr]"),
                                   QStringLiteral("Description[fr]"),
                                   QStringLiteral("Icon"),
                                   QStringLiteral("License"),
                                   QStringLiteral("Authors"),
                                   QStringLiteral("Version"),
                                   QStringLiteral("X-Plasma-API-Minimum-Version"),
                                   QStringLiteral("contents/ui/main.qml")}) {
            QVERIFY2(mentions(problems, key), qPrintable(key + QStringLiteral(" not reported:\n") + describe(problems)));
        }
        QVERIFY(!mentions(problems, QStringLiteral("KPackageStructure")));
    }

    void metadataOfGoodHasNoProblem()
    {
        QVERIFY(checkMetadata(applet(QStringLiteral("good"))).isEmpty());
    }

    void metadataRejectsOtherNames()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + QStringLiteral("/org.kde.plasma.clock");
        QVERIFY(QDir().mkpath(path + QStringLiteral("/contents/ui")));
        QFile::copy(applet(QStringLiteral("good")) + QStringLiteral("/metadata.json"), path + QStringLiteral("/metadata.json"));
        QFile::copy(applet(QStringLiteral("good")) + QStringLiteral("/contents/ui/main.qml"), path + QStringLiteral("/contents/ui/main.qml"));
        QVERIFY(mentions(checkMetadata(path), QStringLiteral("org.raye.")));

        QFile broken(path + QStringLiteral("/metadata.json"));
        QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
        broken.write("{ \"KPlugin\": ");
        broken.close();
        QVERIFY(mentions(checkMetadata(path), QStringLiteral("JSON")));
    }

    void compiledAppletHasNoId()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + QStringLiteral("/org.raye.fixture.good");
        QVERIFY(QDir().mkpath(path));
        QVERIFY(QFile::copy(applet(QStringLiteral("good")) + QStringLiteral("/metadata.json"), path + QStringLiteral("/metadata.json")));
        QFile cmake(path + QStringLiteral("/CMakeLists.txt"));
        QVERIFY(cmake.open(QIODevice::WriteOnly));
        cmake.close();
        QVERIFY(isCompiledApplet(path));
        // no contents/ui/main.qml needed, but the Id must go
        QList<Problem> problems = checkMetadata(path);
        QVERIFY2(problems.size() == 1 && problems[0].message.contains(QStringLiteral("Id")), qPrintable(describe(problems)));

        QFile meta(path + QStringLiteral("/metadata.json"));
        QVERIFY(meta.open(QIODevice::ReadOnly));
        QByteArray json = meta.readAll();
        meta.close();
        json.replace("\"Id\": \"org.raye.fixture.good\",", "");
        QVERIFY(meta.open(QIODevice::WriteOnly | QIODevice::Truncate));
        meta.write(json);
        meta.close();
        problems = checkMetadata(path);
        QVERIFY2(problems.isEmpty(), qPrintable(describe(problems)));
    }

    void badConfigIsFound()
    {
        const QList<Problem> problems = checkConfigXml(applet(QStringLiteral("badmeta")));
        QCOMPARE(problems.size(), 1);
        QCOMPARE(problems[0].file, QStringLiteral("contents/config/main.xml"));
        QVERIFY(problems[0].line > 0);
        QVERIFY(checkConfigXml(applet(QStringLiteral("good"))).isEmpty());
    }

    void badSyntaxIsFound()
    {
        const QList<Problem> problems = compileQml(applet(QStringLiteral("badsyntax")), m_engine);
        QVERIFY2(!problems.isEmpty(), "a missing brace compiled");
        QCOMPARE(problems[0].file, QStringLiteral("contents/ui/main.qml"));
        QCOMPARE(problems[0].line, 9);
    }

    void everyMissingTranslationIsFound()
    {
        const QList<Problem> problems = checkTranslations(applet(QStringLiteral("badi18n")), poDir());
        QVERIFY2(problems.size() == 5, qPrintable(describe(problems)));
        QVERIFY(mentions(problems, QStringLiteral("domain"), 7)); // i18n() with no domain
        QVERIFY(mentions(problems, QStringLiteral("plasma_applet_other"), 8));
        QVERIFY(mentions(problems, QStringLiteral("not translated"), 9));
        QVERIFY(mentions(problems, QStringLiteral("fuzzy"), 10));
        QVERIFY(mentions(problems, QStringLiteral("raye_shell_nopo.po"), 11));
        for (const Problem &p : problems) {
            QCOMPARE(p.file, QStringLiteral("contents/ui/main.qml"));
        }
    }

    void placeholderMismatchIsFound()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + QStringLiteral("/org.raye.fixture.args");
        QVERIFY(QDir().mkpath(path + QStringLiteral("/contents/ui")));
        QVERIFY(QDir().mkpath(dir.path() + QStringLiteral("/po/fr")));
        QFile qml(path + QStringLiteral("/contents/ui/main.qml"));
        QVERIFY(qml.open(QIODevice::WriteOnly));
        qml.write("import QtQuick\nText { text: i18nd(\"raye_shell_args\", \"%1 of %2\", a, b) + i18nd(\"raye_shell_args\", \"Fine %1\", a) }\n");
        qml.close();
        QFile po(dir.path() + QStringLiteral("/po/fr/raye_shell_args.po"));
        QVERIFY(po.open(QIODevice::WriteOnly));
        po.write("msgid \"%1 of %2\"\nmsgstr \"%1 sur %3\"\n\nmsgid \"Fine %1\"\nmsgstr \"Bien %1\"\n");
        po.close();
        const QList<Problem> problems = checkTranslations(path, dir.path() + QStringLiteral("/po"));
        QVERIFY2(problems.size() == 1, qPrintable(describe(problems)));
        QVERIFY(problems[0].message.contains(QStringLiteral("%3")));
    }

    void runtimeErrorPassesTheStaticChecks()
    {
        const QList<Problem> problems = checkApplet(applet(QStringLiteral("runtimebad")), poDir(), m_engine);
        QVERIFY2(problems.isEmpty(), qPrintable(describe(problems)));
    }

    // --- the run log ---------------------------------------------------

    void runLogClassification_data()
    {
        QTest::addColumn<QString>("log");
        QTest::addColumn<int>("expected");

        const QString url = QStringLiteral("file:///x/plasma/plasmoids/org.raye.demo/contents/ui/main.qml");
        QTest::newRow("clean") << QStringLiteral(
            "Loading default layout\nkf.windowsystem: Could not find any platform plugin\nqml: 0\n"
            "This plugin does not support propagateSizeHints()\n") << 0;
        QTest::newRow("reference error") << QString(url + QStringLiteral(":6: ReferenceError: missingThing is not defined\n")) << 1;
        QTest::newRow("same error twice counts once") << QString(url + QStringLiteral(":6: TypeError: x\n") + url + QStringLiteral(":6: TypeError: x\n")) << 1;
        QTest::newRow("load error") << QStringLiteral(
            "QQmlComponent: Component is not ready\n"
            "kf.plasma.quick: error when loading applet \"org.raye.demo\" QList(\"file:///x/main.qml:9:1: Expected token `}'\")\n") << 1;
        QTest::newRow("missing package") << QStringLiteral(
            "kf.plasma.quick: error when loading applet \"org.raye.demo\" QList(\"Error loading Applet: package org.raye.demo does not exist.\")\n") << 1;
        QTest::newRow("missing main script") << QStringLiteral(
            "kf.package: Could not find required file \"mainscript\" for package \"/x/plasma/plasmoids/org.raye.demo/\" should be QList(\"ui/main.qml\")\n") << 1;
        QTest::newRow("compiled applet in qrc") << QStringLiteral(
            "qrc:/qt/qml/plasma/applet/org/raye/demo/main.qml:12:5: Unable to assign [undefined] to int\n") << 1;
        QTest::newRow("binding loop") << QString(url + QStringLiteral(":20:9: QML Item: Binding loop detected for property \"width\"\n")) << 1;
        QTest::newRow("other applet is not ours") << QStringLiteral(
            "file:///usr/share/plasma/plasmoids/org.kde.plasma.clock/contents/ui/main.qml:3: TypeError: y\n"
            "kf.plasma.quick: error when loading applet \"org.raye.demotwo\" QList(\"bad\")\n") << 0;
        QTest::newRow("stock module warning about us") << QStringLiteral(
            "file:///usr/lib/qt6/qml/org/kde/plasma/components/Label.qml:4: TypeError: from org.raye.demo\n") << 0;
    }

    void runLogClassification()
    {
        QFETCH(QString, log);
        QFETCH(int, expected);
        const QList<Problem> problems = classifyRunLog(log, QStringLiteral("org.raye.demo"));
        QVERIFY2(problems.size() == expected, qPrintable(describe(problems)));
    }

    void runLogKeepsTheLine()
    {
        const QList<Problem> problems = classifyRunLog(
            QStringLiteral("file:///x/plasma/plasmoids/org.raye.demo/contents/ui/Item.qml:14:3: ReferenceError: a is not defined\n"),
            QStringLiteral("org.raye.demo"));
        QCOMPARE(problems.size(), 1);
        QCOMPARE(problems[0].file, QStringLiteral("contents/ui/Item.qml"));
        QCOMPARE(problems[0].line, 14);
    }

    // --- a real run of plasmawindowed ------------------------------------

    void realRunOfGoodIsClean()
    {
        const RunResult result = runApplet(QStringLiteral("org.raye.fixture.good"), fixtureRun());
        QVERIFY2(result.started, qPrintable(result.log));
        const QList<Problem> problems = checkRun(result, QStringLiteral("org.raye.fixture.good"));
        QVERIFY2(problems.isEmpty(), qPrintable(describe(problems) + QStringLiteral("\n--- log:\n") + result.log));
    }

    void realRunFindsTheRuntimeError()
    {
        const RunResult result = runApplet(QStringLiteral("org.raye.fixture.runtimebad"), fixtureRun());
        QVERIFY2(result.started, qPrintable(result.log));
        const QList<Problem> problems = checkRun(result, QStringLiteral("org.raye.fixture.runtimebad"));
        QVERIFY2(mentions(problems, QStringLiteral("missingThing"), 6), qPrintable(result.log));
    }

    void realRunFindsAMissingApplet()
    {
        const RunResult result = runApplet(QStringLiteral("org.raye.fixture.nothere"), fixtureRun());
        QVERIFY2(result.started, qPrintable(result.log));
        QVERIFY2(!checkRun(result, QStringLiteral("org.raye.fixture.nothere")).isEmpty(), qPrintable(result.log));
    }

    void earlyExitIsAProblem()
    {
        RunResult result;
        result.started = true;
        result.exitedEarly = true;
        result.exitCode = 11;
        QVERIFY(mentions(checkRun(result, QStringLiteral("org.raye.demo")), QStringLiteral("stopped")));
    }
};

QTEST_MAIN(PlasmoidCheckTest)

#include "tst_plasmoidcheck.moc"
