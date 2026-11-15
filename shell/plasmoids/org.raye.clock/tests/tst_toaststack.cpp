// The toasts' windows as they come and go: the newest sits just above the
// taskbar, and when a toast leaves, the ones that stay slide down to their
// own place instead of staying where they were. On either screen, with the
// slide or without it.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QQmlComponent>
#include <QQmlEngine>
#include <QFile>
#include <QRegularExpression>
#include <QTest>
#include <QWindow>

using namespace Qt::StringLiterals;

class TestToastStack : public QObject
{
    Q_OBJECT

    // toastlogic.js: 12 px to the taskbar and between cards, 8 px of window
    // around each card
    static int firstY(int panelTop, int h)
    {
        return panelTop - 12 - (h - 16) - 8;
    }
    static int nextY(int belowY, int h)
    {
        return belowY + 8 - 12 - (h - 16) - 8;
    }

    struct Harness {
        QQmlEngine engine;
        std::unique_ptr<QObject> root;
    };

    std::unique_ptr<Harness> make(bool motion, const QRect &area = QRect(0, 0, 1366, 768), int panelTop = 720)
    {
        auto h = std::make_unique<Harness>();
        h->engine.addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        h->engine.addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        QQmlComponent c(&h->engine, QUrl::fromLocalFile(QStringLiteral(RAYE_CLOCK_TESTS "/toaststack.qml")));
        QObject *o = c.beginCreate(h->engine.rootContext());
        if (!o) {
            qWarning() << c.errorString();
            return nullptr;
        }
        o->setProperty("motion", motion);
        c.completeCreate();
        o->setProperty("area", QRectF(area));
        o->setProperty("panelTop", panelTop);
        h->root.reset(o);
        return h;
    }

    static void add(QObject *root, const QString &name, int height)
    {
        QMetaObject::invokeMethod(root, "add", Q_ARG(QVariant, name), Q_ARG(QVariant, height));
    }
    static void removeRow(QObject *root, int row)
    {
        QMetaObject::invokeMethod(root, "removeRow", Q_ARG(QVariant, row));
    }
    static QWindow *window(QObject *root, int i)
    {
        QVariant w;
        QMetaObject::invokeMethod(root, "windowAt", Q_RETURN_ARG(QVariant, w), Q_ARG(QVariant, i));
        return qobject_cast<QWindow *>(w.value<QObject *>());
    }
    static bool sliding(QWindow *w)
    {
        return w->property("mover").value<QObject *>()->property("running").toBool();
    }

private Q_SLOTS:
    void leaves_data()
    {
        QTest::addColumn<bool>("motion");
        QTest::addColumn<QRect>("screen");
        QTest::newRow("laptop, slide") << true << QRect(0, 0, 1366, 768);
        QTest::newRow("laptop, no slide") << false << QRect(0, 0, 1366, 768);
        QTest::newRow("monitor, slide") << true << QRect(1366, 0, 1920, 1080);
        QTest::newRow("monitor, no slide") << false << QRect(1366, 0, 1920, 1080);
    }

    // the photo of the bug: the newest goes, the older one stayed up there
    void leaves()
    {
        QFETCH(bool, motion);
        QFETCH(QRect, screen);
        const int panelTop = screen.bottom() + 1 - 48;
        auto h = make(motion, screen, panelTop);
        QVERIFY(h);
        QObject *root = h->root.get();
        add(root, u"older"_s, 120);
        add(root, u"newest"_s, 100);
        QWindow *newest = window(root, 0);
        QWindow *older = window(root, 1);
        QVERIFY(newest && older);
        QCOMPARE(newest->objectName(), u"newest"_s);
        QTRY_COMPARE(older->y(), nextY(firstY(panelTop, 100), 120));
        QCOMPARE(newest->y(), firstY(panelTop, 100));
        QCOMPARE(older->x(), screen.right() + 1 - 380);
        QVERIFY(older->isVisible());

        removeRow(root, 0);
        QCOMPARE(window(root, 0), older);
        if (motion) {
            QCoreApplication::processEvents();
            QVERIFY(sliding(older));
        } else {
            QCoreApplication::processEvents();
            QCOMPARE(older->y(), firstY(panelTop, 120));
        }
        QTRY_COMPARE_WITH_TIMEOUT(older->y(), firstY(panelTop, 120), 2000);
        QTRY_VERIFY(!sliding(older));
        QCOMPARE(older->x(), screen.right() + 1 - 380);
        QVERIFY(older->isVisible());
    }

    // one in the middle goes: the newest stays put, the oldest comes down
    void middleLeaves()
    {
        auto h = make(true);
        QVERIFY(h);
        QObject *root = h->root.get();
        add(root, u"a"_s, 100);
        add(root, u"b"_s, 140);
        add(root, u"c"_s, 100);
        QWindow *c = window(root, 0);
        QWindow *a = window(root, 2);
        const int cY = firstY(720, 100);
        QTRY_COMPARE(a->y(), nextY(nextY(cY, 140), 100));

        removeRow(root, 1);
        QCoreApplication::processEvents();
        QVERIFY(!sliding(c));
        QCOMPARE(c->y(), cY);
        QTRY_COMPARE_WITH_TIMEOUT(a->y(), nextY(cY, 100), 2000);
    }

    // a new one comes in at the bottom, straight in place; the others go up
    void arrives()
    {
        auto h = make(true);
        QVERIFY(h);
        QObject *root = h->root.get();
        add(root, u"a"_s, 100);
        QWindow *a = window(root, 0);
        QTRY_COMPARE(a->y(), firstY(720, 100));
        QTRY_VERIFY(a->isVisible());

        add(root, u"b"_s, 120);
        QWindow *b = window(root, 0);
        QCOMPARE(b->objectName(), u"b"_s);
        QCOMPARE(b->y(), firstY(720, 120));
        QVERIFY(!sliding(b));
        QTRY_COMPARE_WITH_TIMEOUT(a->y(), nextY(firstY(720, 120), 100), 2000);
    }

    // the notification centre opens: nothing shows
    void hidden()
    {
        auto h = make(false);
        QVERIFY(h);
        QObject *root = h->root.get();
        add(root, u"a"_s, 100);
        add(root, u"b"_s, 100);
        QTRY_VERIFY(window(root, 0)->isVisible());
        root->property("stack").value<QObject *>()->setProperty("hidden", true);
        QVERIFY(!window(root, 0)->isVisible());
        QVERIFY(!window(root, 1)->isVisible());
    }
    // A toast window that moves asks for a frame: on Wayland KWin takes the
    // new place with the window's next frame, and a toast whose words do
    // not change draws none, so it stays where it was. The offscreen test
    // platform draws on every move by itself, so the file is read instead.
    void moveAsksForAFrame()
    {
        QFile f(QStringLiteral(RAYE_CLOCK_TESTS "/../contents/ui/toasts/ToastWindow.qml"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString qml = QString::fromUtf8(f.readAll());
        for (const char *axis : {"X", "Y"}) {
            const QRegularExpression re(u"^\\s*on%1Changed:\\s*moved\\(\\)"_s.arg(QLatin1String(axis)),
                                        QRegularExpression::MultilineOption);
            QVERIFY2(re.match(qml).hasMatch(), axis);
        }
        // a frame is asked for at once, and the place goes once more when still
        QVERIFY(QRegularExpression(u"function moved\\(\\) \\{\\s*dot\\.flip = !dot\\.flip\\s*update\\(\\)"_s).match(qml).hasMatch());
        QVERIFY(QRegularExpression(u"window\\.y = y \\+ 1\\s*window\\.y = y\\b"_s).match(qml).hasMatch());
    }
};

QTEST_MAIN(TestToastStack)
#include "tst_toaststack.moc"
