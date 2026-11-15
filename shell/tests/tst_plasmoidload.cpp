// The load test of the shell: every applet of shell/plasmoids/ must pass
// the static checks, then load in plasmawindowed against the installed
// Plasma, in English and in French, without one QML warning of its own.
// It runs in the package build, so a Plasma upgrade that breaks one of
// the private modules an applet uses cannot ship unnoticed.

#include "plasmoidcheck.h"

#include <QDir>
#include <QFile>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QTest>

using namespace RayeShellCheck;

namespace
{

QStringList appletIds()
{
    QStringList ids = QStringLiteral(RAYE_SHELL_PLASMOID_IDS).split(QLatin1Char(','), Qt::SkipEmptyParts);
    ids.sort();
    return ids;
}

QString appletDir(const QString &id)
{
    return QStringLiteral(RAYE_SHELL_PLASMOIDS_DIR "/") + id;
}

int runMilliseconds()
{
    bool ok = false;
    const int ms = qEnvironmentVariableIntValue("RAYE_SHELL_LOAD_MS", &ok);
    return ok && ms > 0 ? ms : 4000;
}

} // namespace

class PlasmoidLoadTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_data;
    QQmlEngine *m_engine = nullptr;

    void addRows()
    {
        QTest::addColumn<QString>("id");
        const QStringList ids = appletIds();
        if (ids.isEmpty()) {
            QTest::newRow("no applet yet") << QString();
        }
        for (const QString &id : ids) {
            QTest::newRow(qPrintable(id)) << id;
        }
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_data.isValid());
        // Applets made of QML only are not installed yet when the tests
        // run: give Plasma a data folder where they sit under their id.
        const QString plasmoids = m_data.path() + QStringLiteral("/plasma/plasmoids");
        QVERIFY(QDir().mkpath(plasmoids));
        for (const QString &id : appletIds()) {
            if (!isCompiledApplet(appletDir(id))) {
                QVERIFY(QFile::link(appletDir(id), plasmoids + QLatin1Char('/') + id));
            }
        }
        m_engine = new QQmlEngine(this);
        m_engine->addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        m_engine->addImportPath(QStringLiteral(RAYE_SHELL_BUILD_DIR "/qml"));
    }

    void staticChecks_data()
    {
        addRows();
    }

    void staticChecks()
    {
        QFETCH(QString, id);
        if (id.isEmpty()) {
            QSKIP("shell/plasmoids has no applet yet");
        }
        const QList<Problem> problems = checkApplet(appletDir(id), QStringLiteral(RAYE_SHELL_PO_DIR), m_engine);
        QVERIFY2(problems.isEmpty(), qPrintable(QStringLiteral("\n") + describe(problems)));
    }

    void loads_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("language");
        const QStringList ids = appletIds();
        if (ids.isEmpty()) {
            QTest::newRow("no applet yet") << QString() << QString();
        }
        for (const QString &id : ids) {
            QTest::newRow(qPrintable(id + QStringLiteral(" en"))) << id << QString();
            QTest::newRow(qPrintable(id + QStringLiteral(" fr"))) << id << QStringLiteral("fr");
        }
    }

    void loads()
    {
        QFETCH(QString, id);
        QFETCH(QString, language);
        if (id.isEmpty()) {
            QSKIP("shell/plasmoids has no applet yet");
        }
        RunOptions options;
        // the build folder holds the compiled translations (locale/)
        options.dataDirs = {m_data.path(), QStringLiteral(RAYE_SHELL_BUILD_DIR)};
        options.pluginDirs = {QStringLiteral(RAYE_SHELL_BUILD_DIR "/bin")};
        options.qmlDirs = {QStringLiteral(RAYE_SHELL_BUILD_DIR "/qml"), QStringLiteral(RAYE_THEME_QML_DIR)};
        options.language = language;
        options.milliseconds = runMilliseconds();

        const RunResult result = runApplet(id, options);
        const QList<Problem> problems = checkRun(result, id);
        QVERIFY2(problems.isEmpty(), qPrintable(QStringLiteral("\n") + describe(problems) + QStringLiteral("\n--- what plasmawindowed printed:\n") + result.log));
    }
};

QTEST_MAIN(PlasmoidLoadTest)

#include "tst_plasmoidload.moc"
