// The USB icon of the taskbar: the rows of its menu, which programs keep
// a device busy, and the words of its toasts, in English and French.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "devicelogic.h"

#include <KLocalizedString>

#include <QFile>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace RayeDevices;

class TestDevices : public QObject
{
    Q_OBJECT

    static QStringList kinds(const QVariantList &rows)
    {
        QStringList out;
        for (const QVariant &r : rows)
            out << r.toMap().value(u"kind"_s).toString();
        return out;
    }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("raye_shell_devices");
        qputenv("LANGUAGE", "en");
    }

    void oneStick()
    {
        Device d{u"/drive/sdb"_s, u"iodd"_s, Kind::UsbDrive, 8000000000ULL, {{u"/vol/sdb1"_s, u"iodd"_s, true, u"/media/u/iodd"_s}}};
        const QVariantList rows = menuRows({d});
        QCOMPARE(kinds(rows), (QStringList{u"eject"_s, u"open"_s}));
        QCOMPARE(rows[0].toMap().value(u"text"_s).toString(), u"Eject iodd"_s);
        QCOMPARE(rows[0].toMap().value(u"udi"_s).toString(), u"/drive/sdb"_s);
        QVERIFY(rows[0].toMap().value(u"detail"_s).toString().endsWith(u"USB drive"_s));
        QVERIFY(rows[0].toMap().value(u"detail"_s).toString().startsWith(u"8"_s));
        QCOMPARE(rows[1].toMap().value(u"text"_s).toString(), u"Open in Files"_s);
        QCOMPARE(rows[1].toMap().value(u"udi"_s).toString(), u"/vol/sdb1"_s);
    }

    void notMountedHasNoOpen()
    {
        Device d{u"/drive/sdb"_s, u"iodd"_s, Kind::UsbDrive, 0, {{u"/vol/sdb1"_s, u"iodd"_s, false, QString()}}};
        QCOMPARE(kinds(menuRows({d})), (QStringList{u"eject"_s}));
        QCOMPARE(menuRows({d})[0].toMap().value(u"detail"_s).toString(), u"USB drive"_s);
    }

    void severalDevicesAndVolumes()
    {
        Device a{u"/a"_s, u"BACKUP"_s, Kind::UsbDrive, 0,
                 {{u"/a1"_s, u"BACKUP"_s, true, u"/m/b"_s}, {u"/a2"_s, u"EFI"_s, true, u"/m/e"_s}}};
        Device c{u"/c"_s, u"SD"_s, Kind::MemoryCard, 0, {}};
        const QVariantList rows = menuRows({a, c});
        QCOMPARE(kinds(rows), (QStringList{u"eject"_s, u"open"_s, u"open"_s, u"separator"_s, u"eject"_s}));
        QCOMPARE(rows[1].toMap().value(u"text"_s).toString(), u"Open BACKUP in Files"_s);
        QCOMPARE(rows[4].toMap().value(u"detail"_s).toString(), u"Memory card"_s);
        QVERIFY(menuRows({}).isEmpty());
    }

    void busyProcesses()
    {
        const QList<Process> procs = {
            {10, u"/home/u"_s, {u"/media/u/iodd/song.mp3"_s}, u"elisa"_s},
            {11, u"/media/u/iodd/photos"_s, {}, u"bash"_s},
            {12, u"/home/u"_s, {u"/media/u/ioddx/other"_s}, u"kate"_s}, // a neighbour, not inside
            {13, u"/media/u/iodd"_s, {}, u"bash"_s},
            {14, u"/"_s, {}, QString()},
        };
        QCOMPARE(processesUsing(procs, {u"/media/u/iodd"_s}), (QStringList{u"bash"_s, u"elisa"_s}));
        QVERIFY(processesUsing(procs, {}).isEmpty());
    }

    void toasts()
    {
        QCOMPARE(safeTitle(), u"Safe to remove hardware"_s);
        QCOMPARE(safeBody(u"iodd"_s), u"The 'iodd' device can now be safely removed from the computer."_s);
        QCOMPARE(busyTitle(Kind::UsbDrive), u"Problem ejecting USB device"_s);
        QVERIFY(!busyBody({}).contains(u"In use by"_s));
        QVERIFY(busyBody({u"Elisa"_s, u"Kate"_s}).endsWith(u"In use by: Elisa, Kate"_s));
        Device d{u"/d"_s, u"iodd"_s, Kind::UsbDrive, 0, {}};
        QCOMPARE(plugTitle(d), u"USB drive (iodd)"_s);
        QCOMPARE(autoPlay(u"open"_s), u"open"_s);
        QCOMPARE(autoPlay(u"nothing"_s), u"nothing"_s);
        QCOMPARE(autoPlay(u"x"_s), u"ask"_s);
        QCOMPARE(autoPlay(QString()), u"ask"_s);
    }

    void french()
    {
        QFile po(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_devices.po"));
        QVERIFY(po.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(po.readAll());
        QVERIFY(text.contains(u"msgstr \"Éjecter %1\""_s));
        QVERIFY(text.contains(u"msgstr \"Le matériel peut être retiré en toute sécurité\""_s));
        QVERIFY(text.contains(u"msgstr \"Ouvrir dans Fichiers\""_s));
    }
};

QTEST_GUILESS_MAIN(TestDevices)
#include "tst_devices.moc"
