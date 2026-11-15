// VolumeChime: when the volume sound plays. Shorter timings than the real
// ones so the test is quick; the rules are the same.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "volumechime.h"

#include <QSignalSpy>
#include <QTest>

class TestVolumeChime : public QObject
{
    Q_OBJECT

    static void setUp(VolumeChime &c)
    {
        c.setQuietMs(300);
        c.setSettleMs(60);
    }

    // a change every `step` ms for `ms` ms, like a held key or a wheel
    static void stream(VolumeChime &c, int ms, int step = 20)
    {
        for (int t = 0; t < ms; t += step) {
            c.changed();
            QTest::qWait(step);
        }
    }

private Q_SLOTS:
    void defaults()
    {
        VolumeChime c;
        QVERIFY(c.isEnabled());
        // longer than the 600 ms a held key waits before it repeats
        QVERIFY(c.quietMs() > 600);
        QCOMPARE(c.settleMs(), 150);
    }

    void tapRingsAtOnce()
    {
        VolumeChime c;
        setUp(c);
        QSignalSpy ring(&c, &VolumeChime::ring);
        c.changed();
        QCOMPARE(ring.count(), 1);
        QTest::qWait(400);
        QCOMPARE(ring.count(), 1);
    }

    void heldKeyRingsAtStartAndEnd()
    {
        VolumeChime c;
        setUp(c);
        QSignalSpy ring(&c, &VolumeChime::ring);
        c.changed();
        QTest::qWait(200);
        stream(c, 400);
        QCOMPARE(ring.count(), 1);
        QTRY_COMPARE(ring.count(), 2);
        QTest::qWait(200);
        QCOMPARE(ring.count(), 2);
    }

    void wheelTurnedFast()
    {
        VolumeChime c;
        setUp(c);
        QSignalSpy ring(&c, &VolumeChime::ring);
        stream(c, 500, 10);
        QCOMPARE(ring.count(), 1);
        QTRY_COMPARE(ring.count(), 2);
    }

    void tapsFarApart()
    {
        VolumeChime c;
        setUp(c);
        QSignalSpy ring(&c, &VolumeChime::ring);
        c.changed();
        QTest::qWait(350);
        c.changed();
        QCOMPARE(ring.count(), 2);
    }

    void sliderRingsOnRelease()
    {
        VolumeChime c;
        setUp(c);
        QSignalSpy ring(&c, &VolumeChime::ring);
        c.hold();
        stream(c, 300);
        QCOMPARE(ring.count(), 0);
        c.release();
        QCOMPARE(ring.count(), 1);
        QTest::qWait(150);
        QCOMPARE(ring.count(), 1);
    }

    void sliderUntouched()
    {
        VolumeChime c;
        setUp(c);
        QSignalSpy ring(&c, &VolumeChime::ring);
        c.hold();
        c.release();
        QTest::qWait(150);
        QCOMPARE(ring.count(), 0);
    }

    void holdWaitsOverAPendingSound()
    {
        VolumeChime c;
        setUp(c);
        QSignalSpy ring(&c, &VolumeChime::ring);
        c.changed();
        c.changed();
        c.hold();
        QTest::qWait(150);
        QCOMPARE(ring.count(), 1);
        c.release();
        QCOMPARE(ring.count(), 2);
    }

    void off()
    {
        VolumeChime c;
        setUp(c);
        QSignalSpy ring(&c, &VolumeChime::ring);
        c.setEnabled(false);
        c.changed();
        c.hold();
        c.changed();
        c.release();
        QTest::qWait(150);
        QCOMPARE(ring.count(), 0);
    }

    void turnedOffWhileWaiting()
    {
        VolumeChime c;
        setUp(c);
        QSignalSpy ring(&c, &VolumeChime::ring);
        c.changed();
        c.changed();
        c.setEnabled(false);
        QTest::qWait(150);
        QCOMPARE(ring.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestVolumeChime)
#include "tst_volumechime.moc"
