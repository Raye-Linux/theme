// UpdateState: reading Raye Update's state file (the fixtures are the
// ones of the update repo, copied), following it when it is replaced, and
// "Update and restart" / "Update and shut down".
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "updatestate.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <grp.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

static QByteArray fixture(const char *name)
{
    QFile f(QStringLiteral(RAYE_STATE_FIXTURES) + u'/' + QLatin1String(name) + u".json"_s);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("missing fixture %s", name);
    return f.readAll();
}

// the file is always replaced, never written in place
static void replaceFile(const QString &path, const QByteArray &data)
{
    QFile f(path + u".new"_s);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
    f.close();
    QFile::remove(path);
    QVERIFY(QFile::rename(path + u".new"_s, path));
}

class TestUpdateState : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    // a stand-in for raye-update: records its arguments, exits with the code
    QString fakeTool(int code)
    {
        const QString path = m_dir.filePath(u"raye-update-%1"_s.arg(code));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("cannot write the fake tool");
        f.write("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"" + m_dir.filePath(u"args"_s).toUtf8()
                + "\"\nexit " + QByteArray::number(code) + "\n");
        f.close();
        f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return path;
    }

    QStringList lastArgs()
    {
        QFile f(m_dir.filePath(u"args"_s));
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(f.readAll()).split(u'\n', Qt::SkipEmptyParts);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    void idle()
    {
        const UpdateInfo i = UpdateInfo::parse(fixture("idle"));
        QCOMPARE(i.phase, u"idle"_s);
        QCOMPARE(i.count, 0);
        QVERIFY(!i.needsRestart);
        QVERIFY(!i.canArm());
    }

    void ready()
    {
        const UpdateInfo i = UpdateInfo::parse(fixture("ready"));
        QCOMPARE(i.phase, u"ready"_s);
        QCOMPARE(i.count, 3);
        QVERIFY(i.needsRestart);
        QVERIFY(i.kernelChange);
        QCOMPARE(i.readyId, u"5f0c2a9e41b7"_s);
        QCOMPARE(i.firmwareCount, 1);
        QCOMPARE(i.pendingNames.size(), 3);
        QVERIFY(!i.armed);
        QVERIFY(i.canArm());
        QVERIFY(i.restartPending());
        QCOMPARE(i.lastCheck, 1793462520);
    }

    void armed()
    {
        const UpdateInfo i = UpdateInfo::parse(fixture("armed"));
        QCOMPARE(i.phase, u"armed"_s);
        QVERIFY(i.armed);
        QVERIFY(i.armedThen == u"restart"_s || i.armedThen == u"shutdown"_s);
        QVERIFY(!i.canArm());
        QVERIFY(i.restartPending());
        const UpdateInfo s = UpdateInfo::parse(fixture("armed-scheduled"));
        QVERIFY(s.armed);
        QVERIFY(s.armedAt > 0);
    }

    void downloading()
    {
        const UpdateInfo i = UpdateInfo::parse(fixture("downloading"));
        QCOMPARE(i.phase, u"downloading"_s);
        QVERIFY(i.totalBytes > 0);
        QVERIFY(i.downloadedBytes > 0 && i.downloadedBytes <= i.totalBytes);
        QVERIFY(i.progress() > 0.0 && i.progress() <= 1.0);
        QVERIFY(!i.canArm());
    }

    void otherPhases()
    {
        QCOMPARE(UpdateInfo::parse(fixture("checking")).phase, u"checking"_s);
        const UpdateInfo p = UpdateInfo::parse(fixture("paused"));
        QCOMPARE(p.phase, u"paused"_s);
        QVERIFY(p.pausedUntil > 0);
        const UpdateInfo w = UpdateInfo::parse(fixture("waiting-game"));
        QCOMPARE(w.phase, u"waiting"_s);
        QCOMPARE(w.waitReason, u"game"_s);
        const UpdateInfo e = UpdateInfo::parse(fixture("error"));
        QCOMPARE(e.phase, u"error"_s);
        QVERIFY(!e.errorCode.isEmpty());
        QCOMPARE(UpdateInfo::parse(fixture("install-failed")).errorCode, u"install-failed"_s);
    }

    // a file written by a Raye Update older than schema version 2
    void versionOne()
    {
        const UpdateInfo i = UpdateInfo::parse(fixture("v1-ready"));
        QCOMPARE(i.phase, u"ready"_s);
        QCOMPARE(i.count, 2);
        QVERIFY(i.needsRestart);
        QCOMPARE(i.readyId, u"1793462520-2"_s);
        QVERIFY(i.canArm());
    }

    void unreadableMeansIdle()
    {
        for (const QByteArray &bad : {QByteArray(), QByteArray("not json"), QByteArray("[1,2]"),
                                      QByteArray(R"({"version":2,"phase":"exploding","count":-4})"),
                                      QByteArray(R"({"version":2,"phase":42,"count":"many"})")}) {
            const UpdateInfo i = UpdateInfo::parse(bad);
            QCOMPARE(i.phase, u"idle"_s);
            QVERIFY(i.count >= 0);
            QVERIFY(!i.canArm());
        }
        // unknown keys are ignored, missing keys take their default
        const UpdateInfo i = UpdateInfo::parse(R"({"version":3,"phase":"ready","needsRestart":true,"future":1})");
        QCOMPARE(i.phase, u"ready"_s);
        QVERIFY(i.canArm());
        QCOMPARE(i.count, 0);
    }

    void followsTheFile()
    {
        const QString path = m_dir.filePath(u"state.json"_s);
        replaceFile(path, fixture("idle"));
        UpdateState s;
        s.setStatePath(path);
        QCOMPARE(s.phase(), u"idle"_s);
        QSignalSpy spy(&s, &UpdateState::changed);
        replaceFile(path, fixture("ready"));
        QVERIFY(spy.wait(3000));
        QCOMPARE(s.phase(), u"ready"_s);
        QVERIFY(s.canArm());
        // gone: nothing known
        QFile::remove(path);
        QVERIFY(spy.wait(3000));
        QCOMPARE(s.phase(), u"idle"_s);
    }

    void updateAndRestart()
    {
        const QString path = m_dir.filePath(u"state2.json"_s);
        replaceFile(path, fixture("ready"));
        UpdateState s;
        s.setStatePath(path);
        s.setToolPath(fakeTool(0));
        int restarts = 0;
        s.setRestartRequest([&restarts] { ++restarts; });
        QSignalSpy ok(&s, &UpdateState::armSucceeded);
        s.updateAndRestart();
        QVERIFY(s.busy());
        // a second click while the first runs does nothing
        s.updateAndShutdown();
        QVERIFY(ok.wait(5000));
        QCOMPARE(lastArgs(), QStringList({u"arm"_s, u"--then"_s, u"restart"_s}));
        QCOMPARE(restarts, 1);
        QVERIFY(!s.busy());

        s.updateAndShutdown();
        QVERIFY(ok.wait(5000));
        QCOMPARE(lastArgs(), QStringList({u"arm"_s, u"--then"_s, u"shutdown"_s}));
        // the session restarts for a shut down too: the install runs at start-up
        QCOMPARE(restarts, 2);
    }

    void armFailures_data()
    {
        QTest::addColumn<int>("code");
        QTest::addColumn<QString>("reason");
        QTest::newRow("nothing downloaded") << 3 << u"nothing-ready"_s;
        QTest::newRow("other install armed") << 4 << u"busy"_s;
        QTest::newRow("password dialog closed") << 126 << u"cancelled"_s;
        QTest::newRow("not allowed") << 127 << u"cancelled"_s;
        QTest::newRow("anything else") << 1 << u"failed"_s;
    }

    void armFailures()
    {
        QFETCH(int, code);
        QFETCH(QString, reason);
        const QString path = m_dir.filePath(u"state3.json"_s);
        replaceFile(path, fixture("ready"));
        UpdateState s;
        s.setStatePath(path);
        s.setToolPath(fakeTool(code));
        int restarts = 0;
        s.setRestartRequest([&restarts] { ++restarts; });
        QSignalSpy failed(&s, &UpdateState::armFailed);
        s.updateAndRestart();
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().first().toString(), reason);
        QCOMPARE(restarts, 0);
    }

    void missingTool()
    {
        const QString path = m_dir.filePath(u"state4.json"_s);
        replaceFile(path, fixture("ready"));
        UpdateState s;
        s.setStatePath(path);
        s.setToolPath(m_dir.filePath(u"nowhere"_s));
        int restarts = 0;
        s.setRestartRequest([&restarts] { ++restarts; });
        QSignalSpy failed(&s, &UpdateState::armFailed);
        s.updateAndRestart();
        QVERIFY(failed.count() == 1 || failed.wait(5000));
        QCOMPARE(failed.first().first().toString(), u"failed"_s);
        QCOMPARE(restarts, 0);
        QVERIFY(!s.busy());
    }

    void nothingToArm()
    {
        const QString path = m_dir.filePath(u"state5.json"_s);
        replaceFile(path, fixture("idle"));
        UpdateState s;
        s.setStatePath(path);
        s.setToolPath(fakeTool(0));
        QSignalSpy failed(&s, &UpdateState::armFailed);
        s.updateAndRestart();
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().first().toString(), u"nothing-ready"_s);
        QVERIFY(!s.busy());
    }

    void armsWithoutPasswordFollowsTheGroup()
    {
        UpdateState s;
        const struct group *mine = getgrgid(getegid());
        QVERIFY(mine);
        s.setAdminGroup(QByteArray(mine->gr_name));
        QVERIFY(s.armsWithoutPassword());
        s.setAdminGroup(QByteArrayLiteral("raye-no-such-group"));
        QVERIFY(!s.armsWithoutPassword());
    }
};

QTEST_GUILESS_MAIN(TestUpdateState)
#include "tst_updatestate.moc"
