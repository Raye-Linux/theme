#include "scheme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace
{
const QString themeDir = QStringLiteral(RAYE_THEME_DIR);

// [group] key=r,g,b read straight from the file, without KConfig
QColor colourInFile(const QString &path, const QString &group, const QString &key)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    bool inGroup = false;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inGroup = line == QLatin1Char('[') + group + QLatin1Char(']');
            continue;
        }
        if (inGroup && line.startsWith(key + QLatin1Char('='))) {
            const QStringList rgb = line.mid(key.size() + 1).split(QLatin1Char(','));
            if (rgb.size() >= 3) {
                return QColor(rgb[0].toInt(), rgb[1].toInt(), rgb[2].toInt());
            }
        }
    }
    return {};
}
}

class TestScheme : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void findsTheRepoColours()
    {
        const QString dir = SchemeManager::findColorsDir(themeDir + QStringLiteral("/tools/gallery/src"));
        QCOMPARE(QDir(dir).canonicalPath(), QDir(themeDir + QStringLiteral("/colors")).canonicalPath());
    }

    void findsNothingInAnEmptyTree()
    {
        QTemporaryDir tmp;
        QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("a/b/c")));
        QVERIFY(SchemeManager::findColorsDir(tmp.path() + QStringLiteral("/a/b/c")).isEmpty());
        // a colors/ without the scheme files is not it
        QVERIFY(QDir(tmp.path()).mkpath(QStringLiteral("a/colors")));
        QVERIFY(SchemeManager::findColorsDir(tmp.path() + QStringLiteral("/a/b/c")).isEmpty());
        QFile f(tmp.filePath(QStringLiteral("a/colors/RayeNight.colors")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QCOMPARE(SchemeManager::findColorsDir(tmp.path() + QStringLiteral("/a/b/c")), tmp.path() + QStringLiteral("/a/colors"));
    }

    void appliesNightAndDay()
    {
        SchemeManager schemes(themeDir + QStringLiteral("/colors"));
        QSignalSpy spy(&schemes, &SchemeManager::schemeChanged);
        for (Scheme s : {Scheme::Night, Scheme::Day}) {
            QString error;
            QVERIFY2(schemes.apply(s, &error), qPrintable(error));
            const QString file = schemes.fileFor(s);
            const QPalette pal = QApplication::palette();
            QCOMPARE(pal.color(QPalette::Active, QPalette::Window),
                     colourInFile(file, QStringLiteral("Colors:Window"), QStringLiteral("BackgroundNormal")));
            QCOMPARE(pal.color(QPalette::Active, QPalette::Base),
                     colourInFile(file, QStringLiteral("Colors:View"), QStringLiteral("BackgroundNormal")));
            QCOMPARE(pal.color(QPalette::Active, QPalette::Highlight),
                     colourInFile(file, QStringLiteral("Colors:Selection"), QStringLiteral("BackgroundNormal")));
            QCOMPARE(qApp->property("KDE_COLOR_SCHEME_PATH").toString(), QFileInfo(file).absoluteFilePath());
            QCOMPARE(schemes.current(), s);
        }
        QCOMPARE(spy.count(), 2);
        QVERIFY(colourInFile(schemes.fileFor(Scheme::Night), QStringLiteral("Colors:Window"), QStringLiteral("BackgroundNormal"))
                != colourInFile(schemes.fileFor(Scheme::Day), QStringLiteral("Colors:Window"), QStringLiteral("BackgroundNormal")));
    }

    void refusesWhatIsNotAScheme()
    {
        QTemporaryDir tmp;
        QFile f(tmp.filePath(QStringLiteral("RayeNight.colors")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[General]\nName=nothing\n");
        f.close();
        SchemeManager schemes(tmp.path());
        const QPalette before = QApplication::palette();
        QString error;
        QVERIFY(!schemes.apply(Scheme::Night, &error));
        QVERIFY(error.contains(QStringLiteral("not a colour scheme")));
        QVERIFY(!schemes.apply(Scheme::Day, &error));
        QVERIFY(error.contains(QStringLiteral("no colour scheme file")));
        QVERIFY(!schemes.applied());
        QCOMPARE(QApplication::palette(), before);

        SchemeManager none{QString()};
        QVERIFY(!none.apply(Scheme::Night, &error));
    }

    void stylePick()
    {
        const QStringList keys = {QStringLiteral("Windows"), QStringLiteral("Fusion"), QStringLiteral("Breeze")};
        QCOMPARE(pickStyle({}, keys), QStringLiteral("Breeze"));
        QCOMPARE(pickStyle({}, keys + QStringList{QStringLiteral("Raye")}), QStringLiteral("Raye"));
        QCOMPARE(pickStyle({}, {QStringLiteral("Windows"), QStringLiteral("Fusion")}), QStringLiteral("Fusion"));
        QCOMPARE(pickStyle({}, {QStringLiteral("Windows")}), QStringLiteral("Windows"));
        QCOMPARE(pickStyle(QStringLiteral("fusion"), keys), QStringLiteral("Fusion"));
        QVERIFY(pickStyle(QStringLiteral("Raye"), keys).isEmpty());
        QVERIFY(pickStyle({}, {}).isEmpty());
    }

    void iconPick()
    {
        const QStringList breeze = {QStringLiteral("breeze"), QStringLiteral("breeze-dark"), QStringLiteral("hicolor")};
        QCOMPARE(pickIconTheme(Scheme::Night, breeze), QStringLiteral("breeze-dark"));
        QCOMPARE(pickIconTheme(Scheme::Day, breeze), QStringLiteral("breeze"));
        const QStringList raye = breeze + QStringList{QStringLiteral("Raye"), QStringLiteral("Raye-Dark")};
        QCOMPARE(pickIconTheme(Scheme::Night, raye), QStringLiteral("Raye-Dark"));
        QCOMPARE(pickIconTheme(Scheme::Day, raye), QStringLiteral("Raye"));
        QVERIFY(pickIconTheme(Scheme::Day, {QStringLiteral("hicolor")}).isEmpty());
    }

    void qmlImportDir()
    {
        const QString dir = findQmlImportDir(themeDir + QStringLiteral("/tools/gallery"));
        QCOMPARE(QDir(dir).canonicalPath(), QDir(themeDir + QStringLiteral("/qml")).canonicalPath());
        QTemporaryDir tmp;
        QVERIFY(findQmlImportDir(tmp.path()).isEmpty());
    }

    void iconThemesHaveIconFolders()
    {
        QTemporaryDir data;
        QTemporaryDir home;
        auto write = [&data](const QString &theme, const QByteArray &text) {
            QVERIFY(QDir(data.path()).mkpath(QStringLiteral("icons/") + theme));
            QFile f(data.filePath(QStringLiteral("icons/") + theme + QStringLiteral("/index.theme")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(text);
        };
        write(QStringLiteral("Icons"), "[Icon Theme]\nName=Icons\nDirectories=16x16/apps\n");
        // a cursor theme also has an index.theme, without icon folders
        write(QStringLiteral("Pointer"), "[Icon Theme]\nName=Pointer\nInherits=Adwaita\n");
        QVERIFY(QDir(data.path()).mkpath(QStringLiteral("icons/Empty")));

        const QByteArray oldDirs = qgetenv("XDG_DATA_DIRS");
        const QByteArray oldHome = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_DIRS", data.path().toLocal8Bit());
        qputenv("XDG_DATA_HOME", home.path().toLocal8Bit());
        const QStringList found = installedIconThemes();
        qputenv("XDG_DATA_DIRS", oldDirs);
        qputenv("XDG_DATA_HOME", oldHome);
        QCOMPARE(found, QStringList{QStringLiteral("Icons")});
    }
};

QTEST_MAIN(TestScheme)
#include "tst_scheme.moc"
