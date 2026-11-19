// The wallpaper of the Hyprland session follows the one picked for the
// Plasma desktop: read from Plasma's own desktop settings.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestViews : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString write(const QString &name, const QByteArray &text)
    {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("cannot write %s", qPrintable(path));
        f.write(text);
        return path;
    }

    // a wallpaper package with two sizes
    QString package()
    {
        const QString images = m_dir.filePath(QStringLiteral("Lake/contents/images"));
        QDir().mkpath(images);
        for (const char *n : {"1920x1080.png", "3840x2160.png"}) {
            QFile f(images + QLatin1Char('/') + QLatin1String(n));
            if (!f.open(QIODevice::WriteOnly))
                qFatal("cannot write %s", n);
        }
        return m_dir.filePath(QStringLiteral("Lake"));
    }

private Q_SLOTS:
    void imageOfTheFirstScreen()
    {
        const QString first = write(QStringLiteral("first.jpg"), "x");
        const QString second = write(QStringLiteral("second.jpg"), "x");
        const QString rc = write(QStringLiteral("a"),
            "[Containments][2]\nformfactor=2\nlastScreen=0\nplugin=org.raye.panel\nwallpaperplugin=org.kde.image\n\n"
            "[Containments][7]\nformfactor=0\nlastScreen=1\nwallpaperplugin=org.kde.image\n\n"
            "[Containments][7][Wallpaper][org.kde.image][General]\nImage=file://" + second.toUtf8() + "\n\n"
            "[Containments][9]\nformfactor=0\nlastScreen=0\nwallpaperplugin=org.kde.image\n\n"
            "[Containments][9][Wallpaper][org.kde.image][General]\nImage=" + first.toUtf8() + "\n");
        QVariantMap b = Views::readBackground(rc, 0, QSize(1920, 1080));
        QCOMPARE(b.value(QStringLiteral("source")).toUrl(), QUrl::fromLocalFile(first));
        b = Views::readBackground(rc, 1, QSize(1920, 1080));
        QCOMPARE(b.value(QStringLiteral("source")).toUrl(), QUrl::fromLocalFile(second));
        // a third screen Plasma never saw: the first screen's picture
        b = Views::readBackground(rc, 2, QSize(1920, 1080));
        QCOMPARE(b.value(QStringLiteral("source")).toUrl(), QUrl::fromLocalFile(first));
    }

    void packageGivesTheRightSize()
    {
        const QString pkg = package();
        const QString rc = write(QStringLiteral("b"),
            "[Containments][1]\nformfactor=0\nlastScreen=0\nwallpaperplugin=org.kde.image\n\n"
            "[Containments][1][Wallpaper][org.kde.image][General]\nImage=" + pkg.toUtf8() + "/\n");
        QVariantMap b = Views::readBackground(rc, 0, QSize(1920, 1080));
        QVERIFY(b.value(QStringLiteral("source")).toUrl().toLocalFile().endsWith(QLatin1String("1920x1080.png")));
        b = Views::readBackground(rc, 0, QSize(2560, 1440));
        QVERIFY(b.value(QStringLiteral("source")).toUrl().toLocalFile().endsWith(QLatin1String("3840x2160.png")));
    }

    void plainColour()
    {
        const QString rc = write(QStringLiteral("c"),
            "[Containments][1]\nformfactor=0\nlastScreen=0\nwallpaperplugin=org.kde.color\n\n"
            "[Containments][1][Wallpaper][org.kde.color][General]\nColor=26,16,40\n");
        const QVariantMap b = Views::readBackground(rc, 0, QSize(1920, 1080));
        QCOMPARE(b.value(QStringLiteral("color")).value<QColor>(), QColor(26, 16, 40));
        QVERIFY(b.value(QStringLiteral("source")).toUrl().isEmpty());
    }

    void nothingToFollow()
    {
        // no file, another kind of wallpaper, a picture that is gone: the
        // caller keeps the Raye one
        QVERIFY(Views::readBackground(m_dir.filePath(QStringLiteral("none")), 0, QSize(1920, 1080)).isEmpty());
        const QString rc = write(QStringLiteral("d"),
            "[Containments][1]\nformfactor=0\nlastScreen=0\nwallpaperplugin=org.raye.animated\n\n"
            "[Containments][1][Wallpaper][org.raye.animated][General]\nSource=file:///a.mp4\n");
        QVERIFY(Views::readBackground(rc, 0, QSize(1920, 1080)).isEmpty());
        const QString gone = write(QStringLiteral("e"),
            "[Containments][1]\nformfactor=0\nlastScreen=0\nwallpaperplugin=org.kde.image\n\n"
            "[Containments][1][Wallpaper][org.kde.image][General]\nImage=" + package().toUtf8() + "/missing/\n");
        QVERIFY(Views::readBackground(gone, 0, QSize(1920, 1080)).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestViews)
#include "tst_views.moc"
