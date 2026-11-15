#include "runhistory.h"

#include <KConfig>

#include <QTemporaryDir>
#include <QTest>

class TestRunHistory : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void newestFirst()
    {
        QStringList list;
        list = RunHistory::added(list, QStringLiteral("firefox"));
        list = RunHistory::added(list, QStringLiteral("kate"));
        QCOMPARE(list, (QStringList{QStringLiteral("kate"), QStringLiteral("firefox")}));
    }

    void noDuplicate()
    {
        const QStringList list = RunHistory::added({QStringLiteral("kate"), QStringLiteral("firefox"), QStringLiteral("ls")},
                                                   QStringLiteral(" firefox "));
        QCOMPARE(list, (QStringList{QStringLiteral("firefox"), QStringLiteral("kate"), QStringLiteral("ls")}));
    }

    void emptyIgnored()
    {
        QCOMPARE(RunHistory::added({QStringLiteral("kate")}, QStringLiteral("  ")), QStringList{QStringLiteral("kate")});
    }

    void capped()
    {
        QStringList list;
        for (int i = 0; i < 40; ++i)
            list = RunHistory::added(list, QString::number(i));
        QCOMPARE(list.size(), RunHistory::MaxEntries);
        QCOMPARE(list.first(), QStringLiteral("39"));
        QCOMPARE(list.last(), QStringLiteral("14"));
    }

    void savedAndRead()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("raye-runstaterc"));
        {
            KConfig config(file, KConfig::SimpleConfig);
            RunHistory history(config.group(QStringLiteral("History")));
            history.add(QStringLiteral("firefox"));
            history.add(QStringLiteral("~/Documents"));
        }
        KConfig config(file, KConfig::SimpleConfig);
        RunHistory history(config.group(QStringLiteral("History")));
        QCOMPARE(history.entries(), (QStringList{QStringLiteral("~/Documents"), QStringLiteral("firefox")}));
    }
};

QTEST_GUILESS_MAIN(TestRunHistory)
#include "tst_runhistory.moc"
