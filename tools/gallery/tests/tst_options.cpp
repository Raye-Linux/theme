#include "options.h"

#include <QTest>

class TestOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults()
    {
        const ParsedOptions p = parseGalleryOptions({});
        QVERIFY(p.ok());
        QCOMPARE(p.options.scheme, Scheme::Night);
        QCOMPARE(p.options.scale, 0.0);
        QVERIFY(!p.options.size.isValid());
        QVERIFY(p.options.page.isEmpty());
        QVERIFY(!p.options.listPages);
    }

    void everyOption()
    {
        const ParsedOptions p = parseGalleryOptions({QStringLiteral("--scheme"), QStringLiteral("day"),
                                                     QStringLiteral("--scale=125%"), QStringLiteral("--page"),
                                                     QStringLiteral("q-menus"), QStringLiteral("--size"),
                                                     QStringLiteral("1920x1080"), QStringLiteral("--shoot"),
                                                     QStringLiteral("/tmp/a.png"), QStringLiteral("--colors-dir=/c"),
                                                     QStringLiteral("--qml-import"), QStringLiteral("/q"),
                                                     QStringLiteral("--style"), QStringLiteral("Breeze"),
                                                     QStringLiteral("--list-pages")});
        QVERIFY2(p.ok(), qPrintable(p.error));
        QCOMPARE(p.options.scheme, Scheme::Day);
        QCOMPARE(p.options.scale, 1.25);
        QCOMPARE(p.options.page, QStringLiteral("q-menus"));
        QCOMPARE(p.options.size, QSize(1920, 1080));
        QCOMPARE(p.options.shootFile, QStringLiteral("/tmp/a.png"));
        QCOMPARE(p.options.colorsDir, QStringLiteral("/c"));
        QCOMPARE(p.options.qmlImportDir, QStringLiteral("/q"));
        QCOMPARE(p.options.style, QStringLiteral("Breeze"));
        QVERIFY(p.options.listPages);
    }

    void rejects_data()
    {
        QTest::addColumn<QStringList>("args");
        QTest::addColumn<QString>("needle");
        QTest::newRow("unknown") << QStringList{QStringLiteral("--colour")} << QStringLiteral("unknown option");
        QTest::newRow("bare word") << QStringList{QStringLiteral("night")} << QStringLiteral("unknown option");
        QTest::newRow("no value") << QStringList{QStringLiteral("--page")} << QStringLiteral("needs a value");
        QTest::newRow("empty value") << QStringList{QStringLiteral("--page=")} << QStringLiteral("needs a value");
        QTest::newRow("scheme") << QStringList{QStringLiteral("--scheme"), QStringLiteral("dusk")} << QStringLiteral("night or day");
        QTest::newRow("scale low") << QStringList{QStringLiteral("--scale"), QStringLiteral("0.4")} << QStringLiteral("0.5 to 3");
        QTest::newRow("scale high") << QStringList{QStringLiteral("--scale"), QStringLiteral("301%")} << QStringLiteral("0.5 to 3");
        QTest::newRow("scale text") << QStringList{QStringLiteral("--scale"), QStringLiteral("big")} << QStringLiteral("0.5 to 3");
        QTest::newRow("size shape") << QStringList{QStringLiteral("--size"), QStringLiteral("1280*800")} << QStringLiteral("WIDTHxHEIGHT");
        QTest::newRow("size small") << QStringList{QStringLiteral("--size"), QStringLiteral("200x800")} << QStringLiteral("WIDTHxHEIGHT");
        QTest::newRow("size big") << QStringList{QStringLiteral("--size"), QStringLiteral("9000x800")} << QStringLiteral("WIDTHxHEIGHT");
    }

    void rejects()
    {
        QFETCH(QStringList, args);
        QFETCH(QString, needle);
        const ParsedOptions p = parseGalleryOptions(args);
        QVERIFY(!p.ok());
        QVERIFY2(p.error.contains(needle), qPrintable(p.error));
    }

    void scaleForms()
    {
        double s = 0;
        QVERIFY(parseScale(QStringLiteral("1.5"), &s));
        QCOMPARE(s, 1.5);
        QVERIFY(parseScale(QStringLiteral(" 150% "), &s));
        QCOMPARE(s, 1.5);
        QVERIFY(parseScale(QStringLiteral("0.5"), &s));
        QVERIFY(parseScale(QStringLiteral("3"), &s));
        QVERIFY(!parseScale(QStringLiteral("%"), &s));
        QVERIFY(!parseScale(QString(), &s));
        QVERIFY(!parseScale(QStringLiteral("nan"), &s));
        QVERIFY(!parseScale(QStringLiteral("inf"), &s));
    }

    void schemeNames()
    {
        Scheme s = Scheme::Night;
        QVERIFY(parseScheme(QStringLiteral("Day"), &s));
        QCOMPARE(s, Scheme::Day);
        QCOMPARE(schemeName(s), QStringLiteral("day"));
        QVERIFY(parseScheme(schemeName(Scheme::Night), &s));
        QCOMPARE(s, Scheme::Night);
    }

    void relaunchKeepsEverythingButTheCapture()
    {
        ParsedOptions p = parseGalleryOptions({QStringLiteral("--scheme=day"), QStringLiteral("--page=w-text"),
                                               QStringLiteral("--size=1280x800"), QStringLiteral("--shoot=/tmp/x.png"),
                                               QStringLiteral("--style=Breeze"), QStringLiteral("--colors-dir=/c")});
        QVERIFY(p.ok());
        const QStringList again = relaunchArguments(p.options, 1.5);
        const ParsedOptions back = parseGalleryOptions(again);
        QVERIFY2(back.ok(), qPrintable(back.error));
        QCOMPARE(back.options.scheme, Scheme::Day);
        QCOMPARE(back.options.scale, 1.5);
        QCOMPARE(back.options.page, QStringLiteral("w-text"));
        QCOMPARE(back.options.size, QSize(1280, 800));
        QCOMPARE(back.options.style, QStringLiteral("Breeze"));
        QCOMPARE(back.options.colorsDir, QStringLiteral("/c"));
        QVERIFY(back.options.shootFile.isEmpty());
    }

    void usageNamesEveryOption()
    {
        const QString usage = galleryUsage();
        for (const char *o : {"--scheme", "--scale", "--page", "--size", "--style", "--colors-dir", "--qml-import",
                              "--shoot", "--list-pages"}) {
            QVERIFY2(usage.contains(QLatin1String(o)), o);
        }
    }
};

QTEST_GUILESS_MAIN(TestOptions)
#include "tst_options.moc"
