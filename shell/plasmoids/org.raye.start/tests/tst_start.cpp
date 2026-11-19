// The Start applet: its decisions (startlogic.js), the row of taskbar
// buttons in each search style (StartRow.qml) and the popup frame
// switching between Start and Search (StartFrame.qml, with the views'
// stand-ins of this package).
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>

#include <cstdio>

#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QJSEngine>
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TestStart : public QObject
{
    Q_OBJECT

private:
    QJSEngine m_js;
    QJSEngine m_menu;
    QJSEngine m_links;
    QJSEngine m_tiles;
    QStringList m_warnings;
    static inline TestStart *s_self = nullptr;

    static void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
    {
        if (type != QtDebugMsg && type != QtInfoMsg && msg.contains(u"org.raye.start"_s))
            s_self->m_warnings << msg;
    }

    QJSValue call(const char *name, const QJSValueList &args)
    {
        const QJSValue r = m_js.globalObject().property(QLatin1String(name)).call(args);
        if (r.isError())
            qWarning() << name << r.toString();
        return r;
    }

    QJSValue rect(int x, int y, int w, int h)
    {
        QJSValue r = m_js.newObject();
        r.setProperty(u"x"_s, x);
        r.setProperty(u"y"_s, y);
        r.setProperty(u"width"_s, w);
        r.setProperty(u"height"_s, h);
        return r;
    }

    QJSValue menu(const char *name, const QJSValueList &args)
    {
        const QJSValue r = m_menu.globalObject().property(QLatin1String(name)).call(args);
        if (r.isError())
            qWarning() << name << r.toString();
        return r;
    }

    // a JS expression on tileslogic.js, as JSON
    QString tiles(const QString &expr)
    {
        const QJSValue r = m_tiles.evaluate(u"JSON.stringify("_s + expr + u")"_s);
        if (r.isError())
            qWarning() << expr << r.toString();
        return r.toString();
    }

    QJSValue links(const char *name, const QJSValueList &args)
    {
        const QJSValue r = m_links.globalObject().property(QLatin1String(name)).call(args);
        if (r.isError())
            qWarning() << name << r.toString();
        return r;
    }

    // the rows of the quick link menu when only `installed` apps are there
    QStringList linkIds(const QStringList &installed)
    {
        QJSValue have = m_links.newArray(installed.size());
        for (int i = 0; i < installed.size(); ++i)
            have.setProperty(i, installed.at(i));
        m_links.globalObject().setProperty(u"testInstalled"_s, have);
        const QJSValue rows = m_links.evaluate(
            u"rows(mainEntries, function (id) { return testInstalled.indexOf(id) >= 0 }, {})"_s);
        QStringList out;
        const int n = rows.property(u"length"_s).toInt();
        for (int i = 0; i < n; ++i) {
            const QJSValue row = rows.property(i);
            out << (row.property(u"separator"_s).toBool() ? u"-"_s : row.property(u"id"_s).toString());
        }
        return out;
    }

    // the window of a PanelMenu that is open
    static QQuickWindow *openMenuWindow(QQuickWindow *not_)
    {
        const auto all = QGuiApplication::allWindows();
        for (QWindow *w : all) {
            auto *q = qobject_cast<QQuickWindow *>(w);
            if (q && q != not_ && q->isVisible())
                return q;
        }
        return nullptr;
    }

    QJSValue list(const QStringList &items)
    {
        QJSValue a = m_menu.newArray(items.size());
        for (int i = 0; i < items.size(); ++i)
            a.setProperty(i, items.at(i));
        return a;
    }

    static QStringList strings(const QJSValue &v)
    {
        QStringList out;
        const int n = v.property(u"length"_s).toInt();
        for (int i = 0; i < n; ++i)
            out << v.property(i).toString();
        return out;
    }

    struct View {
        std::unique_ptr<QQmlEngine> engine;
        std::unique_ptr<QObject> fixture;
        QQuickWindow *window = nullptr;
        QQuickItem *item = nullptr;
    };

    std::unique_ptr<View> view(const QString &what, const QVariantMap &options = {})
    {
        auto v = std::make_unique<View>();
        v->engine = std::make_unique<QQmlEngine>();
        v->engine->addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        v->engine->addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        KLocalization::setupLocalizedContext(v->engine.get());
        QQmlComponent c(v->engine.get(), QUrl::fromLocalFile(QStringLiteral(RAYE_START_TESTS "/fixture.qml")));
        QObject *o = c.beginCreate(v->engine->rootContext());
        if (!o) {
            qWarning() << c.errors();
            return nullptr;
        }
        o->setProperty("uiDir", QUrl::fromLocalFile(QStringLiteral(RAYE_START_DIR "/contents/ui/")));
        o->setProperty("what", what);
        o->setProperty("withBackend", options.value(u"withBackend"_s).toBool());
        if (options.contains(u"autoOpen"_s))
            o->setProperty("autoOpen", options.value(u"autoOpen"_s).toBool());
        c.completeCreate();
        v->fixture.reset(o);
        for (auto it = options.cbegin(); it != options.cend(); ++it)
            if (it.key() != u"withBackend"_s && it.key() != u"autoOpen"_s)
                QMetaObject::invokeMethod(o, "setOption", Q_ARG(QVariant, it.key()), Q_ARG(QVariant, it.value()));
        v->window = qobject_cast<QQuickWindow *>(o);
        v->item = o->property("item").value<QQuickItem *>();
        if (!v->window || !v->item)
            return nullptr;
        v->window->show();
        if (!QTest::qWaitForWindowExposed(v->window))
            return nullptr;
        QTest::qWait(50);
        return v;
    }

    static QQuickItem *child(QQuickItem *root, const QString &name)
    {
        return root->findChild<QQuickItem *>(name);
    }

    // a view loader of the frame is done (Loader.Ready)
    static bool loaderReady(QQuickItem *frame, const QString &name)
    {
        QQuickItem *l = child(frame, name);
        return l && l->property("status").toInt() == 1;
    }

    QStringList calls(View *v)
    {
        return v->fixture->property("calls").toStringList();
    }

    void click(View *v, QQuickItem *item, Qt::MouseButton button = Qt::LeftButton)
    {
        QTest::mouseClick(v->window, button, {}, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
    }

public Q_SLOTS:
    void lidMoved()
    {
        if (m_eye)
            m_lowestLid = std::min(m_lowestLid, m_eye->property("lid").toReal());
    }

private:
    // Writes the person's rayeshellrc for one test
    struct StartConfig {
        explicit StartConfig(const QString &lines)
            : path(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/rayeshellrc"_s)
        {
            QDir().mkpath(QFileInfo(path).path());
            QFile f(path);
            f.open(QIODevice::WriteOnly | QIODevice::Truncate);
            f.write(lines.toUtf8());
        }
        ~StartConfig() { QFile::remove(path); }
        QString path;
    };

    QQuickItem *m_eye = nullptr;
    qreal m_lowestLid = 1.0;

    // Where the content is at each frame the window paints, from the
    // moment Start is asked to open.
    struct Sample {
        qint64 ms;
        qreal y;
        qreal opacity;
    };

    void openAndWatch(View *v, int stallMs, QList<Sample> &out)
    {
        QQuickItem *content = child(v->item, u"startContent"_s);
        QVERIFY(content);
        QElapsedTimer clock;
        auto conn = connect(v->window, &QQuickWindow::frameSwapped, this, [&] {
            out << Sample{clock.elapsed(), content->y(), content->opacity()};
        }, Qt::QueuedConnection);
        clock.start();
        QMetaObject::invokeMethod(v->item, "reset", Q_ARG(QVariant, u"start"_s), Q_ARG(QVariant, QString()));
        QMetaObject::invokeMethod(v->item, "present");
        // the window takes a while to appear (the compositor maps it, a
        // slow machine paints its first frame) while the rest runs on
        QTest::qWait(stallMs);
        v->window->show();
        QTRY_VERIFY_WITH_TIMEOUT(content->y() == 0 && content->opacity() == 1.0, 3000);
        QTest::qWait(50);
        disconnect(conn);
    }

private Q_SLOTS:
    void initTestCase()
    {
        s_self = this;
        QFile f(QStringLiteral(RAYE_START_DIR "/contents/ui/startlogic.js"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJSValue r = m_js.evaluate(QString::fromUtf8(f.readAll()).remove(u".pragma library"_s));
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
        QFile m(QStringLiteral(RAYE_START_DIR "/contents/ui/menulogic.js"));
        QVERIFY(m.open(QIODevice::ReadOnly));
        const QJSValue rm = m_menu.evaluate(QString::fromUtf8(m.readAll()).remove(u".pragma library"_s));
        QVERIFY2(!rm.isError(), qPrintable(rm.toString()));
        QFile q(QStringLiteral(RAYE_START_DIR "/contents/ui/quicklinks.js"));
        QVERIFY(q.open(QIODevice::ReadOnly));
        const QJSValue rq = m_links.evaluate(QString::fromUtf8(q.readAll()).remove(u".pragma library"_s));
        QVERIFY2(!rq.isError(), qPrintable(rq.toString()));
        QFile t(QStringLiteral(RAYE_START_DIR "/contents/ui/tileslogic.js"));
        QVERIFY(t.open(QIODevice::ReadOnly));
        const QJSValue rt = m_tiles.evaluate(QString::fromUtf8(t.readAll()).remove(u".pragma library"_s));
        QVERIFY2(!rt.isError(), qPrintable(rt.toString()));
        qInstallMessageHandler(collect);
    }

    // Win+X, Win+S...: one global shortcut for every Start of the session.
    // It went to the Start used last, so with a second screen the menu could
    // open there, out of sight, while the person looked at the other one.
    // It goes to the Start of the screen in use (the pointer's), and to the
    // one used last only when no Start is on that screen.
    void aShortcutOpensTheStartOfTheScreenInUse()
    {
        QJSEngine js;
        QFile f(QStringLiteral(RAYE_START_DIR "/contents/ui/registry.js"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJSValue r = js.evaluate(QString::fromUtf8(f.readAll()).remove(u".pragma library"_s));
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
        const QJSValue made = js.evaluate(uR"(
            var left = { name: "left", screenRect: { x: 0, y: 0, width: 1280, height: 800 } }
            var right = { name: "right", screenRect: { x: 1280, y: 0, width: 1920, height: 1080 } }
            add(right); add(left); used(right)
            var r = []
            r.push(target("raye-quick-links", 1000, { x: 0, y: 0, width: 1280, height: 800 }).name)
            r.push(target("raye-quick-links", 1050, { x: 0, y: 0, width: 1280, height: 800 }) === null)
            r.push(target("raye-quick-links", 2000, { x: 1280, y: 0, width: 1920, height: 1080 }).name)
            r.push(target("raye-search", 3000, { x: 5000, y: 0, width: 100, height: 100 }).name)
            r.push(target("raye-search", 4000).name)
            r
        )"_s);
        QVERIFY2(!made.isError(), qPrintable(made.toString()));
        QCOMPARE(made.property(0).toString(), u"left"_s);
        QVERIFY(made.property(1).toBool());
        QCOMPARE(made.property(2).toString(), u"right"_s);
        QCOMPARE(made.property(3).toString(), u"right"_s);
        QCOMPARE(made.property(4).toString(), u"right"_s);
    }

    void cleanupTestCase()
    {
        qInstallMessageHandler(nullptr);
    }

    void init()
    {
        m_warnings.clear();
    }

    void cleanup()
    {
        QVERIFY2(m_warnings.isEmpty(), qPrintable(m_warnings.join(u'\n')));
    }


    void searchStyles()
    {
        QCOMPARE(call("searchStyle", {u"box"_s}).toString(), u"box"_s);
        QCOMPARE(call("searchStyle", {u"iconLabel"_s}).toString(), u"iconlabel"_s);
        QCOMPARE(call("searchStyle", {u"hidden"_s}).toString(), u"hidden"_s);
        QCOMPARE(call("searchStyle", {u"nonsense"_s}).toString(), u"box"_s);
        QCOMPARE(call("searchStyle", {QJSValue()}).toString(), u"box"_s);
        QCOMPARE(call("searchWidth", {u"box"_s}).toInt(), 232);
        QCOMPARE(call("searchWidth", {u"iconlabel"_s}).toInt(), 104);
        QCOMPARE(call("searchWidth", {u"icon"_s}).toInt(), 40);
        QCOMPARE(call("searchWidth", {u"hidden"_s}).toInt(), 0);
    }

    void rowWidths()
    {
        QCOMPARE(call("rowWidth", {u"box"_s, true}).toInt(), 44 + 236 + 44);
        QCOMPARE(call("rowWidth", {u"icon"_s, true}).toInt(), 3 * 44);
        QCOMPARE(call("rowWidth", {u"hidden"_s, false}).toInt(), 44);
        QCOMPARE(call("rowWidth", {u"iconlabel"_s, false}).toInt(), 44 + 108);
    }

    void centring()
    {
        // 1920 screen, Start applet at 0, row 324, apps 440: group 764 wide, left at 578
        QCOMPARE(call("leadingSpace", {u"center"_s, 1920, 0, 324, 440}).toInt(), 578);
        // the applet starts further right (a widget before it)
        QCOMPARE(call("leadingSpace", {u"center"_s, 1920, 100, 324, 440}).toInt(), 478);
        // group too wide: no room before it
        QCOMPARE(call("leadingSpace", {u"center"_s, 1280, 0, 324, 1500}).toInt(), 0);
        QCOMPARE(call("leadingSpace", {u"left"_s, 1920, 0, 324, 440}).toInt(), 8);
        QCOMPARE(call("leadingSpace", {u"left"_s, 1920, 40, 324, 440}).toInt(), 0);
    }

    void popup()
    {
        QJSValue small = call("popupSize", {1280, 800, 48});
        QCOMPARE(small.property(u"width"_s).toInt(), 680);
        QCOMPARE(small.property(u"height"_s).toInt(), 728);
        QJSValue wide = call("popupSize", {1920, 1080, 48});
        QCOMPARE(wide.property(u"width"_s).toInt(), 840);
        QCOMPARE(wide.property(u"height"_s).toInt(), 760);
        QJSValue tiny = call("popupSize", {640, 480, 48});
        QCOMPARE(tiny.property(u"width"_s).toInt(), 624);
        QCOMPARE(tiny.property(u"height"_s).toInt(), 408);

        QJSValue centred = call("popupPosition", {u"center"_s, rect(0, 0, 1920, 1080), 1032, 840, 760});
        QCOMPARE(centred.property(u"x"_s).toInt(), 540);
        QCOMPARE(centred.property(u"y"_s).toInt(), 1032 - 12 - 760);
        QJSValue left = call("popupPosition", {u"left"_s, rect(1920, 0, 1280, 800), 752, 680, 728});
        QCOMPARE(left.property(u"x"_s).toInt(), 1932);
        QCOMPARE(left.property(u"y"_s).toInt(), 12);
        // never above the screen
        QCOMPARE(call("popupPosition", {u"center"_s, rect(0, 0, 1280, 800), 752, 680, 900}).property(u"y"_s).toInt(), 0);
    }

    void typedText()
    {
        QVERIFY(call("isTypedText", {u"a"_s, false, false, false}).toBool());
        QVERIFY(call("isTypedText", {u"é"_s, false, false, false}).toBool());
        QVERIFY(call("isTypedText", {u" "_s, false, false, false}).toBool());
        QVERIFY(!call("isTypedText", {u"a"_s, true, false, false}).toBool());
        QVERIFY(!call("isTypedText", {u"\t"_s, false, false, false}).toBool());
        QVERIFY(!call("isTypedText", {u"\x1b"_s, false, false, false}).toBool());
        QVERIFY(!call("isTypedText", {QString(), false, false, false}).toBool());
    }


    void rowBox()
    {
        auto v = view(u"row"_s);
        QVERIFY(v);
        QVERIFY(child(v->item, u"startButton"_s)->isVisible());
        QQuickItem *box = child(v->item, u"searchBox"_s);
        QVERIFY(box->isVisible());
        QCOMPARE(box->width(), 232.0);
        QCOMPARE(box->height(), 32.0);
        QVERIFY(!child(v->item, u"searchIcon"_s)->isVisible());
        QVERIFY(child(v->item, u"taskViewButton"_s)->isVisible());
        QCOMPARE(v->item->property("rowWidth").toInt(), 324);
        QCOMPARE(v->item->implicitWidth(), 324.0);
    }

    void rowStyles()
    {
        auto v = view(u"row"_s, {{u"searchMode"_s, u"icon"_s}, {u"taskView"_s, false}});
        QVERIFY(v);
        QVERIFY(!child(v->item, u"searchBox"_s)->isVisible());
        QVERIFY(child(v->item, u"searchIcon"_s)->isVisible());
        QVERIFY(!child(v->item, u"taskViewButton"_s)->isVisible());
        QCOMPARE(v->item->property("rowWidth").toInt(), 88);

        QMetaObject::invokeMethod(v->fixture.get(), "setOption", Q_ARG(QVariant, u"searchMode"_s), Q_ARG(QVariant, u"iconlabel"_s));
        QTRY_VERIFY(child(v->item, u"searchLabel"_s)->isVisible());
        QCOMPARE(child(v->item, u"searchLabel"_s)->width(), 104.0);

        QMetaObject::invokeMethod(v->fixture.get(), "setOption", Q_ARG(QVariant, u"searchMode"_s), Q_ARG(QVariant, u"hidden"_s));
        QTRY_COMPARE(v->item->property("rowWidth").toInt(), 44);
        QVERIFY(!child(v->item, u"searchLabel"_s)->isVisible());
        QVERIFY(!child(v->item, u"searchIcon"_s)->isVisible());
    }

    void rowLeadingSpace()
    {
        auto v = view(u"row"_s, {{u"leadingSpace"_s, 300}});
        QVERIFY(v);
        // it slides there (250 ms)
        QTRY_COMPARE(child(v->item, u"startButton"_s)->mapToItem(v->item, QPointF(0, 0)).x(), 300.0);
        QTRY_COMPARE(v->item->implicitWidth(), 624.0);
    }

    void rowClicks()
    {
        auto v = view(u"row"_s);
        QVERIFY(v);
        click(v.get(), child(v->item, u"startButton"_s));
        click(v.get(), child(v->item, u"searchBox"_s));
        click(v.get(), child(v->item, u"taskViewButton"_s));
        click(v.get(), child(v->item, u"startButton"_s), Qt::RightButton);
        QCOMPARE(calls(v.get()), (QStringList{u"start"_s, u"search"_s, u"taskView"_s, u"quickLinks"_s}));
    }

    void blink()
    {
        auto v = view(u"row"_s);
        QVERIFY(v);
        QQuickItem *eye = child(v->item, u"eye"_s);
        QVERIFY(eye);
        // the lowest the lid went, read at every change: polling can miss
        // an 83 ms closing on a busy machine
        m_eye = eye;
        m_lowestLid = 1.0;
        QVERIFY(connect(eye, SIGNAL(lidChanged()), this, SLOT(lidMoved())));
        QMetaObject::invokeMethod(child(v->item, u"startButton"_s), "blink");
        // the lid closes within 83 ms, then opens again
        QTRY_VERIFY_WITH_TIMEOUT(m_lowestLid < 0.5, 400);
        QTRY_COMPARE_WITH_TIMEOUT(eye->property("lid").toReal(), 1.0, 1000);
    }



    void pinIds()
    {
        QCOMPARE(menu("pinId", {u"applications:org.kde.dolphin.desktop"_s}).toString(), u"org.kde.dolphin.desktop"_s);
        QCOMPARE(menu("pinId", {u"preferred://browser"_s}).toString(), u"preferred://browser"_s);
        QCOMPARE(menu("pinId", {u"file:///usr/share/applications/firefox.desktop"_s}).toString(), u"firefox.desktop"_s);
        QCOMPARE(menu("pinId", {u"  org.kde.kate.desktop "_s}).toString(), u"org.kde.kate.desktop"_s);
        QCOMPARE(strings(menu("cleanPins", {list({u"a.desktop"_s, u""_s, u"applications:a.desktop"_s, u"b.desktop"_s})})),
                 QStringList({u"a.desktop"_s, u"b.desktop"_s}));
    }

    void pinEdits()
    {
        const QJSValue pins = list({u"a"_s, u"b"_s, u"c"_s});
        QCOMPARE(strings(menu("pin", {pins, u"d"_s})), QStringList({u"a"_s, u"b"_s, u"c"_s, u"d"_s}));
        QCOMPARE(strings(menu("pin", {pins, u"applications:b"_s})), QStringList({u"a"_s, u"b"_s, u"c"_s}));
        QCOMPARE(strings(menu("unpin", {pins, u"b"_s})), QStringList({u"a"_s, u"c"_s}));
        QCOMPARE(strings(menu("unpin", {pins, u"zz"_s})), QStringList({u"a"_s, u"b"_s, u"c"_s}));
        QCOMPARE(strings(menu("moveToFront", {pins, u"c"_s})), QStringList({u"c"_s, u"a"_s, u"b"_s}));
        QVERIFY(menu("isPinned", {pins, u"applications:c"_s}).toBool());
        QVERIFY(!menu("isPinned", {pins, u"x"_s}).toBool());
        // the list given is never changed
        QCOMPARE(strings(pins), QStringList({u"a"_s, u"b"_s, u"c"_s}));
    }

    void pinDrag()
    {
        const QJSValue pins = list({u"a"_s, u"b"_s, u"c"_s, u"d"_s});
        QCOMPARE(strings(menu("movePin", {pins, 0, 2})), QStringList({u"b"_s, u"c"_s, u"a"_s, u"d"_s}));
        QCOMPARE(strings(menu("movePin", {pins, 3, 0})), QStringList({u"d"_s, u"a"_s, u"b"_s, u"c"_s}));
        QCOMPARE(strings(menu("movePin", {pins, 1, 99})), QStringList({u"a"_s, u"c"_s, u"d"_s, u"b"_s}));
        QCOMPARE(strings(menu("movePin", {pins, 9, 0})), QStringList({u"a"_s, u"b"_s, u"c"_s, u"d"_s}));
        // while dragging 0 over 2: 0 shows at 2, 1 and 2 move back one, 3 stays
        QCOMPARE(menu("visualIndex", {0, 0, 2}).toInt(), 2);
        QCOMPARE(menu("visualIndex", {1, 0, 2}).toInt(), 0);
        QCOMPARE(menu("visualIndex", {2, 0, 2}).toInt(), 1);
        QCOMPARE(menu("visualIndex", {3, 0, 2}).toInt(), 3);
        // dragging 3 over 1
        QCOMPARE(menu("visualIndex", {1, 3, 1}).toInt(), 2);
        QCOMPARE(menu("visualIndex", {0, 3, 1}).toInt(), 0);
        QCOMPARE(menu("visualIndex", {2, -1, 1}).toInt(), 2);
        // the slot under a point: 96 x 84 cells, 6 per row, 18 per page
        QCOMPARE(menu("slotAt", {10, 10, 0, 18, 30}).toInt(), 0);
        QCOMPARE(menu("slotAt", {200, 100, 0, 18, 30}).toInt(), 8);
        QCOMPARE(menu("slotAt", {200, 100, 1, 18, 30}).toInt(), 26);
        QCOMPARE(menu("slotAt", {590, 250, 1, 18, 30}).toInt(), 29);
        QCOMPARE(menu("slotAt", {-5, -5, 0, 18, 30}).toInt(), 0);
        // wider cells (the wide Start)
        QCOMPARE(menu("slotAt", {200, 10, 0, 18, 30, 129, 84}).toInt(), 1);
        // below the last row of the page: still on that page
        QCOMPARE(menu("slotAt", {10, 400, 0, 18, 30}).toInt(), 12);
        QCOMPARE(menu("pageCount", {0, 18}).toInt(), 1);
        QCOMPARE(menu("pageCount", {18, 18}).toInt(), 1);
        QCOMPARE(menu("pageCount", {19, 18}).toInt(), 2);
    }

    void sections()
    {
        QCOMPARE(menu("pinnedRows", {696, u"default"_s}).toInt(), 3);
        QCOMPARE(menu("pinnedRows", {760, u"default"_s}).toInt(), 4);
        QCOMPARE(menu("pinnedRows", {696, u"pins"_s}).toInt(), 4);
        QCOMPARE(menu("pinnedRows", {696, u"recommendations"_s}).toInt(), 2);
        QCOMPARE(menu("recommendedCount", {696, u"default"_s, true}).toInt(), 4);
        QCOMPARE(menu("recommendedCount", {760, u"default"_s, true}).toInt(), 6);
        QCOMPARE(menu("recommendedCount", {760, u"default"_s, false}).toInt(), 0);
        QCOMPARE(menu("recommendedCount", {696, u"pins"_s, true}).toInt(), 0);
    }

    void migration()
    {
        const QJSValue dflt = list({u"org.kde.dolphin.desktop"_s, u"firefox.desktop"_s});
        const QJSValue stock = list({u"preferred://browser"_s, u"applications:org.kde.konsole.desktop"_s, u"org.kde.discover.desktop"_s});
        QJSValue installed = m_menu.evaluate(u"(function (id) { return id !== 'org.kde.discover.desktop' })"_s);
        // still the defaults: the stock pins come over, without apps that are gone
        QCOMPARE(strings(menu("migratedPins", {stock, dflt, dflt, installed})),
                 QStringList({u"preferred://browser"_s, u"org.kde.konsole.desktop"_s}));
        // the person changed the pins already: nothing moves
        QVERIFY(menu("migratedPins", {stock, list({u"firefox.desktop"_s}), dflt, installed}).isNull());
        // nothing to move
        QVERIFY(menu("migratedPins", {list({}), dflt, dflt, installed}).isNull());
        QVERIFY(menu("migratedPins", {dflt, dflt, dflt, installed}).isNull());
        // the saved order first, then what it does not list
        QCOMPARE(strings(menu("orderedFavorites", {list({u"a"_s, u"b"_s, u"applications:c"_s}), list({u"c"_s, u"a"_s})})),
                 QStringList({u"c"_s, u"a"_s, u"b"_s}));
    }

    void defaultsMatchRayeshellrc()
    {
        QFile f(QStringLiteral(RAYE_SHELL_PO_DIR "/../lib/rayeshellrc"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QString group;
        QString pinned;
        for (const QString &line : QString::fromUtf8(f.readAll()).split(u'\n')) {
            if (line.startsWith(u'['))
                group = line;
            else if (group == u"[Start]"_s && line.startsWith(u"Pinned="_s))
                pinned = line.mid(7).trimmed();
        }
        QVERIFY(!pinned.isEmpty());
        QCOMPARE(strings(m_menu.globalObject().property(u"defaultPins"_s)), pinned.split(u','));
    }

    void letters()
    {
        QCOMPARE(menu("letterFor", {u"Ark"_s}).toString(), u"A"_s);
        QCOMPARE(menu("letterFor", {u"éditeur"_s}).toString(), u"E"_s);
        QCOMPARE(menu("letterFor", {u"7-Zip"_s}).toString(), u"#"_s);
        QCOMPARE(menu("letterFor", {u""_s}).toString(), u"#"_s);
        QCOMPARE(menu("letterFor", {u"Ωmega"_s}).toString(), u"#"_s);
        const QJSValue names = list({u"0 A.D."_s, u"Ark"_s, u"Écran"_s, u"Files"_s, u"Kate"_s});
        QCOMPARE(menu("firstRowFor", {names, u"#"_s}).toInt(), 0);
        QCOMPARE(menu("firstRowFor", {names, u"E"_s}).toInt(), 2);
        // no app under B: the next letter that has some
        QCOMPARE(menu("firstRowFor", {names, u"B"_s}).toInt(), 2);
        QCOMPARE(menu("firstRowFor", {names, u"Z"_s}).toInt(), -1);
        const QJSValue present = menu("lettersPresent", {names});
        QVERIFY(present.property(u"E"_s).toBool());
        QVERIFY(present.property(u"#"_s).toBool());
        QVERIFY(!present.property(u"B"_s).toBool());
        QVERIFY(menu("sortKey", {u"écran"_s}).toString() < menu("sortKey", {u"Files"_s}).toString());
        QVERIFY(menu("sortKey", {u"7-Zip"_s}).toString() < menu("sortKey", {u"Ark"_s}).toString());
    }

    void allAppsRows()
    {
        QJSValue apps = m_menu.evaluate(uR"([{ name: "7-Zip" }, { name: "Ark" }, { name: "Audacity" }, { name: "Écran" }])"_s);
        const QJSValue rows = menu("withHeaders", {apps});
        QCOMPARE(rows.property(u"length"_s).toInt(), 7);
        QCOMPARE(rows.property(0).property(u"header"_s).toString(), u"#"_s);
        QCOMPARE(rows.property(1).property(u"app"_s).property(u"name"_s).toString(), u"7-Zip"_s);
        QCOMPARE(rows.property(2).property(u"header"_s).toString(), u"A"_s);
        QCOMPARE(rows.property(5).property(u"header"_s).toString(), u"E"_s);
        QCOMPARE(menu("headerRow", {rows, u"A"_s}).toInt(), 2);
        QCOMPARE(menu("headerRow", {rows, u"B"_s}).toInt(), 5);
        QCOMPARE(menu("headerRow", {rows, u"Z"_s}).toInt(), -1);
    }

    void searchGroups()
    {
        QCOMPARE(menu("searchGroup", {u"krunner_services"_s}).toString(), u"apps"_s);
        QCOMPARE(menu("searchGroup", {u"org.raye.settings.runner"_s}).toString(), u"settings"_s);
        QCOMPARE(menu("searchGroup", {u"baloosearch"_s}).toString(), u"files"_s);
        QCOMPARE(menu("searchGroup", {u"org.raye.store.runner"_s}).toString(), u"store"_s);
        QCOMPARE(menu("searchGroup", {u"calculator"_s}).toString(), u"answers"_s);
        QCOMPARE(menu("searchGroup", {u"whatever"_s}).toString(), u"other"_s);

        QJSValue groups = m_menu.evaluate(uR"([
            { runner: "org.raye.store.runner", items: ["Get Kdenlive"] },
            { runner: "baloosearch", items: ["notes.txt", "kde.pdf"] },
            { runner: "krunner_services", items: ["Kate", "KCalc", "Konsole"] }
        ])"_s);
        const QJSValue out = menu("groupResults", {groups, 2});
        QCOMPARE(out.property(u"length"_s).toInt(), 4);
        QCOMPARE(out.property(0).property(u"group"_s).toString(), u"best"_s);
        QCOMPARE(out.property(0).property(u"items"_s).property(0).toString(), u"Kate"_s);
        QCOMPARE(out.property(1).property(u"group"_s).toString(), u"apps"_s);
        QCOMPARE(strings(out.property(1).property(u"items"_s)), QStringList({u"KCalc"_s, u"Konsole"_s}));
        QCOMPARE(out.property(2).property(u"group"_s).toString(), u"files"_s);
        QCOMPARE(out.property(3).property(u"group"_s).toString(), u"store"_s);
        // only Store results: no best match taken from them
        QJSValue store = m_menu.evaluate(uR"([{ runner: "org.raye.store.runner", items: ["Get GIMP"] }])"_s);
        QCOMPARE(menu("groupResults", {store, 4}).property(0).property(u"group"_s).toString(), u"store"_s);
    }

    void recommendedMix()
    {
        const QStringList out = strings(menu("recommended", {list({u"app1"_s, u"app2"_s, u"app3"_s, u"app4"_s}),
                                                               list({u"f1"_s, u"f2"_s, u"f3"_s}), 4}));
        QCOMPARE(out, QStringList({u"app1"_s, u"app2"_s, u"f1"_s, u"f2"_s}));
        QCOMPARE(strings(menu("recommended", {list({u"app1"_s, u"app2"_s, u"app3"_s}), list({}), 4})),
                 QStringList({u"app1"_s, u"app2"_s, u"app3"_s}));
        QCOMPARE(strings(menu("recommended", {list({}), list({u"f1"_s}), 6})), QStringList({u"f1"_s}));
    }

    // The quick link menu: which rows, in which order, what hides them
    void quickLinkRows()
    {
        const QStringList everything{u"org.kde.ksystemlog.desktop"_s, u"org.kde.partitionmanager.desktop"_s,
                                     u"org.kde.konsole.desktop"_s, u"org.raye.terminal-admin.desktop"_s,
                                     u"org.raye.taskmanager.desktop"_s, u"org.kde.dolphin.desktop"_s,
                                     u"org.raye.run.desktop"_s};
        QCOMPARE(linkIds(everything),
                 QStringList({u"apps"_s, u"power"_s, u"log"_s, u"system"_s, u"devices"_s, u"network"_s, u"disks"_s, u"-"_s,
                              u"terminal"_s, u"terminal-admin"_s, u"tasks"_s, u"-"_s,
                              u"settings"_s, u"files"_s, u"search"_s, u"run"_s, u"-"_s,
                              u"session"_s, u"desktop"_s}));
        // an app that is not installed leaves no row, and no row that does nothing
        const QStringList some{u"org.kde.konsole.desktop"_s, u"org.kde.dolphin.desktop"_s, u"org.raye.run.desktop"_s};
        const QStringList ids = linkIds(some);
        QVERIFY(!ids.contains(u"log"_s));
        QVERIFY(!ids.contains(u"disks"_s));
        QVERIFY(!ids.contains(u"terminal-admin"_s));
        QVERIFY(!ids.contains(u"tasks"_s));
        // the admin terminal needs the terminal it runs in
        QVERIFY(!linkIds({u"org.raye.terminal-admin.desktop"_s}).contains(u"terminal-admin"_s));
        // nothing but the settings rows left: no separators doubled, first or last
        const QStringList few = linkIds({});
        for (int i = 0; i < few.size(); ++i) {
            QVERIFY(!(few.at(i) == u"-"_s && (i == 0 || i == few.size() - 1 || few.at(i - 1) == u"-"_s)));
        }
        // Raye's task manager first, the other one only when it is missing
        const QStringList chain = m_links.evaluate(u"mainEntries.filter(function (e) { return e.id === 'tasks' })[0].apps"_s).toVariant().toStringList();
        QCOMPARE(chain.size(), 2);
        QCOMPARE(chain.first(), u"org.raye.taskmanager.desktop"_s);
        const QString pick = u"rows(mainEntries, function (id) { return testInstalled.indexOf(id) >= 0 }, {}).filter(function (e) { return e.id === 'tasks' })[0].app"_s;
        m_links.globalObject().setProperty(u"testInstalled"_s, m_links.toScriptValue(QStringList{chain.at(1)}));
        QCOMPARE(m_links.evaluate(pick).toString(), chain.at(1));
        m_links.globalObject().setProperty(u"testInstalled"_s, m_links.toScriptValue(chain));
        QCOMPARE(m_links.evaluate(pick).toString(), chain.first());
    }

    void quickLinkSessionRows()
    {
        const QJSValue all = m_links.evaluate(
            u"rows(sessionEntries, function () { return true }, { canLogout: true, canSuspend: true, canShutdown: true, canReboot: true }).map(function (e) { return e.separator ? '-' : e.id }).join(',')"_s);
        QCOMPARE(all.toString(), u"back,-,signout,sleep,shutdown,restart"_s);
        // what the machine cannot do is not offered
        const QJSValue some = m_links.evaluate(
            u"rows(sessionEntries, function () { return true }, { canLogout: true, canSuspend: false, canShutdown: true, canReboot: false }).map(function (e) { return e.separator ? '-' : e.id }).join(',')"_s);
        QCOMPARE(some.toString(), u"back,-,signout,shutdown"_s);
    }

    void quickLinkAccessKeys()
    {
        QCOMPARE(links("accessKey", {u"Power &options"_s}).toString(), u"o"_s);
        QCOMPARE(links("accessKey", {u"No key"_s}).toString(), QString());
        QCOMPARE(links("plainLabel", {u"Power &options"_s}).toString(), u"Power options"_s);
        QCOMPARE(links("styledLabel", {u"Power &options"_s}).toString(), u"Power <u>o</u>ptions"_s);
        // a label is text, never markup
        QCOMPARE(links("styledLabel", {u"<b>&x</b>"_s}).toString(), u"&lt;b&gt;<u>x</u>&lt;/b&gt;"_s);
    }

    // Every row has a label and a key of its own, in both languages, and the
    // names are Raye's: nothing here says what is underneath.
    void quickLinkLabels()
    {
        auto v = view(u"quicklinks"_s);
        QVERIFY(v);
        QFile po(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_start.po"));
        QVERIFY(po.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(po.readAll());
        const auto french = [&](const QString &id) {
            const qsizetype at = text.indexOf(u"msgid \""_s + id + u"\"\n"_s);
            if (at < 0)
                return QString();
            return text.mid(at).section(u'\n', 1, 1).section(u'"', 1, -2);
        };
        static const QRegularExpression foreign(u"kde|plasma|wine|konsole|dolphin|microsoft|windows"_s,
                                                QRegularExpression::CaseInsensitiveOption);
        for (const char *list : {"mainEntries", "sessionEntries"}) {
            const QStringList ids = m_links.evaluate(QLatin1String(list) + u".filter(function (e) { return !e.separator }).map(function (e) { return e.id })"_s)
                                        .toVariant().toStringList();
            QVERIFY(ids.size() >= 5);
            QSet<QString> en, fr;
            for (const QString &id : ids) {
                QVariant label;
                QVERIFY(QMetaObject::invokeMethod(v->item, "label", Q_RETURN_ARG(QVariant, label), Q_ARG(QVariant, id)));
                const QString l = label.toString();
                QVERIFY2(!l.isEmpty() && !foreign.match(l).hasMatch(), qPrintable(id + u": "_s + l));
                const QString ke = links("accessKey", {l}).toString();
                QVERIFY2(ke.size() == 1 && !en.contains(ke), qPrintable(u"key of "_s + l));
                en.insert(ke);
                const QString lf = french(l);
                QVERIFY2(!lf.isEmpty() && !foreign.match(lf).hasMatch(), qPrintable(u"French of "_s + l));
                const QString kf = links("accessKey", {lf}).toString();
                QVERIFY2(kf.size() == 1 && !fr.contains(kf), qPrintable(u"key of "_s + lf));
                fr.insert(kf);
            }
        }
    }

    // Open, keys, what a row does, the second menu
    void quickLinkMenuWorks()
    {
        auto v = view(u"quicklinks"_s);
        QVERIFY(v);
        QVERIFY(QMetaObject::invokeMethod(v->fixture.get(), "openLinks"));
        QTRY_VERIFY(openMenuWindow(v->window));
        QQuickWindow *w = openMenuWindow(v->window);
        QTRY_VERIFY(w->activeFocusItem());

        // rows are drawn: the underlined key, and no row of a missing app
        const auto rows = visibleNamed(w->contentItem(), u"quickLinkRow"_s);
        QVERIFY(rows.size() >= 10);
        QStringList names;
        qreal widest = 0;
        for (QQuickItem *r : rows) {
            names << r->property("text").toString();
            widest = std::max(widest, r->width());
            QVERIFY(!r->property("text").toString().contains(u'&'));
        }
        QVERIFY(names.contains(u"Settings"_s));
        QVERIFY(!names.contains(u"System log"_s));
        QVERIFY(w->width() >= widest);

        // the key picks, no Alt, and the menu closes
        QTest::keyClick(w, Qt::Key_N);
        QTRY_VERIFY(!openMenuWindow(v->window));
        QCOMPARE(calls(v.get()), QStringList({u"settings "_s}));

        // a page of Settings
        QVERIFY(QMetaObject::invokeMethod(v->fixture.get(), "openLinks"));
        QTRY_VERIFY(openMenuWindow(v->window));
        w = openMenuWindow(v->window);
        QTRY_VERIFY(w->activeFocusItem());
        QTest::keyClick(w, Qt::Key_O);
        QTRY_VERIFY(!openMenuWindow(v->window));
        QVERIFY(calls(v.get()).contains(u"settings system/power"_s));

        // shut down or sign out: a second menu, Back returns, Esc closes
        QVERIFY(QMetaObject::invokeMethod(v->fixture.get(), "openLinks"));
        QTRY_VERIFY(openMenuWindow(v->window));
        w = openMenuWindow(v->window);
        QTRY_VERIFY(w->activeFocusItem());
        QTest::keyClick(w, Qt::Key_U);
        QTRY_VERIFY(v->item->property("sessionOpened").toBool());
        QTRY_VERIFY(openMenuWindow(v->window));
        w = openMenuWindow(v->window);
        QTRY_VERIFY(w->activeFocusItem());
        QTRY_VERIFY(visibleNamed(w->contentItem(), u"quickLinkRow"_s).size() == 5);
        QTest::keyClick(w, Qt::Key_R);
        QTRY_VERIFY(!openMenuWindow(v->window));
        QVERIFY(calls(v.get()).contains(u"power restart"_s));
        QVERIFY(!v->item->property("opened").toBool());
    }

    void quickLinkAdminTerminalFile()
    {
        QFile f(QStringLiteral(RAYE_START_DIR "/../../menus/org.raye.terminal-admin.desktop"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(f.readAll());
        QVERIFY(text.contains(u"Exec=konsole -e sudo -i"_s));
        // it belongs to this menu: no second Terminal in All apps or Search
        QVERIFY(text.contains(u"NoDisplay=true"_s));
        QVERIFY(text.contains(u"Name[fr]="_s));
    }

    // Windows 10 look: Raye's first tiles, before anything is changed
    void windows10Row()
    {
        QCOMPARE(call("rowWidth10", {u"box"_s, true}).toInt(), 48 + 320 + 48);
        // no icon-and-label search on Windows 10: the box
        QCOMPARE(call("rowWidth10", {u"iconlabel"_s, false}).toInt(), 48 + 320);
        QCOMPARE(call("rowWidth10", {u"icon"_s, true}).toInt(), 3 * 48);
        QCOMPARE(call("rowWidth10", {u"hidden"_s, false}).toInt(), 48);
    }

    // Start sits on the taskbar, against the left edge, one column of
    // tiles on a small screen and two on a wide one
    void windows10Popup()
    {
        const QJSValue small = call("popupSize10", {1280, 800, 48, 1});
        QCOMPARE(small.property(u"width"_s).toInt(), 48 + 296 + 8 + 24 + 320);
        QCOMPARE(small.property(u"height"_s).toInt(), 720);
        const QJSValue wide = call("popupSize10", {1920, 1080, 48, 2});
        QCOMPARE(wide.property(u"width"_s).toInt(), 48 + 296 + 8 + 24 + 2 * 320 + 32);
        // never wider or taller than the screen
        const QJSValue tiny = call("popupSize10", {640, 480, 48, 3});
        QCOMPARE(tiny.property(u"width"_s).toInt(), 640);
        QCOMPARE(tiny.property(u"height"_s).toInt(), 432);
        const QJSValue pos = call("popupPosition10", {rect(1280, 0, 1920, 1080), 1032, 1048, 720});
        QCOMPARE(pos.property(u"x"_s).toInt(), 1280);
        QCOMPARE(pos.property(u"y"_s).toInt(), 1032 - 720);
    }

    void tileDefaults()
    {
        const QString pins = uR"(["org.kde.dolphin.desktop","firefox.desktop","org.raye.store.desktop","org.kde.elisa.desktop"])"_s;
        QCOMPARE(tiles(u"groups('', "_s + pins + u").map(g => g.name)"_s), uR"(["@productivity","@explore"])"_s);
        QCOMPARE(tiles(u"groups('', "_s + pins + u")[0].tiles"_s),
                 uR"([{"id":"org.kde.dolphin.desktop","size":"wide"},{"id":"firefox.desktop","size":"medium"}])"_s);
        QCOMPARE(tiles(u"groups('', "_s + pins + u")[1].tiles.map(t => t.id + ' ' + t.size)"_s),
                 uR"(["org.raye.store.desktop wide","org.kde.elisa.desktop medium"])"_s);
        QCOMPARE(tiles(u"groups('', [])"_s), u"[]"_s);
    }

    // the groups follow the pins: unpinned apps go, a new pin lands at the
    // end of the last group, an app shows once
    void tilesFollowThePins()
    {
        const QString text = u"Work|a.desktop=wide,b.desktop=small,a.desktop=large;Play|gone.desktop=medium"_s;
        QCOMPARE(tiles(u"groups('"_s + text + u"', ['a.desktop','b.desktop','new.desktop'])"_s),
                 uR"([{"name":"Work","tiles":[{"id":"a.desktop","size":"wide"},{"id":"b.desktop","size":"small"},{"id":"new.desktop","size":"medium"}]}])"_s);
        // a size nobody knows is medium
        QCOMPARE(tiles(u"parse('|x.desktop=huge')[0].tiles[0].size"_s), u"\"medium\""_s);
    }

    void tileNamesRoundTrip()
    {
        QCOMPARE(tiles(u"parse(serialize([{ name: 'Jeux; à | moi, =', tiles: [{ id: 'a.desktop', size: 'small' }] }]))"_s),
                 uR"([{"name":"Jeux; à | moi, =","tiles":[{"id":"a.desktop","size":"small"}]}])"_s);
        QCOMPARE(tiles(u"serialize(rename(parse('|a.desktop=wide'), 0, '  Work '))"_s), u"\"Work|a.desktop=wide\""_s);
    }

    // medium, wide and large start on even cells; small ones fill a 2 x 2
    // block four at a time, as on Windows 10
    void tilesPack()
    {
        const QString r = tiles(u"pack(parse('|A=medium,b=small,c=small,d=small,e=small,F=wide,g=small')[0].tiles)"
                                ".tiles.map(t => t.id + ' ' + t.x + ',' + t.y)"_s);
        QCOMPARE(r, uR"(["A 0,0","b 2,0","c 3,0","d 2,1","e 3,1","F 0,2","g 4,0"])"_s);
        QCOMPARE(tiles(u"pack(parse('|A=large,B=medium,C=medium')[0].tiles).rows"_s), u"4"_s);
        QCOMPARE(tiles(u"tilePixels('medium')"_s), uR"({"width":104,"height":104})"_s);
        QCOMPARE(tiles(u"groupWidth"_s), u"320"_s);
    }

    void tilesMove()
    {
        const QString g = u"parse('One|a=medium,b=medium,c=medium;Two|d=small')"_s;
        // later in the same group: counted without the tile itself
        QCOMPARE(tiles(u"moveTile("_s + g + u", 'a', 0, 2)[0].tiles.map(t => t.id)"_s), uR"(["b","a","c"])"_s);
        // to the other group, which keeps its name
        QCOMPARE(tiles(u"serialize(moveTile("_s + g + u", 'b', 1, 0))"_s), u"\"One|a=medium,c=medium;Two|b=medium,d=small\""_s);
        // past the last group: a new one; the empty group goes
        QCOMPARE(tiles(u"serialize(moveTile("_s + g + u", 'd', 2, 0))"_s), u"\"One|a=medium,b=medium,c=medium;|d=small\""_s);
        QCOMPARE(tiles(u"serialize(setSize("_s + g + u", 'd', 'large'))"_s), u"\"One|a=medium,b=medium,c=medium;Two|d=large\""_s);
        QCOMPARE(tiles(u"serialize(setSize("_s + g + u", 'd', 'giant'))"_s), u"\"One|a=medium,b=medium,c=medium;Two|d=small\""_s);
    }

    void tileColumns()
    {
        QCOMPARE(tiles(u"columnsFor('auto', 1280)"_s), u"1"_s);
        QCOMPARE(tiles(u"columnsFor('auto', 1920)"_s), u"2"_s);
        QCOMPARE(tiles(u"columnsFor('3', 1280)"_s), u"3"_s);
        QCOMPARE(tiles(u"columnsFor('9', 1280)"_s), u"1"_s);
        QCOMPARE(tiles(u"columnsForWidth(320)"_s), u"1"_s);
        QCOMPARE(tiles(u"columnsForWidth(700)"_s), u"2"_s);
        QCOMPARE(tiles(u"columnsForWidth(5000)"_s), u"3"_s);
        QCOMPARE(tiles(u"columnOf([300, 100, 100], 2)"_s), u"[0,1,1]"_s);
    }

    void popupLeftEdge()
    {
        // centred on a 1920 screen at x 1366
        QCOMPARE(menu("popupX", {u"center"_s, 1366, 1920, 1400, 840}).toInt(), 1366 + 540);
        // left aligned: with the Start button, 12 px from the edge at least
        QCOMPARE(menu("popupX", {u"left"_s, 0, 1366, 8, 680}).toInt(), 12);
        QCOMPARE(menu("popupX", {u"left"_s, 0, 1366, 300, 680}).toInt(), 300);
        // never off the screen
        QCOMPARE(menu("popupX", {u"left"_s, 0, 1366, 1000, 680}).toInt(), 1366 - 680);
    }


    void frameModes()
    {
        auto v = view(u"frame"_s);
        QVERIFY(v);
        QCOMPARE(v->item->property("mode").toString(), u"start"_s);
        QObject *start = v->item->property("view").value<QObject *>();
        QVERIFY(start);
        QVERIFY(start->metaObject()->indexOfSignal("requestClose()") >= 0);
        QVERIFY(start->metaObject()->indexOfProperty("pendingText") >= 0);

        // typing in Start goes to Search, with what was typed
        QTest::keyClick(v->window, Qt::Key_W, {}, 0);
        QTRY_COMPARE(v->item->property("mode").toString(), u"search"_s);
        QObject *search = v->item->property("view").value<QObject *>();
        QVERIFY(search && search->metaObject()->indexOfProperty("query") >= 0);
        QCOMPARE(search->property("query").toString(), u"w"_s);

        // Esc closes
        QSignalSpy closed(v->item, SIGNAL(requestClose()));
        QTest::keyClick(v->window, Qt::Key_Escape);
        QCOMPARE(closed.count(), 1);

        // a view asks to close
        QMetaObject::invokeMethod(search, "requestClose");
        QCOMPARE(closed.count(), 2);

        // opening again in Start mode
        QMetaObject::invokeMethod(v->item, "reset", Q_ARG(QVariant, u"start"_s), Q_ARG(QVariant, QString()));
        QTRY_COMPARE(v->item->property("mode").toString(), u"start"_s);
    }

    // by the item tree: what Repeater and ListView make is not always a
    // QObject child of the view
    static void collectNamed(QQuickItem *root, const QString &name, QList<QQuickItem *> &out)
    {
        const auto kids = root->childItems();
        for (QQuickItem *i : kids) {
            if (!i->isVisible())
                continue;
            if (i->objectName() == name)
                out << i;
            collectNamed(i, name, out);
        }
    }
    static QList<QQuickItem *> visibleNamed(QQuickItem *root, const QString &name)
    {
        QList<QQuickItem *> out;
        collectNamed(root, name, out);
        return out;
    }

    QStringList backendCalls(View *v)
    {
        return v->fixture->property("backend").value<QObject *>()->property("calls").toStringList();
    }

    // Start with a stand-in backend: pins, Recommended, All apps, the menus
    void startContent()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QTRY_COMPARE(visibleNamed(v->item, u"pinnedTile"_s).size(), 3);
        QCOMPARE(visibleNamed(v->item, u"pinnedTile"_s).first()->property("name").toString(), u"Files"_s);

        // a click on a tile starts the app
        QQuickItem *tile = visibleNamed(v->item, u"pinnedTile"_s).at(1);
        click(v.get(), tile);
        QTRY_VERIFY(backendCalls(v.get()).contains(u"trigger 1"_s));

        // one new app and the two recent files
        QTRY_COMPARE(visibleNamed(v->item, u"recommendedItem"_s).size(), 3);

        // the app menu: this one is pinned
        QObject *pinItem = v->item->findChild<QObject *>(u"pinToStart"_s);
        QVERIFY(pinItem);
        QObject *menu = pinItem->parent();
        QMetaObject::invokeMethod(menu, "openFor", Q_ARG(QVariant, u"org.kde.konsole.desktop"_s),
                                  Q_ARG(QVariant, true), Q_ARG(QVariant, QPointF(10, 10)));
        QCOMPARE(pinItem->property("text").toString(), u"Unpin from Start"_s);
        QMetaObject::invokeMethod(v->item->findChild<QObject *>(u"moveToFront"_s), "triggered");
        QVERIFY(backendCalls(v.get()).contains(u"front org.kde.konsole.desktop"_s));
        QMetaObject::invokeMethod(menu, "openFor", Q_ARG(QVariant, u"org.kde.ark.desktop"_s),
                                  Q_ARG(QVariant, false), Q_ARG(QVariant, QPointF(10, 10)));
        QCOMPARE(pinItem->property("text").toString(), u"Pin to Start"_s);
        QVERIFY(!v->item->findChild<QQuickItem *>(u"moveToFront"_s)->isVisible());
        QMetaObject::invokeMethod(v->item->findChild<QObject *>(u"uninstall"_s), "triggered");
        QVERIFY(backendCalls(v.get()).contains(u"uninstall org.kde.ark.desktop"_s));
        QMetaObject::invokeMethod(menu, "close");

        // All apps, in its list view: four apps under A, C, F and K
        // (the old view first: while it lives, the settings file is not read again)
        v.reset();
        StartConfig config(u"[Start]\nAllView=list\n"_s);
        v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QQuickItem *allButton = nullptr;
        for (QQuickItem *b : visibleNamed(v->item, u"sectionButton"_s))
            if (b->property("text").toString() == u"All apps"_s)
                allButton = b;
        QVERIFY(allButton);
        click(v.get(), allButton);
        QTRY_COMPARE(visibleNamed(v->item, u"allAppsRow"_s).size(), 4);
        QCOMPARE(visibleNamed(v->item, u"letterHeader"_s).size(), 4);
        click(v.get(), visibleNamed(v->item, u"allAppsRow"_s).at(1));
        QTRY_VERIFY(backendCalls(v.get()).contains(u"app org.kde.kcalc.desktop"_s));

        // the account name, and power without a prompt of Plasma's
        QQuickItem *bar = v->item->findChild<QQuickItem *>(u"bottomBar"_s);
        QCOMPARE(bar->property("fullName").toString(), u"Test Raye"_s);
        QVERIFY(!v->item->findChild<QQuickItem *>(u"updateDot"_s)->isVisible());
    }

    // The Windows 10 look: the eye, a wide square search box and Task view,
    // each as tall as the taskbar, side by side
    void windows10RowButtons()
    {
        auto v = view(u"row"_s, {{u"windows10"_s, true}});
        QVERIFY(v);
        QQuickItem *eye = child(v->item, u"startButton"_s);
        QQuickItem *box = child(v->item, u"searchBox"_s);
        QQuickItem *taskView = child(v->item, u"taskViewButton"_s);
        QCOMPARE(eye->width(), 48.0);
        QCOMPARE(eye->height(), 48.0);
        QCOMPARE(box->width(), 320.0);
        QCOMPARE(box->height(), 48.0);
        QCOMPARE(v->item->property("rowWidth").toInt(), 48 + 320 + 48);
        QCOMPARE(box->mapToScene(QPointF(0, 0)).x(), 48.0);
        QCOMPARE(taskView->mapToScene(QPointF(0, 0)).x(), 48.0 + 320);
        // no icon-and-label search on Windows 10: it stays a box
        QMetaObject::invokeMethod(v->fixture.get(), "setOption", Q_ARG(QVariant, u"searchMode"_s), Q_ARG(QVariant, u"iconlabel"_s));
        QVERIFY(box->isVisible());
        QVERIFY(!child(v->item, u"searchLabel"_s)->isVisible());
        click(v.get(), box);
        QCOMPARE(calls(v.get()), QStringList({u"search"_s}));
    }

    // Start in the Windows 10 look: rail, every app A to Z, tiles in groups
    void windows10Start()
    {
        StartConfig config(u"[Appearance]\nLook=windows10\n[Start]\nAllView=category\n"_s);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QCOMPARE(v->item->width(), 48.0 + 296 + 8 + 24 + 320);
        QTRY_COMPARE(visibleNamed(v->item, u"tile"_s).size(), 3);
        QVERIFY(child(v->item, u"startRail"_s));
        // the list is always the letters, whatever AllView says
        QTRY_COMPARE(visibleNamed(v->item, u"allAppsRow"_s).size(), 4);
        QCOMPARE(visibleNamed(v->item, u"letterHeader"_s).size(), 4);
        QCOMPARE(visibleNamed(v->item, u"recentlyAddedRow"_s).size(), 1);
        // one group, Raye's own, with Files wide
        const auto names = visibleNamed(v->item, u"groupName"_s);
        QCOMPARE(names.size(), 1);
        QCOMPARE(names.first()->property("text").toString(), u"Productivity"_s);
        const auto tiles = visibleNamed(v->item, u"tile"_s);
        QCOMPARE(tiles.at(0)->property("name").toString(), u"Files"_s);
        QCOMPARE(tiles.at(0)->width(), 4 * 54.0 - 4);
        QCOMPARE(tiles.at(1)->width(), 2 * 54.0 - 4);

        // a click on a tile starts its app, an app of the list too
        click(v.get(), tiles.at(1));
        QTRY_VERIFY(backendCalls(v.get()).contains(u"trigger 1"_s));
        click(v.get(), visibleNamed(v->item, u"allAppsRow"_s).at(1));
        QTRY_VERIFY(backendCalls(v.get()).contains(u"app org.kde.kcalc.desktop"_s));

        // the account and power in the rail
        QVERIFY(child(v->item, u"accountButton"_s)->isVisible());
        QVERIFY(child(v->item, u"powerButton"_s)->isVisible());
        QVERIFY(v->item->findChild<QQuickItem *>(u"restartToWindows"_s));
        QVERIFY(!child(v->item, u"updateDot"_s)->isVisible());
    }

    // resize, move and name tiles: each ends up in [Start] Tiles
    void windows10TileEdits()
    {
        StartConfig config(u"[Appearance]\nLook=windows10\n"_s);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QTRY_COMPARE(visibleNamed(v->item, u"tile"_s).size(), 3);

        QObject *wide = v->item->findChild<QObject *>(u"tileWide"_s);
        QVERIFY(wide);
        QObject *menu = v->item->findChild<QObject *>(u"tileMenu"_s);
        QVERIFY(menu);
        QMetaObject::invokeMethod(menu, "openFor", Q_ARG(QVariant, u"firefox.desktop"_s),
                                  Q_ARG(QVariant, u"medium"_s), Q_ARG(QVariant, QPointF(10, 10)));
        QMetaObject::invokeMethod(wide, "triggered");
        QVERIFY(backendCalls(v.get()).contains(
            u"tiles %40productivity|org.kde.dolphin.desktop=wide,firefox.desktop=wide,org.kde.konsole.desktop=medium"_s));
        QMetaObject::invokeMethod(menu, "close");

        // dropped below the groups: a group of its own
        QQuickItem *area = child(v->item, u"tileArea"_s);
        const int bottom = area->property("groupsBottom").toInt();
        QMetaObject::invokeMethod(area, "dropTile", Q_ARG(QVariant, u"org.kde.konsole.desktop"_s),
                                  Q_ARG(QVariant, QPointF(10, bottom + 10)));
        QVERIFY(backendCalls(v.get()).contains(
            u"tiles %40productivity|org.kde.dolphin.desktop=wide,firefox.desktop=medium;|org.kde.konsole.desktop=medium"_s));
        // dropped on Files: in its place
        QMetaObject::invokeMethod(area, "dropTile", Q_ARG(QVariant, u"org.kde.konsole.desktop"_s),
                                  Q_ARG(QVariant, QPointF(10, 50)));
        QVERIFY(backendCalls(v.get()).contains(
            u"tiles %40productivity|org.kde.konsole.desktop=medium,org.kde.dolphin.desktop=wide,firefox.desktop=medium"_s));

        // a name of the person's own
        // the tile menu had its own window: ours gets the keys back
        QTest::qWait(300);
        v->window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(v->window));
        QQuickItem *header = visibleNamed(v->item, u"groupHeader"_s).first();
        click(v.get(), header);
        QTRY_COMPARE(visibleNamed(v->item, u"groupField"_s).size(), 1);
        QTRY_VERIFY(visibleNamed(v->item, u"groupField"_s).first()->hasActiveFocus());
        for (const char c : {'W', 'o', 'r', 'k'})
            QTest::keyClick(v->window, c);
        QTest::keyClick(v->window, Qt::Key_Return);
        QVERIFY(backendCalls(v.get()).contains(
            u"tiles Work|org.kde.dolphin.desktop=wide,firefox.desktop=medium,org.kde.konsole.desktop=medium"_s));
        // and the typing never went to Search
        QCOMPARE(v->item->property("mode").toString(), u"start"_s);
    }

    // The look changes at once, back and forth, in a Start that is open
    void lookSwitchesAtOnce()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QCOMPARE(v->item->width(), 680.0);
        QTRY_COMPARE(visibleNamed(v->item, u"pinnedTile"_s).size(), 3);
        QObject *settings = v->engine->singletonInstance<QObject *>("org.raye.shell", "ShellSettings");
        QVERIFY(settings);
        {
            StartConfig config(u"[Appearance]\nLook=windows10\n"_s);
            QMetaObject::invokeMethod(settings, "reload");
            QTRY_VERIFY(child(v->item, u"startRail"_s));
            QCOMPARE(v->item->width(), 696.0);
            QTRY_COMPARE(visibleNamed(v->item, u"tile"_s).size(), 3);
        }
        QMetaObject::invokeMethod(settings, "reload");
        QTRY_COMPARE(visibleNamed(v->item, u"pinnedTile"_s).size(), 3);
        QCOMPARE(v->item->width(), 680.0);
    }

    // A restart another part of the system asked for puts the dot on the
    // power button, as ready updates do
    void restartNeededShowsTheDot()
    {
        StartConfig config(u"[Start]\nRestartNeeded=drivers\n"_s);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QTRY_VERIFY(v->item->findChild<QQuickItem *>(u"updateDot"_s)->isVisible());
    }

    // Esc closes from the list of all apps too, and Start reopens on Pinned
    void startReopensOnPinned()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QObject *start = v->item->property("view").value<QObject *>();
        start->setProperty("page", u"all"_s);
        QSignalSpy closed(v->item, SIGNAL(requestClose()));
        QTest::keyClick(v->window, Qt::Key_Escape);
        QCOMPARE(closed.count(), 1);
        QMetaObject::invokeMethod(v->item, "reset", Q_ARG(QVariant, u"start"_s), Q_ARG(QVariant, QString()));
        QCOMPARE(start->property("page").toString(), u"pinned"_s);
    }

    // closed a while, Start lets its views go, and makes them again on open
    // [Start] Folders: known names, each once, in the order set
    void folderButtons()
    {
        QCOMPARE(strings(menu("folderButtons", {list({u"files"_s, u"bogus"_s, u"files"_s, u"music"_s, u"settings"_s})})),
                 QStringList({u"files"_s, u"music"_s, u"settings"_s}));
        QCOMPARE(strings(menu("folderButtons", {list({})})), QStringList());
        // the list of names of the settings key, every one has an icon
        for (const QString &k : {u"settings"_s, u"files"_s, u"documents"_s, u"downloads"_s, u"music"_s, u"pictures"_s,
                                 u"videos"_s, u"network"_s, u"personal"_s}) {
            QCOMPARE(strings(menu("folderButtons", {list({k})})), QStringList({k}));
            QVERIFY(!menu("folderIcon", {k}).toString().isEmpty());
        }
        // a folder is told apart by its drawing, as on Windows: Files is a
        // folder, Downloads an arrow, and no two folders look the same
        QStringList glyphs;
        for (const QString &k : {u"files"_s, u"documents"_s, u"downloads"_s, u"music"_s, u"pictures"_s,
                                 u"videos"_s, u"network"_s, u"personal"_s}) {
            const QString g = menu("folderGlyph", {k}).toString();
            QVERIFY2(!g.isEmpty() && g != u"undefined"_s, qPrintable(k));
            QVERIFY2(!glyphs.contains(g), qPrintable(k));
            glyphs << g;
        }
        QCOMPARE(menu("folderGlyph", {u"files"_s}).toString(), u"folder"_s);
        QCOMPARE(menu("folderGlyph", {u"downloads"_s}).toString(), u"download"_s);
    }

    // Files and Downloads next to power are two different pictures
    void folderButtonsLookApart()
    {
        StartConfig config(u"[Start]\nFolders=files,downloads\n"_s);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QTRY_COMPARE(visibleNamed(v->item, u"folderButton"_s).size(), 2);
        QTest::qWait(200);
        const QImage shot = v->window->grabWindow();
        QList<QImage> pics;
        for (QQuickItem *b : visibleNamed(v->item, u"folderButton"_s)) {
            const QRectF r = b->mapRectToScene(QRectF(0, 0, b->width(), b->height()));
            const QImage pic = shot.copy(r.toRect());
            // something is drawn on the button
            bool drawn = false;
            const QRgb bg = pic.pixel(1, 1);
            for (int y = 0; y < pic.height() && !drawn; ++y)
                for (int x = 0; x < pic.width() && !drawn; ++x)
                    drawn = qAbs(qGray(pic.pixel(x, y)) - qGray(bg)) > 60;
            QVERIFY(drawn);
            pics << pic;
        }
        QVERIFY(pics.at(0) != pics.at(1));
    }

    // A category tile: up to four apps in full; more, and the fourth place
    // holds four small ones. Pinned apps come first, so the apps people
    // know are the ones the tile shows.
    void categoryPreview()
    {
        auto apps = [this](const QStringList &names) {
            QJSValue a = m_menu.newArray(names.size());
            for (int i = 0; i < names.size(); ++i) {
                QJSValue o = m_menu.newObject();
                o.setProperty(u"name"_s, names.at(i));
                o.setProperty(u"favoriteId"_s, QString(names.at(i).toLower() + u".desktop"_s));
                a.setProperty(i, o);
            }
            return a;
        };
        auto names = [](const QJSValue &v) {
            QStringList out;
            const int n = v.property(u"length"_s).toInt();
            for (int i = 0; i < n; ++i)
                out << v.property(i).property(u"name"_s).toString();
            return out;
        };
        QJSValue p = menu("categoryPreview", {apps({u"A"_s, u"B"_s}), list({})});
        QCOMPARE(names(p.property(u"big"_s)), QStringList({u"A"_s, u"B"_s}));
        QCOMPARE(names(p.property(u"small"_s)), QStringList());
        p = menu("categoryPreview", {apps({u"A"_s, u"B"_s, u"C"_s, u"D"_s}), list({})});
        QCOMPARE(names(p.property(u"big"_s)).size(), 4);
        // ten apps, Store and Settings pinned: they come first, in pin order
        p = menu("categoryPreview", {apps({u"Calc"_s, u"Clock"_s, u"Files"_s, u"Phone"_s, u"Remote"_s, u"Scan"_s,
                                           u"Shot"_s, u"Settings"_s, u"Recorder"_s, u"Store"_s}),
                                     list({u"store.desktop"_s, u"applications:settings.desktop"_s})});
        QCOMPARE(names(p.property(u"big"_s)), QStringList({u"Store"_s, u"Settings"_s, u"Calc"_s}));
        QCOMPARE(names(p.property(u"small"_s)), QStringList({u"Clock"_s, u"Files"_s, u"Phone"_s, u"Remote"_s}));
    }

    // Win+X and the right-click of the eye: the menu's left edge is the
    // Start button's left edge, not centred on it
    void quickLinksLineUpWithStart()
    {
        auto v = view(u"quicklinks"_s);
        QVERIFY(v);
        QVERIFY(QMetaObject::invokeMethod(v->fixture.get(), "openLinks"));
        QTRY_VERIFY(openMenuWindow(v->window));
        QQuickWindow *w = openMenuWindow(v->window);
        QTRY_VERIFY(w->width() >= 216);
        QQuickItem *button = v->fixture->property("linkButtonItem").value<QQuickItem *>();
        QVERIFY(button);
        QTRY_COMPARE(w->x(), qRound(button->mapToGlobal(QPointF(0, 0)).x()));
    }

    // the quick link menu open: no "Start" tooltip over it
    void noTooltipOverQuickLinks()
    {
        auto v = view(u"row"_s);
        QVERIFY(v);
        QQuickItem *button = child(v->item, u"startButton"_s);
        QCOMPARE(button->property("toolTipText").toString(), u"Start"_s);
        QVERIFY(v->item->setProperty("quickLinksOpen", true));
        QCOMPARE(button->property("toolTipText").toString(), QString());
        QVERIFY(v->item->setProperty("quickLinksOpen", false));
        QCOMPARE(button->property("toolTipText").toString(), u"Start"_s);
    }

    // Recommended with most used apps on: new apps, then most used, then files
    void recommendedMostUsed()
    {
        const auto named = [this](const char *what, int n) {
            QJSValue a = m_menu.newArray(n);
            for (int i = 0; i < n; ++i) {
                QJSValue o = m_menu.newObject();
                o.setProperty(u"name"_s, QString(QString::fromLatin1(what) + QString::number(i)));
                a.setProperty(i, o);
            }
            return a;
        };
        const auto names = [](const QJSValue &v) {
            QStringList out;
            for (int i = 0; i < v.property(u"length"_s).toInt(); ++i)
                out << v.property(i).property(u"name"_s).toString();
            return out;
        };
        QCOMPARE(names(menu("recommended", {named("n", 1), named("f", 5), 4, named("m", 5)})),
                 QStringList({u"n0"_s, u"m0"_s, u"f0"_s, u"f1"_s}));
        // nothing else to show: most used fills the places
        QCOMPARE(names(menu("recommended", {named("n", 0), named("f", 0), 4, named("m", 5)})),
                 QStringList({u"m0"_s, u"m1"_s, u"m2"_s, u"m3"_s}));
        // the way of calling it without most used still works
        QCOMPARE(names(menu("recommended", {named("n", 1), named("f", 1), 4})), QStringList({u"n0"_s, u"f0"_s}));
    }

    // Every [Start] key of Raye Settings changes what Start shows
    void startKeysAreRead()
    {
        StartConfig config(u"[Start]\nMostUsed=true\nRecentApps=false\nRecommended=false\n"
                           "Folders=documents,music,personal\nAllView=grid\n"_s);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        // only the most used apps are left in Recommended
        QTRY_COMPARE(visibleNamed(v->item, u"recommendedItem"_s).size(), 1);
        QCOMPARE(visibleNamed(v->item, u"recommendedItem"_s).first()->property("name").toString(), u"Kate"_s);
        click(v.get(), visibleNamed(v->item, u"recommendedItem"_s).first());
        QTRY_VERIFY(backendCalls(v.get()).contains(u"trigger mostUsed 0"_s));

        // the folders asked for, next to power
        const auto folders = visibleNamed(v->item, u"folderButton"_s);
        QCOMPARE(folders.size(), 3);
        click(v.get(), folders.at(1));
        QTRY_VERIFY(backendCalls(v.get()).contains(u"folder music"_s));

        // All opens in the view asked for
        QObject *start = v->item->property("view").value<QObject *>();
        start->setProperty("page", u"all"_s);
        QTRY_COMPARE(visibleNamed(v->item, u"allGridTile"_s).size(), 4);
        QCOMPARE(visibleNamed(v->item, u"allAppsRow"_s).size(), 0);
        QCOMPARE(visibleNamed(v->item, u"categoryTile"_s).size(), 0);
    }

    // All off: Recommended is not there at all
    void recommendedAllOff()
    {
        StartConfig config(u"[Start]\nMostUsed=false\nRecentApps=false\nRecommended=false\n"_s);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QTest::qWait(100);
        QCOMPARE(visibleNamed(v->item, u"recommended"_s).size(), 0);
    }

    // Recent files off, new apps on: new apps only
    void recommendedFilesOff()
    {
        StartConfig config(u"[Start]\nRecommended=false\nRecentApps=true\n"_s);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QTRY_COMPARE(visibleNamed(v->item, u"recommendedItem"_s).size(), 1);
        QCOMPARE(visibleNamed(v->item, u"recommendedItem"_s).first()->property("name").toString(), u"Kdenlive"_s);
    }

    // Category, Grid, List: the picker changes the view and remembers it
    void allViews()
    {
        StartConfig config(u"[Start]\nAllView=category\n"_s);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QObject *start = v->item->property("view").value<QObject *>();
        start->setProperty("page", u"all"_s);

        // categories: a tile of five apps shows three, then the other two
        // small in the fourth place, as on Windows; the pinned ones first
        QTRY_COMPARE(visibleNamed(v->item, u"categoryTile"_s).size(), 2);
        QQuickItem *tile = visibleNamed(v->item, u"categoryTile"_s).first();
        QCOMPARE(tile->property("name").toString(), u"Productivity"_s);
        QCOMPARE(visibleNamed(tile, u"categoryIcon"_s).size(), 3);
        QCOMPARE(visibleNamed(tile, u"categoryMoreIcon"_s).size(), 2);
        QCOMPARE(visibleNamed(tile, u"categoryIcon"_s).first()->property("source").toString(), u"system-file-manager"_s);
        QQuickItem *small = visibleNamed(v->item, u"categoryTile"_s).at(1);
        QCOMPARE(visibleNamed(small, u"categoryIcon"_s).size(), 1);
        QCOMPARE(visibleNamed(small, u"categoryMoreIcon"_s).size(), 0);

        // a tile opens in place, an app starts, Back returns to the tiles
        click(v.get(), tile);
        QTRY_COMPARE(visibleNamed(v->item, u"categoryApp"_s).size(), 5);
        QCOMPARE(visibleNamed(v->item, u"categoryTile"_s).size(), 0);
        click(v.get(), visibleNamed(v->item, u"categoryApp"_s).at(1));
        QTRY_VERIFY(backendCalls(v.get()).contains(u"app org.kde.kcalc.desktop"_s));
        click(v.get(), visibleNamed(v->item, u"categoryBack"_s).first());
        QTRY_COMPARE(visibleNamed(v->item, u"categoryTile"_s).size(), 2);

        // the picker: Grid, then List, written to the settings
        click(v.get(), visibleNamed(v->item, u"viewPicker_grid"_s).first());
        QTRY_COMPARE(visibleNamed(v->item, u"allGridTile"_s).size(), 4);
        click(v.get(), visibleNamed(v->item, u"viewPicker_list"_s).first());
        QTRY_COMPARE(visibleNamed(v->item, u"allAppsRow"_s).size(), 4);
        QFile written(config.path);
        QVERIFY(written.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(written.readAll()).contains(u"AllView=list"_s));

        // a click in the grid starts the app
        click(v.get(), visibleNamed(v->item, u"viewPicker_grid"_s).first());
        QTRY_COMPARE(visibleNamed(v->item, u"allGridTile"_s).size(), 4);
        click(v.get(), visibleNamed(v->item, u"allGridTile"_s).at(3));
        QTRY_VERIFY(backendCalls(v.get()).contains(u"app org.kde.konsole.desktop"_s));
    }

    // The keyboard works in the grid
    void allViewsKeys()
    {
        StartConfig config(u"[Start]\nAllView=grid\n"_s);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QObject *start = v->item->property("view").value<QObject *>();
        start->setProperty("page", u"all"_s);
        QTRY_COMPARE(visibleNamed(v->item, u"allGridTile"_s).size(), 4);
        QTest::keyClick(v->window, Qt::Key_Right);
        QTest::keyClick(v->window, Qt::Key_Return);
        QTRY_VERIFY(backendCalls(v.get()).contains(u"app org.kde.kcalc.desktop"_s));
    }

    // Restart to Windows is there only when the machine can do it
    void restartToWindows()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QQuickItem *row = v->item->findChild<QQuickItem *>(u"restartToWindows"_s);
        QVERIFY(row);
        QVERIFY(!row->property("wanted").toBool());
        QObject *backend = v->fixture->property("backend").value<QObject *>();
        backend->setProperty("canRestartToWindows", true);
        QVERIFY(row->property("wanted").toBool());
        QMetaObject::invokeMethod(row, "triggered");
        QVERIFY(backendCalls(v.get()).contains(u"restart to windows"_s));
    }

    void frameRelease()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QVERIFY(v->item->property("view").value<QObject *>());
        QMetaObject::invokeMethod(v->item, "release");
        QTRY_VERIFY(!v->item->property("view").value<QObject *>());
        QMetaObject::invokeMethod(v->item, "reset", Q_ARG(QVariant, u"start"_s), Q_ARG(QVariant, QString()));
        QTRY_VERIFY(v->item->property("view").value<QObject *>());
        QTRY_COMPARE(visibleNamed(v->item, u"pinnedTile"_s).size(), 3);
    }

    void slideStartsOnTheFirstFrame_data()
    {
        QTest::addColumn<int>("stall");
        QTest::newRow("quick") << 0;
        QTest::newRow("slow window") << 120;
    }

    void slideStartsOnTheFirstFrame()
    {
        QFETCH(int, stall);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QQuickItem *content = child(v->item, u"startContent"_s);
        QVERIFY(content);
        QTRY_COMPARE(content->opacity(), 1.0);

        // close as Start does, and take the window away
        QSignalSpy closed(v->item, SIGNAL(closed()));
        v->item->setProperty("shown", false);
        QTRY_COMPARE(closed.count(), 1);
        v->window->hide();
        QCOMPARE(content->opacity(), 0.0);

        QList<Sample> frames;
        openAndWatch(v.get(), stall, frames);
        QVERIFY(frames.size() >= 2);
        const qreal slide = 40;
        // the first frame shows the start of the slide, not the middle
        QVERIFY2(frames.first().y >= slide * 0.9 && frames.first().opacity <= 0.1,
                 qPrintable(u"first frame at y %1, opacity %2"_s.arg(frames.first().y).arg(frames.first().opacity)));
        int moving = 0;
        qint64 longest = 0, start = -1, end = -1;
        for (int i = 0; i < frames.size(); ++i) {
            if (frames[i].y > 0 && frames[i].y < slide) {
                ++moving;
                if (start < 0)
                    start = frames[i - 1 >= 0 ? i - 1 : i].ms;
            }
            if (i > 0 && frames[i].y > 0)
                longest = qMax(longest, frames[i].ms - frames[i - 1].ms);
            if (end < 0 && frames[i].y == 0 && frames[i].opacity == 1.0 && start >= 0)
                end = frames[i].ms;
        }
        fprintf(stderr, "open (stall %d ms): first frame at %lld ms, slide %lld ms, %d frames moving, %.0f fps, longest gap %lld ms\n",
              stall, frames.first().ms, end - start, moving, end > start ? moving * 1000.0 / (end - start) : 0.0, longest);
        // the slide is seen: several frames on the way, not one jump
        QVERIFY2(moving >= 4, qPrintable(u"%1 frames on the way"_s.arg(moving)));
    }

    // The acrylic is part of what slides: no frame shows the panel without
    // what is on it
    void panelRisesWithItsContent()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QQuickItem *content = child(v->item, u"startContent"_s);
        QQuickItem *surface = child(v->item, u"startSurface"_s);
        QVERIFY2(surface, "the panel's acrylic is not in the frame");
        QVERIFY(content->isAncestorOf(surface));
        QTRY_COMPARE(content->opacity(), 1.0);
        QSignalSpy closed(v->item, SIGNAL(closed()));
        v->item->setProperty("shown", false);
        QTRY_COMPARE(closed.count(), 1);
        v->window->hide();

        QList<int> tiles;
        QList<qreal> opacity;
        auto conn = connect(v->window, &QQuickWindow::frameSwapped, this, [&] {
            opacity << content->opacity();
            tiles << int(visibleNamed(v->item, u"pinnedTile"_s).size());
        }, Qt::QueuedConnection);
        QMetaObject::invokeMethod(v->item, "reset", Q_ARG(QVariant, u"start"_s), Q_ARG(QVariant, QString()));
        QMetaObject::invokeMethod(v->item, "present");
        v->window->show();
        QTRY_COMPARE(content->opacity(), 1.0);
        disconnect(conn);
        QVERIFY(!opacity.isEmpty());
        for (int i = 0; i < opacity.size(); ++i) {
            if (opacity[i] > 0.05)
                QVERIFY2(tiles[i] == 3, qPrintable(u"frame %1: panel at %2 with %3 pins"_s.arg(i).arg(opacity[i]).arg(tiles[i])));
        }
    }

    // Opening again right after a close rises on the opening curve, at its
    // own speed: most of the way in the first half
    void openingAgainUsesTheOpeningCurve()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QQuickItem *content = child(v->item, u"startContent"_s);
        QTRY_COMPARE(content->opacity(), 1.0);
        QSignalSpy closed(v->item, SIGNAL(closed()));
        v->item->setProperty("shown", false);
        QTRY_COMPARE(closed.count(), 1);
        v->window->hide();

        QList<Sample> frames;
        openAndWatch(v.get(), 0, frames);
        qint64 start = -1, end = -1;
        for (int i = 1; i < frames.size(); ++i) {
            if (start < 0 && frames[i].y < 40)
                start = frames[i - 1].ms;
            if (start >= 0 && end < 0 && frames[i].y == 0)
                end = frames[i].ms;
        }
        QVERIFY(start >= 0 && end > start);
        // where it is halfway through the slide
        qreal mid = 40;
        for (const Sample &f : frames) {
            if (f.ms >= start + (end - start) / 2) {
                mid = f.y;
                break;
            }
        }
        fprintf(stderr, "open again: slide %lld ms, halfway at y %.1f\n", end - start, mid);
        QVERIFY2(end - start >= 200, qPrintable(QString::number(end - start)));
        QVERIFY2(mid <= 12, qPrintable(QString::number(mid)));
    }

    // Closing fades as it drops away. On the exit curve the panel stayed
    // fully there for most of the time and then vanished in one frame.
    void closeFadesAsItGoes()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QQuickItem *content = child(v->item, u"startContent"_s);
        QTRY_COMPARE(content->opacity(), 1.0);
        QTest::qWait(50);

        QList<Sample> frames;
        QElapsedTimer clock;
        auto conn = connect(v->window, &QQuickWindow::frameSwapped, this, [&] {
            frames << Sample{clock.elapsed(), content->y(), content->opacity()};
        }, Qt::QueuedConnection);
        QSignalSpy closed(v->item, SIGNAL(closed()));
        clock.start();
        v->item->setProperty("shown", false);
        QTRY_COMPARE(closed.count(), 1);
        disconnect(conn);

        // a straight fade: no frame takes away much of it at once
        qreal biggest = 0;
        for (int i = 1; i < frames.size(); ++i)
            biggest = qMax(biggest, frames[i - 1].opacity - frames[i].opacity);
        fprintf(stderr, "close: %lld ms, %lld frames, biggest step %.2f\n",
                frames.isEmpty() ? 0 : frames.constLast().ms, qsizetype(frames.size()), biggest);
        QVERIFY(frames.size() >= 4);
        QVERIFY2(biggest < 0.4, qPrintable(u"one frame took %1 of the panel away"_s.arg(biggest)));
    }

    // The Windows 10 Start rises from the taskbar, a longer way than the
    // Windows 11 one
    void windows10RisesFurther()
    {
        StartConfig config(u"[Appearance]\nLook=windows10\n"_s);
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QQuickItem *content = child(v->item, u"startContent"_s);
        QTRY_COMPARE(content->opacity(), 1.0);
        QSignalSpy closed(v->item, SIGNAL(closed()));
        v->item->setProperty("shown", false);
        QTRY_COMPARE(closed.count(), 1);
        QVERIFY2(content->y() >= 100, qPrintable(QString::number(content->y())));
    }

    // Made ahead of time (main.qml does it after login), a little at a
    // time; opening then has nothing left to build
    void preparedBeforeOpening()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}, {u"autoOpen"_s, false}});
        QVERIFY(v);
        QVERIFY(!v->item->property("view").value<QObject *>());
        QVERIFY(QMetaObject::invokeMethod(v->item, "prepare"));
        // not in one go
        QVERIFY(!loaderReady(v->item, u"startLoader"_s));
        QTRY_VERIFY(loaderReady(v->item, u"startLoader"_s) && loaderReady(v->item, u"searchLoader"_s));

        QMetaObject::invokeMethod(v->item, "reset", Q_ARG(QVariant, u"start"_s), Q_ARG(QVariant, QString()));
        QCOMPARE(visibleNamed(v->item, u"pinnedTile"_s).size(), 3);
        QMetaObject::invokeMethod(v->item, "present");
        QTRY_COMPARE(child(v->item, u"startContent"_s)->opacity(), 1.0);
    }

    // Start opened while the views are still being made: they are
    // finished right away, nothing waits for the background
    void openingFinishesWhatIsBeingMade()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}, {u"autoOpen"_s, false}});
        QVERIFY(v);
        QVERIFY(QMetaObject::invokeMethod(v->item, "prepare"));
        QVERIFY(!loaderReady(v->item, u"startLoader"_s));
        QMetaObject::invokeMethod(v->item, "reset", Q_ARG(QVariant, u"start"_s), Q_ARG(QVariant, QString()));
        QVERIFY(loaderReady(v->item, u"startLoader"_s));
        QCOMPARE(visibleNamed(v->item, u"pinnedTile"_s).size(), 3);
    }

    // From a key typed in Start to its results painted: the time the
    // search service takes, plus what Start adds on top
    qint64 keyToResults(View *v, Qt::Key key, const QString &shown, const QString &gone = {})
    {
        QElapsedTimer clock;
        qint64 painted = -1;
        auto conn = connect(v->window, &QQuickWindow::frameSwapped, this, [&] {
            QQuickItem *list = child(v->item, u"searchResults"_s);
            if (painted < 0 && list && list->isVisible() && list->property("count").toInt() > 0) {
                QStringList names;
                for (const QVariant &r : list->property("model").toList())
                    names << r.toMap().value(u"item"_s).toMap().value(u"name"_s).toString();
                if (names.contains(shown) && (gone.isEmpty() || !names.contains(gone)))
                    painted = clock.elapsed();
            }
        }, Qt::QueuedConnection);
        clock.start();
        QTest::keyClick(v->window, key);
        QElapsedTimer limit;
        limit.start();
        while (painted < 0 && limit.elapsed() < 2000)
            QTest::qWait(1);
        disconnect(conn);
        if (painted < 0) {
            QQuickItem *list = child(v->item, u"searchResults"_s);
            QStringList names;
            for (const QVariant &r : list->property("model").toList())
                names << r.toMap().value(u"item"_s).toMap().value(u"name"_s).toString();
            QObject *b = v->fixture->property("backend").value<QObject *>();
            QObject *g = b->property("searchGroups").value<QObject *>();
            QObject *m = b->property("searchMatches").value<QObject *>();
            fprintf(stderr, "results: %s query '%s' groups %d matches %d\n", qPrintable(names.join(u',')),
                    qPrintable(g->property("query").toString()), g->property("count").toInt(), m->property("count").toInt());
        }
        return painted;
    }

    void typingShowsResultsAtOnce()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}});
        QVERIFY(v);
        QObject *backend = v->fixture->property("backend").value<QObject *>();
        QVERIFY(backend);
        backend->setProperty("fakeSearch", true);
        const int service = backend->property("searchAnswerMs").toInt();
        QTRY_COMPARE(child(v->item, u"startContent"_s)->opacity(), 1.0);
        QTest::qWait(300);

        const qint64 first = keyToResults(v.get(), Qt::Key_A, u"Calculator"_s);
        QVERIFY(first >= 0);
        QTest::qWait(200);
        const qint64 next = keyToResults(v.get(), Qt::Key_R, u"Ark"_s, u"Calculator"_s);
        QVERIFY(next >= 0);
        fprintf(stderr, "search: first key %lld ms, next key %lld ms (service %d ms; Start adds %lld and %lld ms)\n",
                first, next, service, first - service, next - service);
        // the first key also makes the search page; after that Start adds
        // about a frame to what the service takes (it used to wait 40 ms more)
        QVERIFY2(first - service <= 70, qPrintable(QString::number(first - service)));
        QVERIFY2(next - service <= 35, qPrintable(QString::number(next - service)));
    }

    // the search page is made with the rest: the first key is as quick
    // as the next one
    void firstKeyAsQuickAsTheNext()
    {
        auto v = view(u"frame"_s, {{u"withBackend"_s, true}, {u"autoOpen"_s, false}});
        QVERIFY(v);
        QObject *backend = v->fixture->property("backend").value<QObject *>();
        backend->setProperty("fakeSearch", true);
        const int service = backend->property("searchAnswerMs").toInt();
        QVERIFY(QMetaObject::invokeMethod(v->item, "prepare"));
        QTRY_VERIFY(loaderReady(v->item, u"searchLoader"_s));
        QMetaObject::invokeMethod(v->item, "reset", Q_ARG(QVariant, u"start"_s), Q_ARG(QVariant, QString()));
        QMetaObject::invokeMethod(v->item, "present");
        QTRY_COMPARE(child(v->item, u"startContent"_s)->opacity(), 1.0);
        QTest::qWait(300);
        const qint64 first = keyToResults(v.get(), Qt::Key_A, u"Calculator"_s);
        QVERIFY(first >= 0);
        fprintf(stderr, "search, page made ahead: first key %lld ms (service %d ms)\n", first, service);
        QVERIFY2(first - service <= 35, qPrintable(QString::number(first - service)));
    }

    void frameSize()
    {
        auto v = view(u"frame"_s);
        QVERIFY(v);
        QCOMPARE(v->item->width(), 680.0);
        QCOMPARE(v->item->height(), 728.0);
    }

    void french()
    {
        QFile po(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_start.po"));
        QVERIFY(po.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(po.readAll());
        QVERIFY(text.contains(u"msgid \"Search\""_s));
        QVERIFY(text.contains(u"msgstr \"Rechercher\""_s));
        QVERIFY(text.contains(u"msgid \"Task view\""_s));
        QVERIFY(text.contains(u"msgstr \"Vue des tâches\""_s));
    }

    // every string the applet shows has its French line
    void frenchComplete()
    {
        QFile po(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_start.po"));
        QVERIFY(po.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(po.readAll());
        static const QRegularExpression call(u"i18nd\\(\"raye_shell_start\", \"([^\"]+)\""_s);
        QStringList missing;
        int seen = 0;
        QDirIterator it(QStringLiteral(RAYE_START_DIR "/contents/ui"), {u"*.qml"_s}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile f(it.next());
            QVERIFY(f.open(QIODevice::ReadOnly));
            auto m = call.globalMatch(QString::fromUtf8(f.readAll()));
            while (m.hasNext()) {
                const QString id = m.next().captured(1);
                ++seen;
                const qsizetype at = text.indexOf(u"msgid \""_s + id + u"\"\n"_s);
                if (at < 0 || text.mid(at).section(u'\n', 1, 1) == u"msgstr \"\""_s)
                    missing << id;
            }
        }
        QVERIFY(seen > 40);
        QVERIFY2(missing.isEmpty(), qPrintable(missing.join(u", "_s)));
    }
};

QTEST_MAIN(TestStart)

#include "tst_start.moc"
