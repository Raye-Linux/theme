// The launch ring the effect draws, read from the committed cursor themes.
#include "../ringframes.h"

#include <QSet>
#include <QTemporaryDir>
#include <QTest>

class TestRingFrames : public QObject
{
    Q_OBJECT
    QString cursors() const
    {
        return QString::fromLocal8Bit(qgetenv("RAYE_CURSORS"));
    }

private Q_SLOTS:
    void bothThemesTurn_data()
    {
        QTest::addColumn<QString>("theme");
        QTest::addColumn<int>("size");
        QTest::addColumn<qreal>("scale");
        for (const char *t : {"Raye", "Raye-Dark"}) {
            QTest::addRow("%s 24 @1", t) << QString::fromLatin1(t) << 24 << 1.0;
            QTest::addRow("%s 24 @1.5", t) << QString::fromLatin1(t) << 24 << 1.5;
            QTest::addRow("%s 48 @2", t) << QString::fromLatin1(t) << 48 << 2.0;
        }
    }
    void bothThemesTurn()
    {
        QFETCH(QString, theme);
        QFETCH(int, size);
        QFETCH(qreal, scale);
        const QStringList dirs = ringDirs(theme, {cursors()});
        QVERIFY(!dirs.isEmpty());
        QCOMPARE(dirs.first(), cursors() + QLatin1Char('/') + theme + QStringLiteral("/cursors_scalable/raye-launch-ring"));
        const RingFrames ring = loadRing(dirs.first(), size, scale);
        // an animation, not one still picture
        QVERIFY2(ring.images.size() >= 8, "the ring has one frame");
        QCOMPARE(ring.delays.size(), ring.images.size());
        // a 32-unit canvas of which 24 make one pointer size
        QCOMPARE(ring.size, size * 32.0 / 24.0);
        QCOMPARE(ring.hotspot, QPointF(3.6, 3.0) * (size / 24.0));
        QSet<QByteArray> seen;
        bool violet = false;
        for (const QImage &img : ring.images) {
            QCOMPARE(img.width(), qRound(ring.size * scale));
            seen.insert(QByteArray(reinterpret_cast<const char *>(img.constBits()), img.sizeInBytes()));
            for (int y = 0; y < img.height() && !violet; ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = QColor::fromRgba(img.pixel(x, y));
                    if (c.alpha() > 200 && c.blue() > 200 && c.red() > 90 && c.red() < 210 && c.green() < 110) {
                        violet = true;
                        break;
                    }
                }
            }
        }
        QCOMPARE(seen.size(), ring.images.size());
        QVERIFY2(violet, "no Raye violet in the ring");
        // the ring sits right of and below the tip, where the progress
        // pointer has it; the top left quarter, over the arrow, is empty
        const QImage &f = ring.images.first();
        for (int y = 0; y < f.height() / 2; ++y) {
            for (int x = 0; x < f.width() / 2; ++x) {
                QVERIFY(qAlpha(f.pixel(x, y)) == 0);
            }
        }
    }

    void otherThemesFallBackToRaye()
    {
        const QStringList dirs = ringDirs(QStringLiteral("breeze_cursors"), {cursors()});
        QCOMPARE(dirs.size(), 1);
        QVERIFY(dirs.first().endsWith(QStringLiteral("/Raye/cursors_scalable/raye-launch-ring")));
    }

    void nothingWithoutTheFiles()
    {
        QTemporaryDir empty;
        QVERIFY(ringDirs(QStringLiteral("Raye"), {empty.path()}).isEmpty());
        QVERIFY(loadRing(empty.path(), 24, 1).isEmpty());
        QVERIFY(loadRing(ringDirs(QStringLiteral("Raye"), {cursors()}).first(), 0, 1).isEmpty());
    }
};

QTEST_MAIN(TestRingFrames)
#include "test_ringframes.moc"
