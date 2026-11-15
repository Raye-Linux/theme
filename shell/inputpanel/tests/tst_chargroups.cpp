#include "chargroups.h"

#include <QFile>
#include <QTest>

class TestCharGroups : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void lines()
    {
        const auto groups = parseCharGroups(QStringLiteral("# kaomoji\n[happy]\n(^_^)\n  ヽ(・∀・)ﾉ \n\n(^_^)\n[sad]\n(╥﹏╥)\n"), CharSplit::Lines);
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups.at(0).id, QStringLiteral("happy"));
        QCOMPARE(groups.at(0).items, (QStringList{QStringLiteral("(^_^)"), QStringLiteral("ヽ(・∀・)ﾉ")}));
        QCOMPARE(groups.at(1).items, QStringList{QStringLiteral("(╥﹏╥)")});
    }

    void spaces()
    {
        const auto groups = parseCharGroups(QStringLiteral("[currency]\n€ $  £\n¥\n[empty]\n# nothing\n[math]\n± ×"), CharSplit::Spaces);
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups.at(0).items, (QStringList{QStringLiteral("€"), QStringLiteral("$"), QStringLiteral("£"), QStringLiteral("¥")}));
        QCOMPARE(groups.at(1).id, QStringLiteral("math"));
    }

    void itemsBeforeAGroup()
    {
        const auto groups = parseCharGroups(QStringLiteral("stray\n[a]\nx"), CharSplit::Lines);
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups.at(0).items, QStringList{QStringLiteral("x")});
    }

    // The files the panel ships: every group the panel names exists, and
    // every group is one the panel can name (its title is translated).
    void shippedFiles_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<int>("split");
        QTest::addColumn<QStringList>("ids");
        QTest::newRow("kaomoji") << QStringLiteral(DATA_DIR "/kaomoji.txt") << int(CharSplit::Lines)
                                 << QStringList{QStringLiteral("happy"), QStringLiteral("greeting"), QStringLiteral("love"),
                                                QStringLiteral("sad"), QStringLiteral("angry"), QStringLiteral("surprised"),
                                                QStringLiteral("shrug"), QStringLiteral("animals"), QStringLiteral("classic")};
        QTest::newRow("symbols") << QStringLiteral(DATA_DIR "/symbols.txt") << int(CharSplit::Spaces)
                                 << QStringList{QStringLiteral("punctuation"), QStringLiteral("currency"), QStringLiteral("latin"),
                                                QStringLiteral("geometric"), QStringLiteral("math"), QStringLiteral("arrows"),
                                                QStringLiteral("greek"), QStringLiteral("other")};
    }

    void shippedFiles()
    {
        QFETCH(QString, file);
        QFETCH(int, split);
        QFETCH(QStringList, ids);
        QFile f(file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto groups = parseCharGroups(QString::fromUtf8(f.readAll()), CharSplit(split));
        QStringList found;
        for (const CharGroup &g : groups) {
            found.append(g.id);
            QVERIFY2(g.items.size() >= 8, qPrintable(g.id));
        }
        QCOMPARE(found, ids);
    }
};

QTEST_GUILESS_MAIN(TestCharGroups)
#include "tst_chargroups.moc"
