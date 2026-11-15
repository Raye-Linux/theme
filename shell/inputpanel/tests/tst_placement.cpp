#include "placement.h"

#include <QTest>

class TestPlacement : public QObject
{
    Q_OBJECT

    const QSize panel{360, 460};
    const QRect screen{0, 0, 1280, 800};

private Q_SLOTS:
    void belowThePointer()
    {
        QCOMPARE(Placement::near(screen, {400, 100}, panel, 48), QPoint(376, 124));
    }

    void aboveWhenNoRoomBelow()
    {
        // 124 + 460 would pass the taskbar: go above the pointer
        QCOMPARE(Placement::near(screen, {400, 700}, panel, 48), QPoint(376, 216));
    }

    void clampedToTheScreen()
    {
        QCOMPARE(Placement::near(screen, {5, 5}, panel, 48), QPoint(12, 29));
        QCOMPARE(Placement::near(screen, {1279, 300}, panel, 48), QPoint(1280 - 12 - 360, 280));
        // middle of a short screen: as low as the taskbar allows
        QCOMPARE(Placement::near(screen, {600, 400}, panel, 48), QPoint(576, 280));
    }

    void secondScreen()
    {
        const QRect right(1280, 0, 1920, 1080);
        QCOMPARE(Placement::near(right, {1300, 50}, panel, 48), QPoint(1292, 74));
    }

    void tooSmallScreen()
    {
        const QRect tiny(0, 0, 300, 400);
        const QPoint p = Placement::near(tiny, {100, 100}, panel, 48);
        QCOMPARE(p, QPoint(12, 12));
    }

    void corner()
    {
        QCOMPARE(Placement::corner(screen, panel, 48), QPoint(1280 - 12 - 360, 800 - 48 - 12 - 460));
    }

    void pasteKeys()
    {
        QCOMPARE(PasteKeys::forWindow(QStringLiteral("firefox")), (QList<quint32>{PasteKeys::LeftCtrl, PasteKeys::V}));
        QCOMPARE(PasteKeys::forWindow(QStringLiteral("org.kde.kate")), (QList<quint32>{PasteKeys::LeftCtrl, PasteKeys::V}));
        QCOMPARE(PasteKeys::forWindow(QString()), (QList<quint32>{PasteKeys::LeftCtrl, PasteKeys::V}));
        QCOMPARE(PasteKeys::forWindow(QStringLiteral("org.kde.konsole")),
                 (QList<quint32>{PasteKeys::LeftCtrl, PasteKeys::LeftShift, PasteKeys::V}));
        QVERIFY(PasteKeys::isTerminal(QStringLiteral(" Konsole ")));
        QVERIFY(!PasteKeys::isTerminal(QStringLiteral("steam_app_123")));
    }
};

QTEST_GUILESS_MAIN(TestPlacement)
#include "tst_placement.moc"
