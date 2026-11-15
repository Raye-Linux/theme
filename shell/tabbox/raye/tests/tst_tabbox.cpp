// The Raye window switcher: card sizes and rows (layout.js), then the
// QML itself with stand-ins for KWin: cards for each window, Delete and
// the close button close, a click switches, "No open windows" when there
// is none, and French for every string.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>

#include <QFile>
#include <QJSValue>
#include <QJSEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTest>

using namespace Qt::StringLiterals;

class TestTabBox : public QObject
{
    Q_OBJECT

private:
    QJSEngine m_js;
    QStringList m_warnings;
    static inline TestTabBox *s_self = nullptr;

    // only what the switcher's own files say: the offscreen platform has
    // no window shadows and no raise(), which is not the switcher's fault
    static void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
    {
        if (type != QtDebugMsg && type != QtInfoMsg && msg.contains(u"contents/ui/"_s))
            s_self->m_warnings << msg;
    }

    QJSValue call(const char *name, const QJSValueList &args)
    {
        QJSValue f = m_js.globalObject().property(QLatin1String(name));
        return f.call(args);
    }

    struct Run {
        std::unique_ptr<QQmlEngine> engine;
        std::unique_ptr<QObject> model;
        std::unique_ptr<QObject> switcher;
        QQuickWindow *dialog = nullptr;
        QQuickItem *main = nullptr;
        QQuickItem *grid = nullptr;
    };

    std::unique_ptr<Run> start(const QList<QVariantMap> &windows, int current = 0)
    {
        auto run = std::make_unique<Run>();
        run->engine = std::make_unique<QQmlEngine>();
        // the stand-ins first, so they win over anything installed
        run->engine->addImportPath(QStringLiteral(RAYE_TABBOX_TESTS "/mock"));
        run->engine->addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        KLocalization::setupLocalizedContext(run->engine.get());

        QQmlComponent modelComponent(run->engine.get(), QUrl::fromLocalFile(QStringLiteral(RAYE_TABBOX_TESTS "/windows.qml")));
        run->model.reset(modelComponent.create());
        if (!run->model) {
            qWarning() << modelComponent.errors();
            return nullptr;
        }
        for (const QVariantMap &w : windows)
            QMetaObject::invokeMethod(run->model.get(), "add", Q_ARG(QVariant, w));

        QQmlComponent c(run->engine.get(), QUrl::fromLocalFile(QStringLiteral(RAYE_TABBOX_DIR "/contents/ui/main.qml")));
        QObject *o = c.beginCreate(run->engine->rootContext());
        if (!o) {
            qWarning() << c.errors();
            return nullptr;
        }
        o->setProperty("visible", false);
        o->setProperty("model", QVariant::fromValue(run->model.get()));
        o->setProperty("currentIndex", current);
        o->setProperty("screenGeometry", QRect(0, 0, 1920, 1080));
        c.completeCreate();
        run->switcher.reset(o);
        o->setProperty("visible", true);
        QCoreApplication::processEvents();
        for (QObject *child : o->children()) {
            if (QByteArray(child->metaObject()->className()).contains("Instantiator"))
                run->dialog = qobject_cast<QQuickWindow *>(child->property("object").value<QObject *>());
        }
        if (run->dialog) {
            run->main = run->dialog->property("mainItem").value<QQuickItem *>();
            if (run->main)
                run->grid = run->main->findChild<QQuickItem *>(u"switcherGrid"_s);
        }
        return run;
    }

    static QVariantMap window(const QString &caption, const QString &id, bool closeable = true)
    {
        return {{u"caption"_s, caption}, {u"icon"_s, u"utilities-terminal"_s},
                {u"windowId"_s, id}, {u"closeable"_s, closeable}, {u"minimized"_s, false}};
    }

    static void press(QQuickItem *item, int key)
    {
        QKeyEvent down(QEvent::KeyPress, key, Qt::AltModifier);
        QCoreApplication::sendEvent(item, &down);
    }

private Q_SLOTS:
    void initTestCase()
    {
        s_self = this;
        QFile f(QStringLiteral(RAYE_TABBOX_DIR "/contents/ui/layout.js"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        // ".pragma library" is for QML only
        QString code = QString::fromUtf8(f.readAll()).remove(u".pragma library"_s);
        const QJSValue r = m_js.evaluate(code);
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
    }

    void cardSize()
    {
        QCOMPARE(call("cardWidth", {}).toInt(), 236);
        QCOMPARE(call("cardHeight", {}).toInt(), 8 + 20 + 8 + 150 + 8);
    }

    void thumbnails()
    {
        QJSValue wide = call("thumbSize", {1600, 900});
        QCOMPARE(wide.property(u"width"_s).toInt(), 220);
        QCOMPARE(wide.property(u"height"_s).toInt(), 124);
        // a tall window: 150 high, narrower
        QJSValue tall = call("thumbSize", {800, 1200});
        QCOMPARE(tall.property(u"height"_s).toInt(), 150);
        QCOMPARE(tall.property(u"width"_s).toInt(), 100);
        // exactly 220 x 150
        QJSValue edge = call("thumbSize", {2200, 1500});
        QCOMPARE(edge.property(u"height"_s).toInt(), 150);
        QCOMPARE(edge.property(u"width"_s).toInt(), 220);
        // no size known yet
        QJSValue none = call("thumbSize", {0, 0});
        QCOMPARE(none.property(u"width"_s).toInt(), 220);
        QVERIFY(none.property(u"height"_s).toInt() <= 150);
    }

    void rows()
    {
        // 1920 wide: (1920 - 160 - 24 + 12) / (236 + 12) = 7 cards a row
        QCOMPARE(call("columns", {1, 1920}).toInt(), 1);
        QCOMPARE(call("columns", {4, 1920}).toInt(), 4);
        QCOMPARE(call("columns", {12, 1920}).toInt(), 7);
        QCOMPARE(call("rows", {12, 1920}).toInt(), 2);
        // 1280 wide: 4 a row
        QCOMPARE(call("columns", {12, 1280}).toInt(), 4);
        QCOMPARE(call("rows", {12, 1280}).toInt(), 3);
        QCOMPARE(call("columns", {0, 1920}).toInt(), 0);
        // a tiny screen still shows one card a row
        QCOMPARE(call("columns", {3, 300}).toInt(), 1);
        // the plate stays inside the screen less 160 px
        for (int w : {1280, 1366, 1920, 2560, 3840}) {
            QJSValue size = call("contentSize", {40, w, 1080});
            QVERIFY(size.property(u"width"_s).toInt() + 24 <= w - 160);
            QVERIFY(size.property(u"height"_s).toInt() + 24 <= 1080 * 0.8);
        }
    }

    void cards()
    {
        m_warnings.clear();
        const auto old = qInstallMessageHandler(collect);
        auto run = start({window(u"Terminal"_s, u"a-1600x900"_s), window(u"Notes - Kate"_s, u"b-800x1200"_s),
                          window(u"Firefox"_s, u"c-1920x1080"_s), window(u"Files"_s, u"d-1200x800"_s)},
                         1);
        QCoreApplication::processEvents();
        qInstallMessageHandler(old);
        QVERIFY(run);
        QVERIFY2(run->dialog, qPrintable(m_warnings.join(u'\n')));
        QVERIFY(run->main);
        QVERIFY(run->grid);
        QVERIFY2(m_warnings.isEmpty(), qPrintable(m_warnings.join(u'\n')));
        QCOMPARE(run->grid->property("count").toInt(), 4);
        QCOMPARE(run->grid->property("currentIndex").toInt(), 1);
        // one row of four, 16 px around them with the frame's 4 px
        QCOMPARE(qRound(run->main->width()), 4 * 236 + 3 * 12 + 24);
        QCOMPARE(qRound(run->main->height()), 194 + 24);
    }

    void deleteClosesTheSelected()
    {
        auto run = start({window(u"One"_s, u"1"_s), window(u"Two"_s, u"2"_s), window(u"Three"_s, u"3"_s)}, 1);
        QVERIFY(run && run->main);
        press(run->main, Qt::Key_Delete);
        QCOMPARE(run->model->property("closed").toList(), QVariantList({1}));
        QCOMPARE(run->grid->property("count").toInt(), 2);
    }

    void deleteLeavesUncloseableWindows()
    {
        auto run = start({window(u"One"_s, u"1"_s, false)}, 0);
        QVERIFY(run && run->main);
        press(run->main, Qt::Key_Delete);
        QVERIFY(run->model->property("closed").toList().isEmpty());
    }

    void arrowsMove()
    {
        auto run = start({window(u"One"_s, u"1"_s), window(u"Two"_s, u"2"_s), window(u"Three"_s, u"3"_s)}, 0);
        QVERIFY(run && run->main);
        press(run->main, Qt::Key_Right);
        QCOMPARE(run->switcher->property("currentIndex").toInt(), 1);
        press(run->main, Qt::Key_Left);
        press(run->main, Qt::Key_Left);
        // wraps
        QCOMPARE(run->switcher->property("currentIndex").toInt(), 2);
        // KWin moves the selection on Tab: the cards follow, each time
        run->switcher->setProperty("currentIndex", 0);
        QCOMPARE(run->grid->property("currentIndex").toInt(), 0);
        run->switcher->setProperty("currentIndex", 1);
        QCOMPARE(run->grid->property("currentIndex").toInt(), 1);
    }

    void clickSwitchesAndCloseButtonCloses()
    {
        auto run = start({window(u"One"_s, u"1"_s), window(u"Two"_s, u"2"_s)}, 0);
        QVERIFY(run && run->grid && run->dialog);
        run->dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(run->dialog));
        QQuickItem *second = nullptr;
        // the cards are the items of the view's content
        const auto items = run->grid->property("contentItem").value<QQuickItem *>()->childItems();
        for (QQuickItem *i : items) {
            if (i->property("closeable").isValid() && i->property("index").toInt() == 1)
                second = i;
        }
        QVERIFY(second);
        QQuickItem *closeButton = second->findChild<QQuickItem *>(u"closeButton"_s);
        QVERIFY(closeButton);
        // shown on hover only
        QVERIFY(!closeButton->isVisible());
        const QPoint middle = second->mapToScene(QPointF(second->width() / 2, second->height() / 2 + 20)).toPoint();
        QTest::mouseMove(run->dialog, middle);
        QTRY_VERIFY(closeButton->isVisible());
        QTest::mouseClick(run->dialog, Qt::LeftButton, {}, middle);
        QCOMPARE(run->model->property("activated").toInt(), 1);
        const QPoint x = closeButton->mapToScene(QPointF(closeButton->width() / 2, closeButton->height() / 2)).toPoint();
        QTest::mouseMove(run->dialog, x);
        QTest::mouseClick(run->dialog, Qt::LeftButton, {}, x);
        QCOMPARE(run->model->property("closed").toList(), QVariantList({1}));
    }

    void noWindows()
    {
        auto run = start({}, 0);
        QVERIFY(run && run->main);
        bool found = false;
        for (QQuickItem *i : run->main->findChildren<QQuickItem *>()) {
            if (i->property("text").toString() == u"No open windows"_s)
                found = i->isVisible();
        }
        QVERIFY(found);
        QVERIFY(run->main->width() > 100);
    }

    void french()
    {
        QFile po(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_tabbox.po"));
        QVERIFY(po.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(po.readAll());
        QFile qml(QStringLiteral(RAYE_TABBOX_DIR "/contents/ui/main.qml"));
        QVERIFY(qml.open(QIODevice::ReadOnly));
        const QString source = QString::fromUtf8(qml.readAll());
        static const QRegularExpression call(u"i18nd\\(\"raye_shell_tabbox\", \"([^\"]+)\"\\)"_s);
        int count = 0;
        for (auto it = call.globalMatch(source); it.hasNext(); ++count) {
            const QString id = it.next().captured(1);
            const QRegularExpression entry(u"msgid \"%1\"\\nmsgstr \"([^\"]+)\""_s.arg(QRegularExpression::escape(id)));
            const auto m = entry.match(text);
            QVERIFY2(m.hasMatch(), qPrintable(id));
            QVERIFY(m.captured(1) != id);
        }
        QCOMPARE(count, 2);
        // every call names the domain
        QVERIFY(!source.contains(u"i18n(\""_s));
    }
};

QTEST_MAIN(TestTabBox)
#include "tst_tabbox.moc"
