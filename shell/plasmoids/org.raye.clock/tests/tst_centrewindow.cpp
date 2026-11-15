// The notification centre's window on two screens of different sizes
// (two-screens.json: a 1366 x 768 laptop, a 1920 x 1080 monitor to its
// right), opened from the clock of one, then of the other: from its first
// frame the window is at its final place and size, inside the room above
// the taskbar of that screen, with the whole calendar in it.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QTest>
#include <QtMath>

using namespace Qt::StringLiterals;

class TestCentreWindow : public QObject
{
    Q_OBJECT

    static constexpr int taskbar = 48;

    static void collectItems(QQuickItem *root, const QString &name, QList<QQuickItem *> *out)
    {
        const auto kids = root->childItems();
        for (QQuickItem *kid : kids) {
            if (kid->objectName() == name)
                out->append(kid);
            collectItems(kid, name, out);
        }
    }

private Q_SLOTS:
    void opensInPlace_data()
    {
        QTest::addColumn<QList<int>>("order");
        QTest::addColumn<int>("count");
        for (int count : {0, 5, 50}) {
            QTest::addRow("monitor, laptop, monitor, %d", count) << QList<int>{1, 0, 1} << count;
            QTest::addRow("laptop, monitor, laptop, %d", count) << QList<int>{0, 1, 0} << count;
        }
    }

    void opensInPlace()
    {
        QFETCH(QList<int>, order);
        QFETCH(int, count);
        const auto screens = QGuiApplication::screens();
        QCOMPARE(screens.size(), 2);

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        engine.addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        KLocalization::setupLocalizedContext(&engine);
        QQmlComponent c(&engine, QUrl::fromLocalFile(QStringLiteral(RAYE_CLOCK_TESTS "/popup.qml")));
        QObject *o = c.beginCreate(engine.rootContext());
        QVERIFY2(o, qPrintable(c.errorString()));
        o->setProperty("uiDir", QUrl::fromLocalFile(QStringLiteral(RAYE_CLOCK_DIR "/contents/ui/")));
        o->setProperty("notificationCount", count);
        c.completeCreate();
        std::unique_ptr<QObject> keep(o);
        auto *window = qobject_cast<QQuickWindow *>(o);
        QVERIFY(window);
        QTRY_VERIFY(o->property("placement").value<QObject *>());
        auto *centre = o->property("centre").value<QQuickItem *>();
        QVERIFY(centre);
        auto *calendar = centre->property("calendarCard").value<QQuickItem *>();
        QVERIFY(calendar);
        const int margin = centre->property("margin").toInt();

        for (int index : order) {
            QScreen *screen = screens.at(index);
            const QRect geometry = screen->geometry();
            const int panelTop = geometry.bottom() + 1 - taskbar;
            const QRectF area(geometry.x(), geometry.y(), geometry.width(), panelTop - geometry.y());
            window->setScreen(screen);
            QMetaObject::invokeMethod(o, "open", Q_ARG(QVariant, QVariant(QRectF(geometry))), Q_ARG(QVariant, panelTop),
                                      Q_ARG(QVariant, QVariant(area)));
            // what the window shows with, before any frame is drawn
            const QRect first = window->geometry();
            const qreal firstHeight = centre->height();
            const QString where = QStringLiteral("%1 x %2 screen").arg(geometry.width()).arg(geometry.height());

            QVERIFY2(first.top() >= geometry.top(), qPrintable(where + u": window starts at "_s + QString::number(first.top())));
            QCOMPARE(first.bottom() + 1, panelTop - 12 + margin);
            QCOMPARE(first.right() + 1, geometry.right() + 1 - 12 + margin);
            QCOMPARE(first.height(), qCeil(firstHeight));

            window->show();
            QVERIFY(QTest::qWaitForWindowExposed(window));
            centre->setProperty("animated", true);
            QTest::qWait(600);

            // nothing moved or grew once shown
            QVERIFY2(window->geometry() == first,
                     qPrintable(where + QStringLiteral(": shown at %1,%2 %3x%4, then %5,%6 %7x%8")
                                           .arg(first.x()).arg(first.y()).arg(first.width()).arg(first.height())
                                           .arg(window->x()).arg(window->y()).arg(window->width()).arg(window->height())));
            QCOMPARE(centre->height(), firstHeight);
            // the calendar is whole, the list is what gives way
            const qreal calendarBottom = calendar->mapToItem(centre, QPointF(0, calendar->height())).y();
            QVERIFY2(calendarBottom <= centre->height() - margin + 0.5, qPrintable(where + u": calendar cut"_s));
            QCOMPARE(calendar->height(), calendar->property("targetHeight").toReal());
            QList<QQuickItem *> today;
            collectItems(centre, u"today"_s, &today);
            QVERIFY(!today.isEmpty() && today.first()->isVisible());
            if (count == 50) {
                QList<QQuickItem *> lists;
                collectItems(centre, u"notificationList"_s, &lists);
                QVERIFY(!lists.isEmpty());
                QVERIFY(lists.first()->property("contentHeight").toReal() > lists.first()->height());
            }
            window->hide();
        }
    }
};

QTEST_MAIN(TestCentreWindow)
#include "tst_centrewindow.moc"
