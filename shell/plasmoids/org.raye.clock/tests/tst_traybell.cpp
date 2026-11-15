// The clock takes the stock notifications item out of the trays.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../traybell.h"

#include <QTest>

using namespace Qt::StringLiterals;

class TestTrayBell : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void onInTray()
    {
        RayeTrayBell::Lists l{{u"org.kde.plasma.volume"_s, u"org.kde.plasma.notifications"_s},
                              {u"org.kde.plasma.volume"_s, u"org.kde.plasma.notifications"_s},
                              {u"org.kde.plasma.notifications"_s},
                              {}};
        QVERIFY(RayeTrayBell::take(l));
        QCOMPARE(l.extra, QStringList{u"org.kde.plasma.volume"_s});
        QVERIFY(l.known.contains(u"org.kde.plasma.notifications"_s));
        QVERIFY(l.hidden.isEmpty());
    }

    // a brand new tray, before it has turned its items on
    void neverSeen()
    {
        RayeTrayBell::Lists l;
        QVERIFY(RayeTrayBell::take(l));
        QVERIFY(l.extra.isEmpty());
        QCOMPARE(l.known, QStringList{u"org.kde.plasma.notifications"_s});
    }

    // already off: by the person, or by an earlier run
    void alreadyOff()
    {
        RayeTrayBell::Lists l{{u"org.kde.plasma.volume"_s}, {u"org.kde.plasma.notifications"_s}, {}, {}};
        QVERIFY(!RayeTrayBell::take(l));
        QCOMPARE(l.extra, QStringList{u"org.kde.plasma.volume"_s});
    }
};

QTEST_GUILESS_MAIN(TestTrayBell)
#include "tst_traybell.moc"
