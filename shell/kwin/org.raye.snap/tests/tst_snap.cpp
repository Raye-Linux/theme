// Raye Snap.
//
// snap.js on its own: the layouts, zone rectangles, which snap a window is
// in, Windows' arrow keys, the snap bar's trigger, resize partners and the
// flyout's place. Then Layouts.qml (zones, clicks, number keys), and the
// script itself, main.qml, against stand-ins for KWin (mock/): the keys
// move a window the way Windows does, Win+Z picks a zone, snap assist
// fills the free zone, the snap bar places a dragged window, partners
// resize together, and Win+Home minimises the rest and brings it back.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>

#include <QColor>
#include <QElapsedTimer>
#include <QFile>
#include <QJSEngine>
#include <QKeyEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

static QVariantMap rect(int x, int y, int w, int h)
{
    return {{u"x"_s, x}, {u"y"_s, y}, {u"width"_s, w}, {u"height"_s, h}};
}

class TestSnap : public QObject
{
    Q_OBJECT

private:
    QJSEngine m_js;
    QStringList m_warnings;
    static inline TestSnap *s_self = nullptr;

    static void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
    {
        if (type != QtDebugMsg && type != QtInfoMsg && msg.contains(u"contents/"_s))
            s_self->m_warnings << msg;
        else if (type != QtDebugMsg)
            fprintf(stderr, "%s\n", qPrintable(msg));
    }

    QJSValue call(const char *name, const QJSValueList &args = {})
    {
        QJSValue r = m_js.globalObject().property(QLatin1String(name)).call(args);
        if (r.isError())
            qWarning() << r.toString();
        return r;
    }

    QJSValue js(const QVariant &v)
    {
        return m_js.toScriptValue(v);
    }

    static QVariantMap map(const QJSValue &v)
    {
        return v.toVariant().toMap();
    }

    // --- running main.qml against the stand-ins -------------------------------

    struct Run {
        std::unique_ptr<QQmlEngine> engine;
        std::unique_ptr<QObject> script;
        QObject *workspace = nullptr;
        QObject *kwin = nullptr;
        QList<QObject *> outputs;
        QList<QObject *> windows;
        std::vector<std::unique_ptr<QObject>> owned;

        QObject *child(const QString &name) const
        {
            return script->findChild<QObject *>(name);
        }
        QObject *shortcut(const QString &name) const
        {
            for (QObject *o : script->children()) {
                if (o->property("sequence").isValid() && o->property("name").toString() == name)
                    return o;
            }
            return nullptr;
        }
        void press(const QString &name) const
        {
            QObject *s = shortcut(name);
            QVERIFY2(s, qPrintable(name));
            QMetaObject::invokeMethod(s, "activated");
        }
    };

    std::unique_ptr<Run> start(const QVariantMap &config = {})
    {
        auto run = std::make_unique<Run>();
        run->engine = std::make_unique<QQmlEngine>();
        run->engine->addImportPath(QStringLiteral(RAYE_SNAP_TESTS "/mock"));
        run->engine->addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        KLocalization::setupLocalizedContext(run->engine.get());

        // load the module once so its singletons exist
        QQmlComponent preload(run->engine.get());
        preload.setData("import org.kde.kwin\nimport QtQml\nQtObject {}", QUrl(u"file:preload.qml"_s));
        std::unique_ptr<QObject> loaded(preload.create());
        if (!loaded) {
            qWarning() << preload.errors();
            return nullptr;
        }
        run->kwin = run->engine->singletonInstance<QObject *>("org.kde.kwin", "KWin");
        run->workspace = run->engine->singletonInstance<QObject *>("org.kde.kwin", "Workspace");
        if (!run->kwin || !run->workspace) {
            qWarning() << "no KWin stand-ins";
            return nullptr;
        }
        run->kwin->setProperty("config", config);

        // two screens side by side; the taskbar takes 48 px of each
        run->outputs << make(run.get(), "FakeOutput", {{u"name"_s, u"A"_s}, {u"geometry"_s, QRectF(0, 0, 1920, 1080)}});
        run->outputs << make(run.get(), "FakeOutput", {{u"name"_s, u"B"_s}, {u"geometry"_s, QRectF(1920, 0, 1280, 1024)}});
        run->workspace->setProperty("screens", QVariant::fromValue(run->outputs));

        QQmlComponent c(run->engine.get(), QUrl::fromLocalFile(QStringLiteral(RAYE_SNAP_PACKAGE "/contents/ui/main.qml")));
        run->script.reset(c.create());
        if (!run->script) {
            qWarning() << c.errors();
            return nullptr;
        }
        return run;
    }

    QObject *make(Run *run, const char *type, const QVariantMap &props)
    {
        QQmlComponent c(run->engine.get());
        c.setData(QByteArray("import org.kde.kwin\n") + type + " {}", QUrl(u"file:make.qml"_s));
        QObject *o = c.createWithInitialProperties(props);
        if (!o) {
            qWarning() << c.errors();
            return nullptr;
        }
        run->owned.emplace_back(o);
        return o;
    }

    QObject *addWindow(Run *run, const QString &caption, const QRectF &frame, int output = 0)
    {
        QObject *w = make(run, "FakeWindow", {{u"caption"_s, caption}, {u"frameGeometry"_s, frame},
                                              {u"output"_s, QVariant::fromValue(run->outputs.at(output))}});
        run->windows << w;
        run->workspace->setProperty("windows", QVariant::fromValue(run->windows));
        QMetaObject::invokeMethod(run->workspace, "windowAdded", Q_ARG(QVariant, QVariant::fromValue(w)));
        return w;
    }

    static QQuickItem *findItem(QQuickItem *in, const QString &name)
    {
        if (!in)
            return nullptr;
        if (in->objectName() == name)
            return in;
        for (QQuickItem *c : in->childItems()) {
            if (QQuickItem *f = findItem(c, name))
                return f;
        }
        return nullptr;
    }

    // the landing sheet leaves hole clear: no part of it is drawn there
    static void checkHole(QQuickItem *landing, const QRectF &hole)
    {
        QTRY_COMPARE(landing->property("hole").toRectF(), hole);
        int bands = 0;
        for (QQuickItem *c : landing->childItems()) {
            if (c->objectName() != u"landingBand"_s || c->width() <= 0 || c->height() <= 0)
                continue;
            ++bands;
            QVERIFY2(!QRectF(c->x(), c->y(), c->width(), c->height()).intersects(hole),
                     "a part of the sheet is drawn over the window");
        }
        QVERIFY(bands > 0);
    }

    static void collectItems(QQuickItem *in, const QString &name, QList<QQuickItem *> &out)
    {
        if (!in)
            return;
        if (in->objectName() == name)
            out << in;
        for (QQuickItem *c : in->childItems())
            collectItems(c, name, out);
    }

    // the live pictures of the dragged window on the sheet, one per band it shows in
    static QList<QQuickItem *> pictures(QQuickItem *in)
    {
        QList<QQuickItem *> out;
        collectItems(in, u"landingPicture"_s, out);
        return out;
    }

    // the size the window's picture is drawn at: its own size is one to one
    static QSizeF thumbnailSize(QQuickItem *picture)
    {
        auto *t = picture->property("thumbnail").value<QQuickItem *>();
        return t ? t->size() : QSizeF();
    }

    // a picture's place in the preview, whichever band shows it
    static QRectF pictureRect(QQuickItem *picture, QQuickItem *landing)
    {
        return QRectF(picture->mapToItem(landing, QPointF(0, 0)), picture->size());
    }

    // the bar shown for a window being dragged, and the pointer on a zone
    struct BarDrag {
        QObject *bar = nullptr;
        QQuickWindow *preview = nullptr;
        QQuickItem *layouts = nullptr;
        QQuickItem *landing = nullptr;
    };

    static BarDrag dragToBar(Run *run, QObject *w)
    {
        BarDrag d;
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        d.bar = run->child(u"snapBar"_s);
        d.preview = qobject_cast<QQuickWindow *>(run->child(u"snapPreview"_s));
        d.layouts = qobject_cast<QQuickItem *>(run->child(u"barLayouts"_s));
        d.landing = d.preview ? findItem(d.preview->contentItem(), u"landing"_s) : nullptr;
        return d;
    }

    static void pointAtZone(Run *run, const BarDrag &d, const QString &name)
    {
        auto *zone = findItem(d.layouts, name);
        QVERIFY2(zone, qPrintable(name));
        const QPointF p = zone->mapToItem(d.layouts, QPointF(zone->width() / 2, zone->height() / 2));
        run->workspace->setProperty("cursorPos", QPointF(d.bar->property("x").toInt() + p.x(), d.bar->property("restY").toInt() + p.y()));
    }

    static QRect frame(QObject *w)
    {
        return w->property("frameGeometry").toRectF().toRect();
    }

    static void setOutput(Run *run, QObject *w, int i)
    {
        w->setProperty("output", QVariant::fromValue(run->outputs.at(i)));
    }

private Q_SLOTS:
    void initTestCase()
    {
        s_self = this;
        qInstallMessageHandler(collect);
        QFile f(QStringLiteral(RAYE_SNAP_PACKAGE "/contents/code/snap.js"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString code = QString::fromUtf8(f.readAll()).remove(u".pragma library"_s);
        const QJSValue r = m_js.evaluate(code);
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
    }

    void init()
    {
        m_warnings.clear();
    }

    void cleanup()
    {
        QVERIFY2(m_warnings.isEmpty(), qPrintable(m_warnings.join(u'\n')));
    }

    // --- snap.js ----------------------------------------------------------------

    void layouts()
    {
        auto ids = [this](int width) {
            QStringList out;
            const QJSValue list = call("layoutsFor", {width});
            for (int i = 0; i < list.property(u"length"_s).toInt(); ++i)
                out << list.property(i).property(u"id"_s).toString();
            return out;
        };
        QCOMPARE(ids(1920), (QStringList{u"halves"_s, u"two-thirds"_s, u"thirds"_s, u"half-quarters"_s, u"quarters"_s, u"one-third"_s}));
        QCOMPARE(ids(1400), ids(1920));
        QCOMPARE(ids(1366), (QStringList{u"halves"_s, u"two-thirds"_s, u"half-quarters"_s, u"quarters"_s}));
        // every layout covers the screen exactly once
        const QJSValue all = m_js.globalObject().property(u"allLayouts"_s);
        for (int i = 0; i < all.property(u"length"_s).toInt(); ++i) {
            const QJSValue zones = all.property(i).property(u"zones"_s);
            double sum = 0;
            for (int z = 0; z < zones.property(u"length"_s).toInt(); ++z)
                sum += zones.property(z).property(u"w"_s).toNumber() * zones.property(z).property(u"h"_s).toNumber();
            QVERIFY(qFuzzyCompare(sum, 1.0));
        }
    }

    void zoneRects()
    {
        const QJSValue area = js(rect(0, 0, 1280, 752));
        const QJSValue thirds = call("layoutById", {u"thirds"_s}).property(u"zones"_s);
        int x = 0;
        int total = 0;
        for (int i = 0; i < 3; ++i) {
            const QVariantMap r = map(call("zoneRect", {area, thirds.property(i)}));
            QCOMPARE(r.value(u"x"_s).toInt(), x);
            QCOMPARE(r.value(u"height"_s).toInt(), 752);
            x += r.value(u"width"_s).toInt();
            total += r.value(u"width"_s).toInt();
        }
        // no gap, no overlap
        QCOMPARE(total, 1280);
        // a work area that does not start at 0 (a second screen)
        const QVariantMap r = map(call("zoneRect", {js(rect(1920, 0, 1280, 976)), m_js.globalObject().property(u"named"_s).property(u"bottomright"_s)}));
        QCOMPARE(r, rect(2560, 488, 640, 488));
    }

    void classify_data()
    {
        QTest::addColumn<QVariantMap>("frame");
        QTest::addColumn<QString>("state");
        QTest::newRow("left") << rect(0, 0, 960, 1032) << u"left"_s;
        QTest::newRow("right, a few px off") << rect(963, 2, 955, 1030) << u"right"_s;
        QTest::newRow("top left") << rect(0, 0, 960, 516) << u"topleft"_s;
        QTest::newRow("bottom right") << rect(960, 516, 960, 516) << u"bottomright"_s;
        QTest::newRow("floating") << rect(200, 150, 800, 600) << u"none"_s;
        QTest::newRow("two thirds is not a half") << rect(0, 0, 1280, 1032) << u"none"_s;
    }

    void classify()
    {
        QFETCH(QVariantMap, frame);
        QFETCH(QString, state);
        QCOMPARE(call("classify", {js(frame), js(rect(0, 0, 1920, 1032))}).toString(), state);
    }

    void arrows_data()
    {
        QTest::addColumn<QString>("state");
        QTest::addColumn<QString>("key");
        QTest::addColumn<bool>("prev");
        QTest::addColumn<bool>("next");
        QTest::addColumn<QString>("expected"); // action, or action:zone:screen
        QTest::newRow("normal, left") << u"none"_s << u"left"_s << false << false << u"snap:left:same"_s;
        QTest::newRow("maximized, right") << u"maximized"_s << u"right"_s << false << false << u"snap:right:same"_s;
        QTest::newRow("left, left, one screen: round to the right") << u"left"_s << u"left"_s << false << false << u"snap:right:same"_s;
        QTest::newRow("left, left, screen on the left") << u"left"_s << u"left"_s << true << false << u"snap:right:prev"_s;
        QTest::newRow("right, right, screen on the right") << u"right"_s << u"right"_s << false << true << u"snap:left:next"_s;
        QTest::newRow("right, left: restore") << u"right"_s << u"left"_s << false << false << u"restore"_s;
        QTest::newRow("left, right: restore") << u"left"_s << u"right"_s << true << true << u"restore"_s;
        QTest::newRow("left, up: top quarter") << u"left"_s << u"up"_s << false << false << u"snap:topleft:same"_s;
        QTest::newRow("right, down: bottom quarter") << u"right"_s << u"down"_s << false << false << u"snap:bottomright:same"_s;
        QTest::newRow("top left, down: back to the half") << u"topleft"_s << u"down"_s << false << false << u"snap:left:same"_s;
        QTest::newRow("bottom right, up: back to the half") << u"bottomright"_s << u"up"_s << false << false << u"snap:right:same"_s;
        QTest::newRow("bottom left, down: minimise") << u"bottomleft"_s << u"down"_s << false << false << u"minimize"_s;
        QTest::newRow("top right, left: top left") << u"topright"_s << u"left"_s << false << false << u"snap:topleft:same"_s;
        QTest::newRow("top left, left, screen on the left") << u"topleft"_s << u"left"_s << true << false << u"snap:topright:prev"_s;
        QTest::newRow("top left, left, one screen") << u"topleft"_s << u"left"_s << false << false << u"nothing"_s;
        QTest::newRow("normal, up: maximise") << u"none"_s << u"up"_s << false << false << u"maximize"_s;
        QTest::newRow("top left, up: maximise") << u"topleft"_s << u"up"_s << false << false << u"maximize"_s;
        QTest::newRow("maximized, up") << u"maximized"_s << u"up"_s << false << false << u"nothing"_s;
        QTest::newRow("maximized, down: restore") << u"maximized"_s << u"down"_s << false << false << u"restore"_s;
        QTest::newRow("normal, down: minimise") << u"none"_s << u"down"_s << false << false << u"minimize"_s;
    }

    void arrows()
    {
        QFETCH(QString, state);
        QFETCH(QString, key);
        QFETCH(bool, prev);
        QFETCH(bool, next);
        QFETCH(QString, expected);
        const QVariantMap r = map(call("arrow", {state, key, prev, next}));
        QString got = r.value(u"action"_s).toString();
        if (got == u"snap"_s)
            got += u':' + r.value(u"zone"_s).toString() + u':' + r.value(u"screen"_s).toString();
        QCOMPARE(got, expected);
    }

    void fullHeightAndBar()
    {
        QCOMPARE(map(call("fullHeight", {js(rect(300, 200, 700, 400)), js(rect(0, 0, 1920, 1032))})), rect(300, 0, 700, 1032));
        const QJSValue screen = js(rect(0, 0, 1920, 1080));
        auto at = [this, &screen](int x, int y) {
            return call("inBarTrigger", {js(QVariantMap{{u"x"_s, x}, {u"y"_s, y}}), screen}).toBool();
        };
        QVERIFY(at(960, 0));
        QVERIFY(at(700, 8));
        QVERIFY(!at(960, 9));
        QVERIFY(!at(600, 0)); // left third
        QVERIFY(!at(1300, 0)); // right third
    }

    void partners()
    {
        // left and right halves; the left one grows by 100
        const QVariantMap left = rect(0, 0, 960, 1032);
        const QVariantMap right = rect(960, 0, 960, 1032);
        QCOMPARE(map(call("partnerFrame", {js(left), js(rect(0, 0, 1060, 1032)), js(right)})), rect(1060, 0, 860, 1032));
        // the right one's left edge moves left
        QCOMPARE(map(call("partnerFrame", {js(right), js(rect(900, 0, 1020, 1032)), js(left)})), rect(0, 0, 900, 1032));
        // top and bottom quarters
        QCOMPARE(map(call("partnerFrame", {js(rect(0, 0, 960, 516)), js(rect(0, 0, 960, 600)), js(rect(0, 516, 960, 516))})),
                 rect(0, 600, 960, 432));
        // not neighbours
        QVERIFY(call("partnerFrame", {js(left), js(rect(0, 0, 1060, 1032)), js(rect(1300, 100, 400, 300))}).isNull());
        // a partner squeezed to nothing is left alone
        QVERIFY(call("partnerFrame", {js(left), js(rect(0, 0, 1920, 1032)), js(right)}).isNull());
    }

    void flyoutPlace()
    {
        const QJSValue screen = js(rect(0, 0, 1920, 1080));
        const QJSValue size = js(QVariantMap{{u"width"_s, 240}, {u"height"_s, 124}});
        auto pos = [&](int x, int y) {
            return map(call("flyoutPosition", {js(QVariantMap{{u"x"_s, x}, {u"y"_s, y}}), size, screen}));
        };
        QCOMPARE(pos(1000, 40), (QVariantMap{{u"x"_s, 880}, {u"y"_s, 48}}));
        // at the right edge it stays on screen
        QCOMPARE(pos(1900, 40).value(u"x"_s).toInt(), 1920 - 240 - 8);
        // near the bottom it opens above
        QCOMPARE(pos(500, 1000).value(u"y"_s).toInt(), 1000 - 8 - 124);
    }

    void restoreUnderPointer()
    {
        // grabbed at the middle of a left half: the restored window keeps the pointer in its middle
        const QVariantMap r = map(call("restoreUnderPointer", {js(rect(0, 0, 960, 1032)), js(rect(400, 300, 800, 600)),
                                                              js(QVariantMap{{u"x"_s, 480}, {u"y"_s, 10}})}));
        QCOMPARE(r, rect(80, 0, 800, 600));
    }

    // --- Layouts.qml ----------------------------------------------------------------

    void layoutsView()
    {
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        KLocalization::setupLocalizedContext(&engine);
        QQmlComponent c(&engine);
        c.setData("import QtQuick\nimport \"../code/snap.js\" as Snap\nLayouts { layouts: Snap.layoutsFor(1920) }",
                  QUrl::fromLocalFile(QStringLiteral(RAYE_SNAP_PACKAGE "/contents/ui/harness.qml")));
        std::unique_ptr<QObject> o(c.create());
        QVERIFY2(o, qPrintable(c.errorString()));
        auto *item = qobject_cast<QQuickItem *>(o.get());
        // 3 columns of 64 with 12 between, 12 around; 2 rows of 44
        QCOMPARE(item->width(), 2 * 12 + 3 * 64 + 2 * 12.0);
        QCOMPARE(item->height(), 2 * 12 + 2 * 44 + 12.0);
        QVERIFY(findItem(item, u"zone-2-2"_s));
        auto *z = findItem(item, u"zone-0-1"_s);
        // the right half of the first miniature, 2 px in from the middle
        QCOMPARE(z->x(), 34.0);
        QCOMPARE(z->width(), 30.0);

        // the zone under a point
        const QVariantMap hit = [&] {
            QVariant r;
            QMetaObject::invokeMethod(item, "zoneAt", Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, 12 + 50), Q_ARG(QVariant, 12 + 20));
            return r.toMap();
        }();
        QCOMPARE(hit.value(u"layout"_s).toInt(), 0);
        QCOMPARE(hit.value(u"zone"_s).toInt(), 1);

        // numbers: 4 (half and quarters), then 3 (bottom quarter)
        QSignalSpy picked(item, SIGNAL(picked(int, int)));
        item->setProperty("numbered", true);
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        item->forceActiveFocus();
        QKeyEvent four(QEvent::KeyPress, Qt::Key_4, Qt::NoModifier, u"4"_s);
        QCoreApplication::sendEvent(item, &four);
        QCOMPARE(item->property("pickedLayout").toInt(), 3);
        QKeyEvent three(QEvent::KeyPress, Qt::Key_3, Qt::NoModifier, u"3"_s);
        QCoreApplication::sendEvent(item, &three);
        QCOMPARE(picked.count(), 1);
        QCOMPARE(picked.at(0).at(0).toInt(), 3);
        QCOMPARE(picked.at(0).at(1).toInt(), 2);
        // Escape steps back
        QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(item, &esc);
        QCOMPARE(item->property("pickedLayout").toInt(), -1);
        item->setParentItem(nullptr);
    }

    // --- main.qml ---------------------------------------------------------------------

    void loads()
    {
        auto run = start();
        QVERIFY(run);
        for (const QString &name : {u"Raye Snap Layouts"_s, u"Raye Snap Layouts Under Pointer"_s, u"Raye Snap Left"_s,
                                    u"Raye Snap Right"_s, u"Raye Snap Up"_s, u"Raye Snap Down"_s,
                                    u"Raye Snap Full Height"_s, u"Raye Minimize Others"_s}) {
            QVERIFY2(run->shortcut(name), qPrintable(name));
        }
        QCOMPARE(run->shortcut(u"Raye Snap Layouts"_s)->property("sequence").toString(), u"Meta+Z"_s);
        QCOMPARE(run->shortcut(u"Raye Snap Left"_s)->property("sequence").toString(), u"Meta+Left"_s);
        QCOMPARE(run->shortcut(u"Raye Snap Full Height"_s)->property("sequence").toString(), u"Meta+Shift+Up"_s);
        QCOMPARE(run->shortcut(u"Raye Minimize Others"_s)->property("sequence").toString(), u"Meta+Home"_s);
        QVERIFY(run->shortcut(u"Raye Snap Layouts Under Pointer"_s)->property("sequence").toString().isEmpty());
        // the relay is woken once
        QObject *wake = nullptr;
        for (QObject *o : run->script->children()) {
            if (o->property("dbusInterface").isValid())
                wake = o;
        }
        QVERIFY(wake);
        QCOMPARE(wake->property("service").toString(), u"org.raye.Snap"_s);
        QCOMPARE(wake->property("calls").toInt(), 1);
    }

    void windowsKeys()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        run->workspace->setProperty("activeWindow", QVariant::fromValue(w));

        run->press(u"Raye Snap Left"_s);
        QCOMPARE(frame(w), QRect(0, 0, 960, 1032));
        run->press(u"Raye Snap Up"_s);
        QCOMPARE(frame(w), QRect(0, 0, 960, 516));
        run->press(u"Raye Snap Down"_s);
        QCOMPARE(frame(w), QRect(0, 0, 960, 1032));
        // the other way: back where it was
        run->press(u"Raye Snap Right"_s);
        QCOMPARE(frame(w), QRect(200, 150, 800, 600));

        // up maximises, down restores, down again minimises
        run->press(u"Raye Snap Up"_s);
        QCOMPARE(w->property("maximizeMode").toInt(), 3);
        run->press(u"Raye Snap Down"_s);
        QCOMPARE(w->property("maximizeMode").toInt(), 0);
        QCOMPARE(frame(w), QRect(200, 150, 800, 600));
        run->press(u"Raye Snap Down"_s);
        QVERIFY(w->property("minimized").toBool());
        w->setProperty("minimized", false);

        // right, right: on to the screen on the right, left half
        run->press(u"Raye Snap Right"_s);
        QCOMPARE(frame(w), QRect(960, 0, 960, 1032));
        run->press(u"Raye Snap Right"_s);
        QCOMPARE(frame(w), QRect(1920, 0, 640, 976));
        setOutput(run.get(), w, 1);
        // and left from there: the right half of the first screen
        run->press(u"Raye Snap Left"_s);
        QCOMPARE(frame(w), QRect(960, 0, 960, 1032));
        setOutput(run.get(), w, 0);

        // full height keeps the width and the place
        // from the right half, left restores
        run->press(u"Raye Snap Left"_s);
        QCOMPARE(frame(w), QRect(200, 150, 800, 600));
        run->press(u"Raye Snap Full Height"_s);
        QCOMPARE(frame(w), QRect(200, 0, 800, 1032));
    }

    void ignoresSpecialWindows()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Panel"_s, QRectF(0, 1032, 1920, 48));
        w->setProperty("normalWindow", false);
        run->workspace->setProperty("activeWindow", QVariant::fromValue(w));
        run->press(u"Raye Snap Left"_s);
        QCOMPARE(frame(w), QRect(0, 1032, 1920, 48));
        // nothing active: nothing happens, no error
        run->workspace->setProperty("activeWindow", QVariant());
        run->press(u"Raye Snap Up"_s);
    }

    void layoutsFlyoutAndAssist()
    {
        auto run = start();
        QVERIFY(run);
        QObject *other = addWindow(run.get(), u"Files"_s, QRectF(500, 300, 900, 600));
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        run->workspace->setProperty("activeWindow", QVariant::fromValue(w));

        run->press(u"Raye Snap Layouts"_s);
        QObject *flyout = run->child(u"snapFlyout"_s);
        QVERIFY(flyout->property("visible").toBool());
        QObject *layouts = run->child(u"flyoutLayouts"_s);
        QVERIFY(layouts->property("numbered").toBool());
        // near the Maximize button of the window, on screen
        QCOMPARE(flyout->property("x").toInt(), 200 + 800 - 69 - 120);
        QCOMPARE(flyout->property("y").toInt(), 150 + 32 + 8);

        // two thirds, the big zone
        QMetaObject::invokeMethod(layouts, "picked", Q_ARG(int, 1), Q_ARG(int, 0));
        QVERIFY(!flyout->property("visible").toBool());
        QCOMPARE(frame(w), QRect(0, 0, 1280, 1032));

        // snap assist offers the other window for the free third
        QObject *assist = run->child(u"snapAssist"_s);
        QVERIFY(assist->property("visible").toBool());
        QCOMPARE(assist->property("x").toInt(), 1280);
        const auto candidates = assist->property("candidates").value<QJSValue>();
        QCOMPARE(candidates.property(u"length"_s).toInt(), 1);
        QMetaObject::invokeMethod(assist, "fill", Q_ARG(QVariant, QVariant::fromValue(other)));
        QCOMPARE(frame(other), QRect(1280, 0, 640, 1032));
        QVERIFY(!assist->property("visible").toBool());

        // the two resize together
        w->setProperty("resize", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        w->setProperty("frameGeometry", QRectF(0, 0, 1100, 1032));
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStepped", Q_ARG(QRectF, QRectF(0, 0, 1100, 1032)));
        QCOMPARE(frame(other), QRect(1100, 0, 820, 1032));
        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        w->setProperty("resize", false);

        // Escape in the flyout closes it without moving anything
        run->press(u"Raye Snap Layouts"_s);
        QMetaObject::invokeMethod(layouts, "cancelled");
        QVERIFY(!flyout->property("visible").toBool());
        QCOMPARE(frame(w), QRect(0, 0, 1100, 1032));
    }

    void assistOff()
    {
        auto run = start({{u"SnapAssist"_s, false}});
        QVERIFY(run);
        addWindow(run.get(), u"Files"_s, QRectF(500, 300, 900, 600));
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        run->workspace->setProperty("activeWindow", QVariant::fromValue(w));
        run->press(u"Raye Snap Left"_s);
        QVERIFY(!run->child(u"snapAssist"_s)->property("visible").toBool());
    }

    void assistOffersSnappedWindows()
    {
        auto run = start();
        QVERIFY(run);
        QObject *other = addWindow(run.get(), u"Files"_s, QRectF(500, 300, 900, 600));
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        // Files is snapped on its own first
        run->workspace->setProperty("activeWindow", QVariant::fromValue(other));
        run->press(u"Raye Snap Right"_s);
        QObject *assist = run->child(u"snapAssist"_s);
        QMetaObject::invokeMethod(assist, "stop");
        run->workspace->setProperty("activeWindow", QVariant::fromValue(w));
        run->press(u"Raye Snap Left"_s);
        QVERIFY(assist->property("visible").toBool());
        const auto candidates = assist->property("candidates").value<QJSValue>();
        QCOMPARE(candidates.property(u"length"_s).toInt(), 1);
    }

    void underPointer()
    {
        auto run = start();
        QVERIFY(run);
        QObject *a = addWindow(run.get(), u"Back"_s, QRectF(100, 100, 600, 400));
        QObject *b = addWindow(run.get(), u"Front"_s, QRectF(900, 200, 700, 500));
        run->workspace->setProperty("activeWindow", QVariant::fromValue(a));
        // resting on b's Maximize button
        run->workspace->setProperty("cursorPos", QPointF(1500, 215));
        run->press(u"Raye Snap Layouts Under Pointer"_s);
        QObject *flyout = run->child(u"snapFlyout"_s);
        QVERIFY(flyout->property("visible").toBool());
        QVERIFY(!run->child(u"flyoutLayouts"_s)->property("numbered").toBool());
        QCOMPARE(flyout->property("y").toInt(), 215 + 8);
        QMetaObject::invokeMethod(run->child(u"flyoutLayouts"_s), "picked", Q_ARG(int, 0), Q_ARG(int, 1));
        QCOMPARE(frame(b), QRect(960, 0, 960, 1032));
        QCOMPARE(frame(a), QRect(100, 100, 600, 400));
    }

    void snapBar()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        QObject *bar = run->child(u"snapBar"_s);
        // not near the top yet
        run->workspace->setProperty("cursorPos", QPointF(960, 300));
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStepped", Q_ARG(QRectF, QRectF()));
        QVERIFY(!bar->property("shown").toBool());
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStepped", Q_ARG(QRectF, QRectF()));
        QVERIFY(bar->property("shown").toBool());

        // the pointer over the bar's first layout, right zone
        auto *zone = findItem(qobject_cast<QQuickItem *>(run->child(u"barLayouts"_s)), u"zone-0-1"_s);
        QVERIFY(zone);
        const QPointF inBar = zone->mapToItem(qobject_cast<QQuickItem *>(run->child(u"barLayouts"_s)), QPointF(zone->width() / 2, zone->height() / 2));
        run->workspace->setProperty("cursorPos", QPointF(bar->property("x").toInt() + inBar.x(), bar->property("restY").toInt() + inBar.y()));
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStepped", Q_ARG(QRectF, QRectF()));
        QVERIFY(zone->property("lit").toBool());
        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        w->setProperty("move", false);
        QVERIFY(!bar->property("shown").toBool());
        QTRY_COMPARE(frame(w), QRect(960, 0, 960, 1032));
    }

    // Over a zone of the bar, that zone shows full size on the screen: the
    // place the window will land, growing as the pointer gets there and
    // moving when the pointer moves on.
    void snapBarShowsWhereTheWindowLands()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        QObject *bar = run->child(u"snapBar"_s);
        QVERIFY(bar->property("shown").toBool());

        QObject *preview = run->child(u"snapPreview"_s);
        QVERIFY(preview);
        QVERIFY(preview->property("visible").toBool());
        // the window is the screen's work area, the shape moves inside it
        QCOMPARE(preview->property("x").toInt(), 0);
        QCOMPARE(preview->property("height").toInt(), 1032);
        auto *window = qobject_cast<QQuickWindow *>(preview);
        QVERIFY(window);
        auto *shape = findItem(window->contentItem(), u"landingSheet"_s);
        QVERIFY(shape);
        QVERIFY(!preview->property("hasZone").toBool());
        QCOMPARE(shape->opacity(), 0.0);

        auto *layouts = qobject_cast<QQuickItem *>(run->child(u"barLayouts"_s));
        auto pointAt = [&](const QString &name) {
            auto *zone = findItem(layouts, name);
            const QPointF inBar = zone->mapToItem(layouts, QPointF(zone->width() / 2, zone->height() / 2));
            run->workspace->setProperty("cursorPos", QPointF(bar->property("x").toInt() + inBar.x(), bar->property("restY").toInt() + inBar.y()));
        };

        pointAt(u"zone-0-1"_s);
        QVERIFY(preview->property("hasZone").toBool());
        QCOMPARE(preview->property("landing").toRect(), QRect(960, 0, 960, 1032));
        // it leaves from the window: right after the pointer arrives it is not there yet
        QVERIFY(shape->width() < 960);
        QTRY_COMPARE(shape->width(), 960.0);
        QTRY_COMPARE(shape->x(), 960.0);
        QTRY_COMPARE(shape->height(), 1032.0);
        QTRY_COMPARE(shape->opacity(), 1.0);

        // on to the other zone: the same shape moves there
        pointAt(u"zone-0-0"_s);
        QCOMPARE(preview->property("landing").toRect(), QRect(0, 0, 960, 1032));
        QVERIFY(shape->x() > 0);
        QTRY_COMPARE(shape->x(), 0.0);

        // off the zones: nothing is promised
        run->workspace->setProperty("cursorPos", QPointF(bar->property("x").toInt() - 40, 300));
        QVERIFY(!preview->property("hasZone").toBool());
        QTRY_COMPARE(shape->opacity(), 0.0);

        // the drag ends: gone
        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        w->setProperty("move", false);
        QTRY_VERIFY(!preview->property("visible").toBool());
    }

    // On the second screen the shape is placed in that screen's work area.
    void previewOnTheOtherScreen()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(2000, 150, 800, 600), 1);
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        run->workspace->setProperty("cursorPos", QPointF(2560, 2));
        QObject *bar = run->child(u"snapBar"_s);
        QVERIFY(bar->property("shown").toBool());
        QObject *preview = run->child(u"snapPreview"_s);
        QCOMPARE(preview->property("x").toInt(), 1920);
        QCOMPARE(preview->property("width").toInt(), 1280);

        auto *layouts = qobject_cast<QQuickItem *>(run->child(u"barLayouts"_s));
        auto *zone = findItem(layouts, u"zone-0-1"_s);
        const QPointF inBar = zone->mapToItem(layouts, QPointF(zone->width() / 2, zone->height() / 2));
        run->workspace->setProperty("cursorPos", QPointF(bar->property("x").toInt() + inBar.x(), bar->property("restY").toInt() + inBar.y()));
        QCOMPARE(preview->property("landing").toRect(), QRect(2560, 0, 640, 976));
        auto *shape = findItem(qobject_cast<QQuickWindow *>(preview)->contentItem(), u"landingSheet"_s);
        QTRY_COMPARE(shape->x(), 640.0);
    }

    // With the bar turned off nothing shows.
    void noPreviewWithoutTheBar()
    {
        auto run = start({{u"SnapBar"_s, false}});
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        QVERIFY(!run->child(u"snapPreview"_s)->property("visible").toBool());
    }

    // KWin stacks the preview over the dragged window, so without a picture
    // of it the plain sheet leaves the window's frame clear: the window
    // reads as lying on top of the place it is about to take.
    void draggedWindowStaysClear()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        w->setProperty("minimized", true);
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        QObject *bar = run->child(u"snapBar"_s);
        QVERIFY(bar->property("shown").toBool());
        auto *preview = qobject_cast<QQuickWindow *>(run->child(u"snapPreview"_s));
        QVERIFY(preview);
        auto *landing = findItem(preview->contentItem(), u"landing"_s);
        QVERIFY(landing);
        checkHole(landing, QRectF(200, 150, 800, 600));

        // the window follows the pointer up to the bar; the hole follows it
        w->setProperty("frameGeometry", QRectF(560, 10, 800, 600));
        auto *layouts = qobject_cast<QQuickItem *>(run->child(u"barLayouts"_s));
        auto *zone = findItem(layouts, u"zone-0-0"_s);
        const QPointF inBar = zone->mapToItem(layouts, QPointF(zone->width() / 2, zone->height() / 2));
        run->workspace->setProperty("cursorPos", QPointF(bar->property("x").toInt() + inBar.x(), bar->property("restY").toInt() + inBar.y()));
        QVERIFY(preview->property("hasZone").toBool());
        checkHole(landing, QRectF(560, 10, 800, 600));

        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        w->setProperty("move", false);
        QTRY_COMPARE(landing->property("hole").toRectF(), QRectF());
    }

    // Over a zone, the zone shows the dragged window itself: its live
    // picture, never scaled, at the top left of the exact frame it will get.
    void zoneShowsTheWindow()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 450, 800, 600));
        const BarDrag d = dragToBar(run.get(), w);
        QVERIFY(d.landing);
        QVERIFY(d.bar->property("shown").toBool());
        // nothing drawn of it yet
        QVERIFY(pictures(d.landing).isEmpty());

        pointAtZone(run.get(), d, u"zone-0-1"_s);
        auto *sheet = findItem(d.landing, u"landingSheet"_s);
        QTRY_COMPARE(sheet->x(), 960.0);
        QTRY_COMPARE(sheet->opacity(), 1.0);
        // one picture of it, whatever it overlaps
        const auto shown = pictures(d.landing);
        QCOMPARE(shown.size(), 1);
        QCOMPARE(shown.first()->property("client").value<QObject *>(), w);
        // 800x600 in 960x1032: its own size, on the corner of the zone
        QTRY_COMPARE(pictureRect(shown.first(), d.landing), QRectF(960, 0, 800, 600));
        QCOMPARE(thumbnailSize(shown.first()), QSizeF(800, 600));
        QCOMPARE(d.landing->property("place").toRectF(), QRectF(960, 0, 960, 1032));
        // the window left under the pointer is covered, so it does not show
        // twice; the plain sheet is not drawn
        auto *veil = findItem(d.landing, u"landingVeil"_s);
        QVERIFY(veil && veil->isVisible());
        QCOMPARE(veil->property("color").value<QColor>().alphaF(), 1.0);
        // with its outline, just outside the frame
        QCOMPARE(QRectF(veil->x(), veil->y(), veil->width(), veil->height()), QRectF(198, 448, 804, 604));
        QList<QQuickItem *> bands;
        collectItems(d.landing, u"landingBand"_s, bands);
        for (QQuickItem *b : bands)
            QVERIFY(!b->isVisible());
    }

    // Pointing at the next zone takes the picture there.
    void pictureFollowsTheZone()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 450, 800, 600));
        const BarDrag d = dragToBar(run.get(), w);
        pointAtZone(run.get(), d, u"zone-0-1"_s);
        auto *sheet = findItem(d.landing, u"landingSheet"_s);
        QTRY_COMPARE(sheet->x(), 960.0);
        pointAtZone(run.get(), d, u"zone-0-0"_s);
        QVERIFY(sheet->x() > 0);
        QTRY_COMPARE(sheet->x(), 0.0);
        const auto shown = pictures(d.landing);
        QCOMPARE(shown.size(), 1);
        QTRY_COMPARE(pictureRect(shown.first(), d.landing), QRectF(0, 0, 800, 600));

        // a narrower zone cuts it instead of shrinking it
        pointAtZone(run.get(), d, u"zone-2-0"_s);
        QTRY_COMPARE(pictureRect(pictures(d.landing).value(0), d.landing), QRectF(0, 0, 640, 600));
        QCOMPARE(thumbnailSize(pictures(d.landing).value(0)), QSizeF(800, 600));
    }

    // Off the zones the picture is gone at once, and the window shows
    // again: a picture flying back while KWin's outline or the bar shows
    // another one was the window seen twice.
    void pictureLeavesAtOnce()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 450, 800, 600));
        const BarDrag d = dragToBar(run.get(), w);
        pointAtZone(run.get(), d, u"zone-0-1"_s);
        auto *sheet = findItem(d.landing, u"landingSheet"_s);
        QTRY_COMPARE(sheet->x(), 960.0);

        run->workspace->setProperty("cursorPos", QPointF(960, 600));
        QVERIFY(!d.bar->property("shown").toBool());
        QCOMPARE(sheet->opacity(), 0.0);
        QVERIFY(pictures(d.landing).isEmpty());
        // the preview shows empty for a moment before it goes: KWin fades a
        // window out from its last picture
        QVERIFY(d.preview->isVisible());
        QTRY_VERIFY(!d.preview->isVisible());
    }

    // The picture shows whole from its first frame: it starts on the window,
    // pixel for pixel, and a fade would show the two of them half way.
    // While it shows, the window under it is covered.
    void neverTwoImages()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 450, 800, 600));
        const BarDrag d = dragToBar(run.get(), w);
        pointAtZone(run.get(), d, u"zone-0-1"_s);
        auto *sheet = findItem(d.landing, u"landingSheet"_s);
        auto *veil = findItem(d.landing, u"landingVeil"_s);
        QElapsedTimer t;
        t.start();
        int samples = 0;
        while (t.elapsed() < 400) {
            QCOMPARE(sheet->opacity(), 1.0);
            QVERIFY(veil->isVisible());
            QCOMPARE(veil->opacity(), 1.0);
            QCOMPARE(veil->property("color").value<QColor>().alphaF(), 1.0);
            QCOMPARE(pictures(d.landing).size(), 1);
            ++samples;
            QTest::qWait(10);
        }
        QVERIFY(samples > 10);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        w->setProperty("move", false);
    }

    // Dropped on a zone: the bar goes, the picture stays until the window
    // has exactly the frame it was on, then it is gone at once.
    void droppedWhereThePictureWas()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 450, 800, 600));
        const BarDrag d = dragToBar(run.get(), w);
        pointAtZone(run.get(), d, u"zone-0-0"_s);
        QTRY_COMPARE(d.landing->property("place").toRectF(), QRectF(0, 0, 960, 1032));
        QTRY_COMPARE(findItem(d.landing, u"landingSheet"_s)->opacity(), 1.0);
        QSignalSpy hidden(d.preview, &QWindow::visibleChanged);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        w->setProperty("move", false);
        QVERIFY(!d.bar->property("shown").toBool());
        // the window is not there yet: its picture still is
        QVERIFY(d.preview->isVisible());
        QCOMPARE(pictures(d.landing).size(), 1);
        QTRY_COMPARE(frame(w), QRect(0, 0, 960, 1032));
        QTRY_VERIFY(!d.preview->isVisible());
        // hidden once, right when the window got there
        QCOMPARE(hidden.size(), 1);
        QVERIFY(pictures(d.landing).isEmpty());
    }

    // Escape during the drag: KWin puts the window back and the bar's zone
    // under the pointer is not taken, the picture goes back to the window.
    void escapeOverTheBar()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 450, 800, 600));
        const BarDrag d = dragToBar(run.get(), w);
        // the window follows the pointer up
        QMetaObject::invokeMethod(w, "frameGeometryAboutToChange");
        w->setProperty("frameGeometry", QRectF(700, 10, 800, 600));
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStepped", Q_ARG(QRectF, QRectF(700, 10, 800, 600)));
        pointAtZone(run.get(), d, u"zone-0-1"_s);
        auto *sheet = findItem(d.landing, u"landingSheet"_s);
        QTRY_COMPARE(sheet->opacity(), 1.0);

        // what KWin does on Escape: back to the start, then the end of the drag
        w->setProperty("move", false);
        QMetaObject::invokeMethod(w, "frameGeometryAboutToChange");
        w->setProperty("frameGeometry", QRectF(200, 450, 800, 600));
        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        QVERIFY(!d.bar->property("shown").toBool());
        QTRY_VERIFY(!d.preview->isVisible());
        QTest::qWait(100);
        QCOMPARE(frame(w), QRect(200, 450, 800, 600));
    }

    // A snapped window dragged off its zone and cancelled is back in it:
    // it is still snapped, with its old size to go back to.
    void escapeKeepsTheSnap()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        run->workspace->setProperty("activeWindow", QVariant::fromValue(w));
        run->press(u"Raye Snap Left"_s);
        QCOMPARE(frame(w), QRect(0, 0, 960, 1032));
        const QString id = w->property("internalId").toString();
        QVERIFY(run->script->property("restoreFrames").toMap().contains(id));

        run->workspace->setProperty("cursorPos", QPointF(480, 10));
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        // restored to its size under the pointer while it is dragged
        QCOMPARE(frame(w).size(), QSize(800, 600));
        QVERIFY(!run->script->property("restoreFrames").toMap().contains(id));

        w->setProperty("move", false);
        QMetaObject::invokeMethod(w, "frameGeometryAboutToChange");
        w->setProperty("frameGeometry", QRectF(0, 0, 960, 1032));
        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        QCOMPARE(run->script->property("restoreFrames").toMap().value(id).toMap(), rect(200, 150, 800, 600));
    }

    // At the top edge KWin maximises the window and shows that (Outline.qml):
    // the bar steps up out of the way, then comes back a little lower.
    void barStepsAsideAtTheTopEdge()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 450, 800, 600));
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        QObject *bar = run->child(u"snapBar"_s);
        QObject *preview = run->child(u"snapPreview"_s);
        run->workspace->setProperty("cursorPos", QPointF(960, 7));
        QVERIFY(bar->property("shown").toBool());
        QVERIFY(!bar->property("tucked").toBool());
        QTRY_COMPARE(bar->property("y").toInt(), 8);

        run->workspace->setProperty("cursorPos", QPointF(960, 3));
        QVERIFY(bar->property("tucked").toBool());
        QVERIFY(!preview->property("hasZone").toBool());
        QTRY_VERIFY(bar->property("y").toInt() + bar->property("height").toInt() <= 0);
        // just below the edge it stays up, so it does not flap
        run->workspace->setProperty("cursorPos", QPointF(960, 9));
        QVERIFY(bar->property("tucked").toBool());

        // on the bar: back down, and its zones work again
        auto *layouts = qobject_cast<QQuickItem *>(run->child(u"barLayouts"_s));
        auto *zone = findItem(layouts, u"zone-0-1"_s);
        const QPointF p = zone->mapToItem(layouts, QPointF(zone->width() / 2, zone->height() / 2));
        run->workspace->setProperty("cursorPos", QPointF(bar->property("x").toInt() + p.x(), 8 + p.y()));
        QVERIFY(!bar->property("tucked").toBool());
        QVERIFY(preview->property("hasZone").toBool());
        QTRY_COMPARE(bar->property("y").toInt(), 8);

        // dropped at the edge: KWin's maximise, not a zone
        run->workspace->setProperty("cursorPos", QPointF(960, 0));
        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        w->setProperty("move", false);
        QTest::qWait(50);
        QCOMPARE(frame(w), QRect(200, 450, 800, 600));
    }

    // With KWin's maximise at the edge turned off the bar stays put.
    void barStaysWithoutTheMaximizeEdge()
    {
        auto run = start();
        QVERIFY(run);
        QObject *options = run->engine->singletonInstance<QObject *>("org.kde.kwin", "Options");
        options->setProperty("electricBorderMaximize", false);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 450, 800, 600));
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        QObject *bar = run->child(u"snapBar"_s);
        QVERIFY(bar->property("shown").toBool());
        QVERIFY(!bar->property("tucked").toBool());
    }

    // The bar slides down as one piece: its layouts sit still in it, so
    // there is never an empty frame on screen.
    void barSlidesInWhole()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 450, 800, 600));
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        run->workspace->setProperty("cursorPos", QPointF(960, 7));
        QObject *bar = run->child(u"snapBar"_s);
        auto *layouts = qobject_cast<QQuickItem *>(run->child(u"barLayouts"_s));
        QVERIFY(bar->property("shown").toBool());
        QVERIFY(bar->property("y").toInt() < 8);
        const QPointF inside = layouts->mapToScene(QPointF(0, 0));
        QTRY_COMPARE(bar->property("y").toInt(), 8);
        QCOMPARE(layouts->mapToScene(QPointF(0, 0)), inside);
    }

    // In a small zone the picture is cut where the frame ends, on a solid
    // sheet the window cannot show through.
    void pictureFillsASmallZone()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(100, 300, 1200, 900));
        const BarDrag d = dragToBar(run.get(), w);
        pointAtZone(run.get(), d, u"zone-4-3"_s);
        auto *sheet = findItem(d.landing, u"landingSheet"_s);
        QTRY_COMPARE(sheet->x(), 960.0);
        QTRY_COMPARE(sheet->y(), 516.0);
        const auto shown = pictures(d.landing);
        QCOMPARE(shown.size(), 1);
        QTRY_COMPARE(pictureRect(shown.first(), d.landing), QRectF(960, 516, 960, 516));
        auto *cut = shown.first()->parentItem();
        QVERIFY(cut->clip());
        QCOMPARE(thumbnailSize(shown.first()), QSizeF(1200, 900));

        auto *glass = findItem(d.landing, u"landingGlass"_s);
        QVERIFY(glass && glass->isVisible());
        QCOMPARE(glass->property("color").value<QColor>().alphaF(), 1.0);
        auto *veil = findItem(d.landing, u"landingVeil"_s);
        QCOMPARE(veil->property("color").value<QColor>().alphaF(), 1.0);
    }

    // All the way from the window to the zone the picture keeps the window's
    // own size and sits on whole screen pixels: nothing is stretched, so
    // nothing goes soft. On a 1366x768 screen and a 1920x1080 one, at 100 %
    // and 125 %, and onto a zone larger and smaller than the window.
    void pictureIsNeverScaled_data()
    {
        QTest::addColumn<QRectF>("screen");
        QTest::addColumn<qreal>("ratio");
        QTest::addColumn<QString>("zone");
        QTest::newRow("1920x1080 half") << QRectF(0, 0, 1920, 1080) << 1.0 << u"zone-0-1"_s;
        QTest::newRow("1920x1080 quarter 125%") << QRectF(0, 0, 1920, 1080) << 1.25 << u"zone-4-3"_s;
        QTest::newRow("1366x768 half") << QRectF(0, 0, 1366, 768) << 1.0 << u"zone-0-1"_s;
        QTest::newRow("1366x768 third 125%") << QRectF(0, 0, 1366, 768) << 1.25 << u"zone-2-0"_s;
    }

    void pictureIsNeverScaled()
    {
        QFETCH(QRectF, screen);
        QFETCH(qreal, ratio);
        QFETCH(QString, zone);
        auto run = start();
        QVERIFY(run);
        run->outputs.at(0)->setProperty("geometry", screen);
        const QRectF window(screen.width() / 5, screen.height() / 3, 701, 417);
        QObject *w = addWindow(run.get(), u"Notes"_s, window);
        const BarDrag d = dragToBar(run.get(), w);
        run->workspace->setProperty("cursorPos", QPointF(screen.center().x(), 2));
        QVERIFY(d.bar->property("shown").toBool());
        d.landing->setProperty("pixelRatio", ratio);
        pointAtZone(run.get(), d, zone);
        const QRectF place = d.landing->property("place").toRectF();
        QVERIFY(place.width() > 0);
        int samples = 0;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 600) {
            for (QQuickItem *p : pictures(d.landing)) {
                ++samples;
                QCOMPARE(thumbnailSize(p), window.size());
                const QPointF at = p->mapToItem(d.landing, QPointF(0, 0));
                QCOMPARE(qRound(at.x() * ratio), at.x() * ratio);
                QCOMPARE(qRound(at.y() * ratio), at.y() * ratio);
            }
            QTest::qWait(10);
        }
        QVERIFY(samples > 10);
        // and it ends on the zone (on the pixel the screen puts it on), cut to it
        const auto shown = pictures(d.landing);
        QCOMPARE(shown.size(), 1);
        const QPointF corner(qRound(place.x() * ratio) / ratio, qRound(place.y() * ratio) / ratio);
        const QRectF got = pictureRect(shown.first(), d.landing);
        QCOMPARE(got.topLeft(), corner);
        QCOMPARE(got.width(), qMin(window.width(), place.right() - corner.x()));
        QCOMPARE(got.height(), qMin(window.height(), place.bottom() - corner.y()));
    }

    // A window KWin has no picture of (minimised) gets the plain sheet.
    void noPictureFallsBackToTheSheet()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 450, 800, 600));
        w->setProperty("minimized", true);
        const BarDrag d = dragToBar(run.get(), w);
        pointAtZone(run.get(), d, u"zone-0-1"_s);
        auto *sheet = findItem(d.landing, u"landingSheet"_s);
        QTRY_COMPARE(sheet->x(), 960.0 + 6);
        QTRY_COMPARE(sheet->width(), 960.0 - 2 * 6);
        QTRY_COMPARE(sheet->opacity(), 1.0);
        QVERIFY(pictures(d.landing).isEmpty());
    }

    // Crossing the gap between two layouts of the bar keeps a place lit:
    // the sheet travels from one to the next instead of vanishing and
    // growing back.
    void snapBarHasNoDeadGaps()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        QObject *bar = run->child(u"snapBar"_s);
        QVERIFY(bar->property("shown").toBool());
        auto *layouts = qobject_cast<QQuickItem *>(run->child(u"barLayouts"_s));
        auto *left = findItem(layouts, u"zone-0-1"_s);
        auto *right = findItem(layouts, u"zone-1-0"_s);
        const QPointF a = left->mapToItem(layouts, QPointF(left->width(), left->height() / 2));
        const QPointF b = right->mapToItem(layouts, QPointF(0, right->height() / 2));
        auto pointAt = [&](const QPointF &p) {
            run->workspace->setProperty("cursorPos", QPointF(bar->property("x").toInt() + p.x(), bar->property("restY").toInt() + p.y()));
        };
        QObject *preview = run->child(u"snapPreview"_s);

        pointAt(QPointF(a.x() + 2, a.y()));
        QVERIFY(preview->property("hasZone").toBool());
        QCOMPARE(bar->property("target").toMap().value(u"layout"_s).toInt(), 0);
        pointAt(QPointF(b.x() - 2, b.y()));
        QVERIFY(preview->property("hasZone").toBool());
        QCOMPARE(bar->property("target").toMap().value(u"layout"_s).toInt(), 1);

        // the bar's own padding, above the zones, still points at one
        pointAt(QPointF(b.x() + 4, 2));
        QVERIFY(preview->property("hasZone").toBool());
    }

    // KWin's own quick tile outline (an edge, a corner, the top to maximise)
    // draws the same sheet as the snap bar, starting from the window.
    void outlineDrawsTheSameSheet()
    {
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(RAYE_SNAP_TESTS "/mock"));
        engine.addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        QQmlComponent moving(&engine);
        moving.setData("import org.kde.kwin\nFakeWindow { frameGeometry: Qt.rect(200, 150, 800, 600); move: true }",
                       QUrl(u"file:outline-window.qml"_s));
        std::unique_ptr<QObject> dragged(moving.create());
        QVERIFY(dragged);
        QQmlComponent fake(&engine);
        fake.setData("import QtQuick\nQtObject { property rect geometry; property rect visualParentGeometry; "
                     "property rect unifiedGeometry; property bool active }",
                     QUrl(u"file:outline-fake.qml"_s));
        std::unique_ptr<QObject> outline(fake.create());
        QVERIFY(outline);
        outline->setProperty("unifiedGeometry", QRect(0, 0, 1920, 1032));
        outline->setProperty("visualParentGeometry", QRect(200, 150, 800, 600));
        outline->setProperty("geometry", QRect(0, 0, 960, 1032));

        QQmlContext context(engine.rootContext());
        context.setContextProperty(u"outline"_s, outline.get());
        QQmlComponent c(&engine, QUrl::fromLocalFile(QStringLiteral(RAYE_SNAP_PACKAGE "/contents/ui/Outline.qml")));
        std::unique_ptr<QObject> made(c.create(&context));
        QVERIFY2(made, qPrintable(c.errorString()));
        QObject *workspace = engine.singletonInstance<QObject *>("org.kde.kwin", "Workspace");
        QVERIFY(workspace);
        workspace->setProperty("activeWindow", QVariant::fromValue(dragged.get()));
        auto *window = qobject_cast<QQuickWindow *>(made.get());
        QVERIFY(window);
        QVERIFY(!window->isVisible());

        outline->setProperty("active", true);
        QVERIFY(window->isVisible());
        QCOMPARE(window->geometry(), QRect(0, 0, 1920, 1032));
        auto *sheet = findItem(window->contentItem(), u"landingSheet"_s);
        QVERIFY(sheet);
        // it leaves from the window's frame, then settles in the half
        QCOMPARE(sheet->x(), 200.0);
        QCOMPARE(sheet->width(), 800.0);
        QTRY_COMPARE(sheet->x(), 0.0);
        QTRY_COMPARE(sheet->width(), 960.0);
        QTRY_COMPARE(sheet->opacity(), 1.0);
        // the window being moved shows in it, the window itself veiled
        QCOMPARE(pictures(window->contentItem()).size(), 1);
        QVERIFY(findItem(window->contentItem(), u"landingVeil"_s)->isVisible());

        // the pointer moves on to the other half: the same sheet goes there
        outline->setProperty("geometry", QRect(960, 0, 960, 1032));
        QVERIFY(sheet->x() < 960.0);
        QTRY_COMPARE(sheet->x(), 960.0);
        QCOMPARE(pictures(window->contentItem()).value(0)->property("client").value<QObject *>(), dragged.get());

        // a window KWin has no picture of: the plain sheet, clear of it
        dragged->setProperty("minimized", true);
        checkHole(findItem(window->contentItem(), u"landing"_s), QRectF(200, 150, 800, 600));
        QVERIFY(pictures(window->contentItem()).isEmpty());
        dragged->setProperty("minimized", false);

        // away from the edge, still dragging: the picture is gone at once,
        // and the window goes once it has shown empty
        outline->setProperty("active", false);
        QVERIFY(pictures(window->contentItem()).isEmpty());
        QCOMPARE(sheet->opacity(), 0.0);
        QTRY_VERIFY(!window->isVisible());

        // dropped at the edge: gone at once, the window takes its place
        outline->setProperty("active", true);
        QTRY_COMPARE(sheet->opacity(), 1.0);
        dragged->setProperty("move", false);
        outline->setProperty("active", false);
        QVERIFY(pictures(window->contentItem()).isEmpty());
        QTRY_VERIFY(!window->isVisible());
    }

    // The cards of snap assist fade in, they do not just appear.
    void assistCardsFadeIn()
    {
        auto run = start();
        QVERIFY(run);
        addWindow(run.get(), u"Files"_s, QRectF(500, 300, 900, 600));
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        run->workspace->setProperty("activeWindow", QVariant::fromValue(w));
        run->press(u"Raye Snap Layouts"_s);
        QMetaObject::invokeMethod(run->child(u"flyoutLayouts"_s), "picked", Q_ARG(int, 0), Q_ARG(int, 0));
        QObject *assist = run->child(u"snapAssist"_s);
        QVERIFY(assist->property("visible").toBool());
        auto *dialog = qobject_cast<QQuickWindow *>(assist);
        QVERIFY(dialog);
        auto *cards = findItem(dialog->contentItem(), u"snapAssistCards"_s);
        QVERIFY(cards);
        QVERIFY(cards->opacity() < 1.0);
        QTRY_COMPARE(cards->opacity(), 1.0);
    }

    void snapBarOnByDefault()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        // near the top KWin no longer moves the window: no step, only the pointer
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        QVERIFY(run->child(u"snapBar"_s)->property("shown").toBool());
        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        w->setProperty("move", false);
        QVERIFY(!run->child(u"snapBar"_s)->property("shown").toBool());
        // after the drag the pointer alone shows nothing
        run->workspace->setProperty("cursorPos", QPointF(960, 300));
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        QVERIFY(!run->child(u"snapBar"_s)->property("shown").toBool());
    }

    void snapBarOff()
    {
        auto run = start({{u"SnapBar"_s, false}});
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStepped", Q_ARG(QRectF, QRectF()));
        QVERIFY(!run->child(u"snapBar"_s)->property("shown").toBool());
    }

    void settingsFollowed()
    {
        auto run = start();
        QVERIFY(run);
        QObject *w = addWindow(run.get(), u"Notes"_s, QRectF(200, 150, 800, 600));
        run->workspace->setProperty("activeWindow", QVariant::fromValue(w));
        // Settings turns the bar and resting on Maximize off, KWin reloads
        run->kwin->setProperty("config", QVariantMap{{u"SnapBar"_s, false}, {u"SnapLayoutsOnHover"_s, false}});
        QObject *options = run->engine->singletonInstance<QObject *>("org.kde.kwin", "Options");
        QVERIFY(options);
        QMetaObject::invokeMethod(options, "configChanged");

        w->setProperty("move", true);
        QMetaObject::invokeMethod(w, "interactiveMoveResizeStarted");
        run->workspace->setProperty("cursorPos", QPointF(960, 2));
        QVERIFY(!run->child(u"snapBar"_s)->property("shown").toBool());
        QMetaObject::invokeMethod(w, "interactiveMoveResizeFinished");
        w->setProperty("move", false);

        run->workspace->setProperty("cursorPos", QPointF(900, 160));
        run->press(u"Raye Snap Layouts Under Pointer"_s);
        QVERIFY(!run->child(u"snapFlyout"_s)->property("visible").toBool());
        // Win+Z still opens them
        run->press(u"Raye Snap Layouts"_s);
        QVERIFY(run->child(u"snapFlyout"_s)->property("visible").toBool());
    }

    void minimizeOthers()
    {
        auto run = start();
        QVERIFY(run);
        QObject *a = addWindow(run.get(), u"A"_s, QRectF(0, 0, 400, 300));
        QObject *b = addWindow(run.get(), u"B"_s, QRectF(100, 100, 400, 300));
        QObject *c = addWindow(run.get(), u"C"_s, QRectF(2000, 100, 400, 300), 1);
        QObject *d = addWindow(run.get(), u"D"_s, QRectF(200, 200, 400, 300));
        d->setProperty("minimized", true);
        run->workspace->setProperty("activeWindow", QVariant::fromValue(a));
        run->press(u"Raye Minimize Others"_s);
        QVERIFY(!a->property("minimized").toBool());
        QVERIFY(b->property("minimized").toBool());
        // only on the active window's screen
        QVERIFY(!c->property("minimized").toBool());
        run->press(u"Raye Minimize Others"_s);
        QVERIFY(!b->property("minimized").toBool());
        // what was minimised before stays so
        QVERIFY(d->property("minimized").toBool());
    }

    void french()
    {
        // every string of the script has its French
        QFile po(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_snap.po"));
        QVERIFY(po.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(po.readAll());
        static const QRegularExpression callRe(u"i18nd\\(\"raye_shell_snap\", \"([^\"]+)\"\\)"_s);
        int count = 0;
        for (const QString &file : {u"main.qml"_s, u"Layouts.qml"_s}) {
            QFile f(QStringLiteral(RAYE_SNAP_PACKAGE "/contents/ui/") + file);
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QString source = QString::fromUtf8(f.readAll());
            QVERIFY(!source.contains(u"i18n(\""_s));
            for (auto it = callRe.globalMatch(source); it.hasNext(); ++count) {
                const QString id = it.next().captured(1);
                const QRegularExpression entry(u"msgid \"%1\"\\nmsgstr \"([^\"]+)\""_s.arg(QRegularExpression::escape(id)));
                const auto m = entry.match(text);
                QVERIFY2(m.hasMatch(), qPrintable(id));
                QVERIFY2(m.captured(1) != id, qPrintable(id));
            }
        }
        QCOMPARE(count, 9);
    }
};

QTEST_MAIN(TestSnap)
#include "tst_snap.moc"
