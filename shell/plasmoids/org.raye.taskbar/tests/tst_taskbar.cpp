// The taskbar: its decisions (taskbarlogic.js) and the row of buttons
// (TaskBar.qml) run against a stand-in for Plasma's tasks model, with the
// Raye tokens: pinned apps, the indicator of the app in use, badges,
// labels, small buttons, the "..." overflow and what a click does.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QJSEngine>
#include <QGuiApplication>
#include <QPointer>
#include <QStyleHints>
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

using namespace Qt::StringLiterals;

class TestTaskbar : public QObject
{
    Q_OBJECT

private:
    QJSEngine m_js;
    QStringList m_warnings;
    static inline TestTaskbar *s_self = nullptr;

    static void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
    {
        if (type != QtDebugMsg && type != QtInfoMsg && msg.contains(u"org.raye.taskbar"_s))
            s_self->m_warnings << msg;
    }

    QJSValue call(const char *name, const QJSValueList &args)
    {
        QJSValue f = m_js.globalObject().property(QLatin1String(name));
        const QJSValue r = f.call(args);
        if (r.isError())
            qWarning() << name << r.toString();
        return r;
    }

    QJSValue task(bool launcher, bool startup, bool active, bool minimized, int children)
    {
        QJSValue t = m_js.newObject();
        t.setProperty(u"isLauncher"_s, launcher);
        t.setProperty(u"isStartup"_s, startup);
        t.setProperty(u"isActive"_s, active);
        t.setProperty(u"isMinimized"_s, minimized);
        t.setProperty(u"childCount"_s, children);
        return t;
    }

    QJSValue list(const QStringList &items)
    {
        QJSValue a = m_js.newArray(items.size());
        for (int i = 0; i < items.size(); ++i)
            a.setProperty(i, items.at(i));
        return a;
    }

    QStringList strings(const QJSValue &array)
    {
        QStringList out;
        const int n = array.property(u"length"_s).toInt();
        for (int i = 0; i < n; ++i)
            out << array.property(i).toString();
        return out;
    }

    struct View {
        std::unique_ptr<QQmlEngine> engine;
        std::unique_ptr<QObject> fixture;
        QQuickWindow *window = nullptr;
        QQuickItem *bar = nullptr;
    };

    // fixture.qml: a window holding a TaskBar on a stand-in model
    std::unique_ptr<View> view(const QVariantMap &settings, const QVariantList &tasks, int width = 1000)
    {
        auto v = std::make_unique<View>();
        v->engine = std::make_unique<QQmlEngine>();
        v->engine->addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        v->engine->addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        KLocalization::setupLocalizedContext(v->engine.get());
        QQmlComponent c(v->engine.get(), QUrl::fromLocalFile(QStringLiteral(RAYE_TASKBAR_TESTS "/fixture.qml")));
        QObject *o = c.beginCreate(v->engine->rootContext());
        if (!o) {
            qWarning() << c.errors();
            return nullptr;
        }
        o->setProperty("uiDir", QUrl::fromLocalFile(QStringLiteral(RAYE_TASKBAR_DIR "/contents/ui/")));
        o->setProperty("barWidth", width);
        c.completeCreate();
        v->fixture.reset(o);
        for (auto it = settings.cbegin(); it != settings.cend(); ++it)
            QMetaObject::invokeMethod(o, "setOption", Q_ARG(QVariant, it.key()), Q_ARG(QVariant, it.value()));
        for (const QVariant &t : tasks)
            QMetaObject::invokeMethod(o, "add", Q_ARG(QVariant, t));
        v->window = qobject_cast<QQuickWindow *>(o);
        v->bar = o->property("bar").value<QQuickItem *>();
        if (!v->window || !v->bar)
            return nullptr;
        v->window->show();
        if (!QTest::qWaitForWindowExposed(v->window))
            return nullptr;
        QTest::qWait(50);
        return v;
    }

    static QVariantMap app(const QString &name, const QString &icon, const QVariantMap &extra = {})
    {
        QVariantMap m{{u"display"_s, name}, {u"AppName"_s, name}, {u"decoration"_s, icon},
                      {u"LauncherUrlWithoutIcon"_s, QString(u"applications:"_s + icon + u".desktop"_s)},
                      {u"AppId"_s, icon}, {u"IsLauncher"_s, false}, {u"IsStartup"_s, false},
                      {u"IsActive"_s, false}, {u"IsMinimized"_s, false}, {u"IsDemandingAttention"_s, false},
                      {u"ChildCount"_s, 0}, {u"IsWindow"_s, true}};
        for (auto it = extra.cbegin(); it != extra.cend(); ++it)
            m.insert(it.key(), it.value());
        return m;
    }

    QQuickItem *button(View *v, int index)
    {
        QVariant r;
        QMetaObject::invokeMethod(v->fixture.get(), "button", Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, index));
        return r.value<QQuickItem *>();
    }

    QStringList calls(View *v)
    {
        return v->fixture->property("calls").toStringList();
    }

private Q_SLOTS:
    void initTestCase()
    {
        s_self = this;
        QFile f(QStringLiteral(RAYE_TASKBAR_DIR "/contents/ui/taskbarlogic.js"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString code = QString::fromUtf8(f.readAll()).remove(u".pragma library"_s);
        const QJSValue r = m_js.evaluate(code);
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
        qInstallMessageHandler(collect);
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

    // Two mistakes that pass in a test window and leave the applet blank in
    // a real panel: "controller: controller" (inside the child that name is
    // its own property, so it binds to itself and stays undefined) and a
    // main.qml without a fullRepresentation (Plasma then never builds the
    // compact one either).
    void plasmaRules()
    {
        static const QRegularExpression selfBinding(u"^\\s*(\\w+)\\s*:\\s*\\1\\s*$"_s);
        QDirIterator it(QStringLiteral(RAYE_TASKBAR_DIR "/contents/ui"), {u"*.qml"_s}, QDir::Files, QDirIterator::Subdirectories);
        int files = 0;
        while (it.hasNext()) {
            QFile f(it.next());
            QVERIFY(f.open(QIODevice::ReadOnly));
            ++files;
            int line = 0;
            const QStringList lines = QString::fromUtf8(f.readAll()).split(u'\n');
            for (const QString &l : lines) {
                ++line;
                QVERIFY2(!selfBinding.match(l).hasMatch(),
                         qPrintable(u"%1:%2 binds a property to itself: %3"_s.arg(f.fileName()).arg(line).arg(l.trimmed())));
            }
        }
        QVERIFY(files > 0);
        QFile main(QStringLiteral(RAYE_TASKBAR_DIR "/contents/ui/main.qml"));
        QVERIFY(main.open(QIODevice::ReadOnly));
        const QString code = QString::fromUtf8(main.readAll());
        QVERIFY2(code.contains(u"fullRepresentation:"_s), "main.qml needs a fullRepresentation");
    }

    // A minimized window keeps its button. On Wayland the tasks model answers
    // IsHidden with IsMinimized (libtaskmanager/waylandtasksmodel.cpp), so
    // filterHidden drops every minimized window from the row: its indicator
    // and preview go, the pin comes back as a launcher, and a click on it
    // starts a second copy of the app while the first keeps running unseen.
    void minimizedWindowsKeepTheirButton()
    {
        QFile main(QStringLiteral(RAYE_TASKBAR_DIR "/contents/ui/main.qml"));
        QVERIFY(main.open(QIODevice::ReadOnly));
        const QString code = QString::fromUtf8(main.readAll());
        const qsizetype start = code.indexOf(u"TaskManager.TasksModel {"_s);
        QVERIFY2(start >= 0, "main.qml has no TasksModel, this test needs a new look");
        const QString model = code.mid(start);
        static const QRegularExpression filter(u"^\\s*(filterHidden|filterNotMinimized)\\s*:\\s*(\\S+)"_s,
                                               QRegularExpression::MultilineOption);
        auto it = filter.globalMatch(model);
        while (it.hasNext()) {
            const auto m = it.next();
            QVERIFY2(m.captured(2) == u"false"_s,
                     qPrintable(u"%1 takes minimized windows off the taskbar"_s.arg(m.captured(1))));
        }
    }

    // --- taskbarlogic.js --------------------------------------------------

    void desktopIds()
    {
        QCOMPARE(call("launcherForDesktopId", {u"org.kde.dolphin.desktop"_s}).toString(), u"applications:org.kde.dolphin.desktop"_s);
        QCOMPARE(call("launcherForDesktopId", {u"../x.desktop"_s}).toString(), QString());
        QCOMPARE(call("launcherForDesktopId", {u"/usr/share/applications/x.desktop"_s}).toString(), QString());
        QCOMPARE(call("launcherForDesktopId", {u"firefox"_s}).toString(), QString());

        QCOMPARE(call("desktopIdForLauncher", {u"applications:firefox.desktop"_s}).toString(), u"firefox.desktop"_s);
        QCOMPARE(call("desktopIdForLauncher", {u"file:///usr/share/applications/org.kde.kate.desktop"_s}).toString(), u"org.kde.kate.desktop"_s);
        QCOMPARE(call("desktopIdForLauncher", {u"applications:org.kde.dolphin.desktop?iconData=xyz"_s}).toString(), u"org.kde.dolphin.desktop"_s);
        QCOMPARE(call("desktopIdForLauncher", {u"https://example.org/a.desktop"_s}).toString(), QString());
        QCOMPARE(call("desktopIdForLauncher", {u"file:///usr/bin/xterm"_s}).toString(), QString());
        QCOMPARE(call("desktopIdForLauncher", {QString()}).toString(), QString());
    }

    void pinnedRoundTrip()
    {
        const QStringList ids{u"org.kde.dolphin.desktop"_s, u"firefox.desktop"_s, u"bad id"_s, u"firefox.desktop"_s};
        const QStringList urls = strings(call("launchersForPinned", {list(ids)}));
        QCOMPARE(urls, (QStringList{u"applications:org.kde.dolphin.desktop"_s, u"applications:firefox.desktop"_s}));
        QCOMPARE(strings(call("pinnedForLaunchers", {list(urls + QStringList{u"file:///usr/bin/foo"_s})})),
                 (QStringList{u"org.kde.dolphin.desktop"_s, u"firefox.desktop"_s}));
        QVERIFY(call("sameList", {list({u"a"_s, u"b"_s}), list({u"a"_s, u"b"_s})}).toBool());
        QVERIFY(!call("sameList", {list({u"a"_s, u"b"_s}), list({u"b"_s, u"a"_s})}).toBool());
    }

    void webAppPinKeepsItsOwnEntry()
    {
        // a site a browser installed as an app: its entry, never the browser's
        const QStringList ids{u"brave-abcdefghijklmnopabcdefghijklmnop-Default.desktop"_s,
                              u"com.brave.Browser.flextop.chrome-abcdefghijklmnopabcdefghijklmnop-Profile_1.desktop"_s};
        const QStringList urls = strings(call("launchersForPinned", {list(ids)}));
        QCOMPARE(urls.size(), 2);
        QCOMPARE(strings(call("pinnedForLaunchers", {list(urls)})), ids);
        QCOMPARE(call("desktopIdForLauncher", {u"file:///home/u/.local/share/applications/brave-abcdefghijklmnopabcdefghijklmnop-Default.desktop"_s}).toString(),
                 ids.first());
    }

    void pinsOnSeveralScreens()
    {
        // "Show my taskbar apps on": pins on every taskbar only for "all"
        QVERIFY(call("showPins", {u"all"_s, true}).toBool());
        QVERIFY(call("showPins", {u"all"_s, false}).toBool());
        QVERIFY(call("showPins", {u"main-and-open"_s, true}).toBool());
        QVERIFY(!call("showPins", {u"main-and-open"_s, false}).toBool());
        QVERIFY(call("showPins", {u"open"_s, true}).toBool());
        QVERIFY(!call("showPins", {u"open"_s, false}).toBool());
    }

    void stockPinsMoveOver()
    {
        // the owner's taskbar of today: stock launchers, in order
        const QString stock = u"applications:org.kde.dolphin.desktop,applications:firefox.desktop,"
                              "applications:dev.vencord.Vesktop.desktop,applications:org.example.Notes.desktop,"
                              "applications:org.raye.store.desktop,applications:org.raye.settings.desktop"_s;
        QCOMPARE(strings(call("pinnedFromStockLaunchers", {stock, u"firefox.desktop"_s})),
                 (QStringList{u"org.kde.dolphin.desktop"_s, u"firefox.desktop"_s, u"dev.vencord.Vesktop.desktop"_s,
                              u"org.example.Notes.desktop"_s, u"org.raye.store.desktop"_s, u"org.raye.settings.desktop"_s}));
        // "default browser" becomes the browser itself, once
        QCOMPARE(strings(call("pinnedFromStockLaunchers", {u"preferred://browser,applications:firefox.desktop"_s, u"firefox.desktop"_s})),
                 (QStringList{u"firefox.desktop"_s}));
        // unknown browser: dropped, never a broken pin
        QCOMPARE(strings(call("pinnedFromStockLaunchers", {u"preferred://browser,file:///usr/share/applications/org.kde.kate.desktop"_s, QString()})),
                 (QStringList{u"org.kde.kate.desktop"_s}));
        QCOMPARE(strings(call("pinnedFromStockLaunchers", {list({u"applications:a.desktop"_s, u"https://x"_s}), QString()})),
                 (QStringList{u"a.desktop"_s}));
        QVERIFY(strings(call("pinnedFromStockLaunchers", {QString(), QString()})).isEmpty());
    }

    void jumpListRows()
    {
        const auto kinds = [this](int recent, int actions, bool valid, bool pinned, bool launcher, int children, bool endTask) {
            QJSValue info = m_js.newObject();
            info.setProperty(u"recent"_s, recent);
            info.setProperty(u"actions"_s, actions);
            info.setProperty(u"valid"_s, valid);
            info.setProperty(u"pinned"_s, pinned);
            info.setProperty(u"isLauncher"_s, launcher);
            info.setProperty(u"childCount"_s, children);
            info.setProperty(u"endTask"_s, endTask);
            const QJSValue rows = call("jumpListEntries", {info});
            QStringList out;
            for (int i = 0; i < rows.property(u"length"_s).toInt(); ++i)
                out << rows.property(i).property(u"kind"_s).toString();
            return out;
        };
        // a running pinned app with recent files and tasks
        QCOMPARE(kinds(2, 1, true, true, false, 1, false),
                 (QStringList{u"header"_s, u"recent"_s, u"recent"_s, u"header"_s, u"action"_s, u"separator"_s,
                              u"app"_s, u"unpin"_s, u"close"_s}));
        // a pin that is not running: no close
        QCOMPARE(kinds(0, 0, true, true, true, 0, true), (QStringList{u"app"_s, u"unpin"_s}));
        // a running app not pinned, End task allowed
        QCOMPARE(kinds(0, 0, true, false, false, 2, true), (QStringList{u"app"_s, u"pin"_s, u"endTask"_s, u"close"_s}));
        // a window without an app entry: nothing to pin
        QCOMPARE(kinds(0, 0, false, false, false, 1, false), (QStringList{u"close"_s}));
    }

    void fitIcons()
    {
        // 10 icons in 1000 px: all fit, 44 px each
        QJSValue r = call("fit", {u"always"_s, u"never"_s, 10, 14, 1000});
        QCOMPARE(r.property(u"labels"_s).toBool(), false);
        QCOMPARE(r.property(u"small"_s).toBool(), false);
        QCOMPARE(r.property(u"count"_s).toInt(), 10);
        QCOMPARE(r.property(u"pitch"_s).toInt(), 44);
        QCOMPARE(r.property(u"buttonWidth"_s).toInt(), 40);
        QCOMPARE(r.property(u"visible"_s).toInt(), 10);
        QCOMPARE(r.property(u"overflow"_s).toBool(), false);
        QCOMPARE(r.property(u"width"_s).toInt(), 440);
    }

    void fitOverflow()
    {
        // 30 icons in 600 px: 13 fit before the "..." (13 * 44 + 44 = 616 > 600 -> 12)
        QJSValue r = call("fit", {u"always"_s, u"never"_s, 30, 30, 600});
        QCOMPARE(r.property(u"overflow"_s).toBool(), true);
        QCOMPARE(r.property(u"visible"_s).toInt(), 12);
        QVERIFY(r.property(u"width"_s).toInt() <= 600);
    }

    void fitSmall()
    {
        QJSValue always = call("fit", {u"always"_s, u"always"_s, 3, 3, 1000});
        QCOMPARE(always.property(u"small"_s).toBool(), true);
        QCOMPARE(always.property(u"pitch"_s).toInt(), 36);
        QCOMPARE(always.property(u"buttonWidth"_s).toInt(), 32);
        // "when full": normal while 20 x 44 fits, small past it
        QCOMPARE(call("fit", {u"always"_s, u"full"_s, 20, 20, 900}).property(u"small"_s).toBool(), false);
        QJSValue full = call("fit", {u"always"_s, u"full"_s, 22, 22, 900});
        QCOMPARE(full.property(u"small"_s).toBool(), true);
        QCOMPARE(full.property(u"overflow"_s).toBool(), false);
    }

    void fitLabels()
    {
        // never combine: one labelled button per window, as wide as the room allows
        QJSValue never = call("fit", {u"never"_s, u"never"_s, 3, 5, 2000});
        QCOMPARE(never.property(u"labels"_s).toBool(), true);
        QCOMPARE(never.property(u"count"_s).toInt(), 5);
        QCOMPARE(never.property(u"buttonWidth"_s).toInt(), 200);
        QCOMPARE(never.property(u"pitch"_s).toInt(), 204);
        // narrower room: down to 160, then the "..."
        QCOMPARE(call("fit", {u"never"_s, u"never"_s, 3, 5, 900}).property(u"buttonWidth"_s).toInt(), 176);
        QJSValue tight = call("fit", {u"never"_s, u"never"_s, 3, 5, 700});
        QCOMPARE(tight.property(u"buttonWidth"_s).toInt(), 160);
        QCOMPARE(tight.property(u"overflow"_s).toBool(), true);
        QCOMPARE(tight.property(u"visible"_s).toInt(), 4);
        // combine when full: labels while they fit, icons after
        QCOMPARE(call("fit", {u"full"_s, u"never"_s, 3, 5, 900}).property(u"labels"_s).toBool(), true);
        QJSValue combined = call("fit", {u"full"_s, u"never"_s, 3, 5, 700});
        QCOMPARE(combined.property(u"labels"_s).toBool(), false);
        QCOMPARE(combined.property(u"count"_s).toInt(), 3);
        // small buttons never apply to labelled buttons
        QCOMPARE(call("fit", {u"never"_s, u"full"_s, 3, 5, 700}).property(u"small"_s).toBool(), false);
    }

    void fitEmpty()
    {
        QJSValue r = call("fit", {u"always"_s, u"never"_s, 0, 0, 0});
        QCOMPARE(r.property(u"visible"_s).toInt(), 0);
        QCOMPARE(r.property(u"overflow"_s).toBool(), false);
        QCOMPARE(r.property(u"width"_s).toInt(), 0);
        QJSValue none = call("fit", {u"always"_s, u"never"_s, 5, 5, 20});
        QCOMPARE(none.property(u"visible"_s).toInt(), 0);
        QCOMPARE(none.property(u"overflow"_s).toBool(), true);
    }

    void rowStart()
    {
        QCOMPARE(call("rowStart", {u"center"_s, 400, 1000}).toInt(), 300);
        QCOMPARE(call("rowStart", {u"center"_s, 1200, 1000}).toInt(), 0);
        QCOMPARE(call("rowStart", {u"left"_s, 400, 1000}).toInt(), 0);
    }

    void clicks()
    {
        auto act = [&](QJSValue t, const QString &button, bool shift = false, bool ctrl = false) {
            return call("clickAction", {t, button, shift, ctrl}).toString();
        };
        const QJSValue pinned = task(true, false, false, false, 0);
        const QJSValue one = task(false, false, false, false, 0);
        const QJSValue focused = task(false, false, true, false, 0);
        const QJSValue focusedMin = task(false, false, true, true, 0);
        const QJSValue group = task(false, false, true, false, 3);
        const QJSValue starting = task(false, true, false, false, 0);
        QCOMPARE(act(pinned, u"left"_s), u"launch"_s);
        QCOMPARE(act(one, u"left"_s), u"activate"_s);
        QCOMPARE(act(focused, u"left"_s), u"minimize"_s);
        QCOMPARE(act(focusedMin, u"left"_s), u"activate"_s);
        QCOMPARE(act(group, u"left"_s), u"cycle"_s);
        QCOMPARE(act(starting, u"left"_s), u"none"_s);
        QCOMPARE(act(one, u"middle"_s), u"newInstance"_s);
        QCOMPARE(act(pinned, u"middle"_s), u"launch"_s);
        QCOMPARE(act(focused, u"left"_s, true), u"newInstance"_s);
        QCOMPARE(act(focused, u"left"_s, true, true), u"newInstance"_s);
        QCOMPARE(act(one, u"right"_s), u"none"_s);

        QCOMPARE(call("nextInGroup", {3, -1}).toInt(), 0);
        QCOMPARE(call("nextInGroup", {3, 0}).toInt(), 1);
        QCOMPARE(call("nextInGroup", {3, 2}).toInt(), 0);
        QCOMPARE(call("nextInGroup", {0, -1}).toInt(), -1);
    }

    void indicators()
    {
        QCOMPARE(call("indicator", {task(true, false, false, false, 0)}).toString(), u"none"_s);
        // an app starting: the loading pill until its window shows
        QCOMPARE(call("indicator", {task(false, true, false, false, 0)}).toString(), u"starting"_s);
        QCOMPARE(call("indicator", {task(true, true, false, false, 0)}).toString(), u"starting"_s);
        QCOMPARE(call("indicator", {task(false, false, false, false, 0)}).toString(), u"running"_s);
        QCOMPARE(call("indicator", {task(false, false, true, false, 0)}).toString(), u"focused"_s);
        QCOMPARE(call("indicator", {task(false, false, true, true, 0)}).toString(), u"running"_s);
    }

    void badges()
    {
        QCOMPARE(call("badgeText", {0}).toString(), QString());
        QCOMPARE(call("badgeText", {-3}).toString(), QString());
        QCOMPARE(call("badgeText", {1}).toString(), u"1"_s);
        QCOMPARE(call("badgeText", {99}).toString(), u"99"_s);
        QCOMPARE(call("badgeText", {100}).toString(), u"99+"_s);
        QCOMPARE(call("badgeText", {QJSValue()}).toString(), QString());
        QCOMPARE(call("appKey", {u"applications:org.kde.Dolphin.desktop"_s, u"x"_s}).toString(), u"org.kde.dolphin"_s);
        QCOMPARE(call("appKey", {QString(), u"firefox"_s}).toString(), u"firefox"_s);
        QCOMPARE(call("appKey", {QString(), u"org.kde.kate.desktop"_s}).toString(), u"org.kde.kate"_s);
    }

    void numbers()
    {
        QCOMPARE(call("indexForNumber", {1, 5}).toInt(), 0);
        QCOMPARE(call("indexForNumber", {5, 5}).toInt(), 4);
        QCOMPARE(call("indexForNumber", {6, 5}).toInt(), -1);
        QCOMPARE(call("indexForNumber", {0, 5}).toInt(), -1);
    }

    // A pointer sweeping right then back over five 40 px buttons 44 px
    // apart, dragging the first one: each button is passed once, at its
    // middle, and a hand that shakes on the line never swaps back and forth.
    // A dragged button never leaves the row: not over the Start button on
    // the left, not past the last button on the right (five buttons, 44 px).
    void dragStaysInTheRow()
    {
        QCOMPARE(call("clampedShift", {0, -300, 44, 5}).toNumber(), 0.0);
        QCOMPARE(call("clampedShift", {2, -300, 44, 5}).toNumber(), -88.0);
        QCOMPARE(call("clampedShift", {2, 500, 44, 5}).toNumber(), 88.0);
        QCOMPARE(call("clampedShift", {4, 100, 44, 5}).toNumber(), 0.0);
        QCOMPARE(call("clampedShift", {1, 30, 44, 5}).toNumber(), 30.0);
        // a single button cannot move at all
        QCOMPARE(call("clampedShift", {0, 50, 44, 1}).toNumber(), 0.0);
    }

    void dragSlots()
    {
        QList<int> seen;
        int t = -1;
        auto step = [&](qreal px) {
            t = call("dragSlot", {0, t, px, 44, 40, 5}).toInt();
            if (seen.isEmpty() || seen.last() != t)
                seen << t;
        };
        for (qreal px = 20; px <= 210; px += 1)
            step(px);
        QCOMPARE(seen, (QList<int>{0, 1, 2, 3, 4}));
        // past the middle of the third button (2 * 44 + 20), not its edge
        t = -1;
        QCOMPARE(call("dragSlot", {0, -1, 108, 44, 40, 5}).toInt(), 1);
        QCOMPARE(call("dragSlot", {0, -1, 109, 44, 40, 5}).toInt(), 2);
        // shaking around a middle: one step, and no step back
        t = 1;
        seen = {1};
        for (int i = 0; i < 20; ++i)
            step(i % 2 ? 106 : 110);
        QCOMPARE(seen, (QList<int>{1, 2}));
        // back left: the button that moved over is passed at its middle
        for (qreal px = 110; px >= 0; px -= 1)
            step(px);
        QCOMPARE(seen, (QList<int>{1, 2, 1, 0}));
        QCOMPARE(call("shiftedSlot", {1, 0, 2}).toInt(), 0);
        QCOMPARE(call("shiftedSlot", {2, 0, 2}).toInt(), 1);
        QCOMPARE(call("shiftedSlot", {3, 0, 2}).toInt(), 3);
        QCOMPARE(call("shiftedSlot", {0, 0, 2}).toInt(), 0);
        QCOMPARE(call("shiftedSlot", {1, 3, 1}).toInt(), 2);
        QCOMPARE(call("shiftedSlot", {1, -1, -1}).toInt(), 1);
    }

    // --- TaskBar.qml on a stand-in model ----------------------------------

    void rowAndIndicators()
    {
        auto v = view({}, {app(u"Files"_s, u"org.kde.dolphin"_s, {{u"IsLauncher"_s, true}, {u"IsWindow"_s, false}}),
                           app(u"Firefox"_s, u"firefox"_s, {{u"IsActive"_s, true}}),
                           app(u"Kate"_s, u"org.kde.kate"_s)});
        QVERIFY(v);
        QCOMPARE(v->bar->property("buttonCount").toInt(), 3);
        QCOMPARE(button(v.get(), 0)->property("indicator").toString(), u"none"_s);
        QCOMPARE(button(v.get(), 1)->property("indicator").toString(), u"focused"_s);
        QCOMPARE(button(v.get(), 2)->property("indicator").toString(), u"running"_s);
        // the long pill once its 167 ms are over
        QTRY_COMPARE(button(v.get(), 1)->findChild<QQuickItem *>(u"pill"_s)->width(), 16.0);
        QTRY_COMPARE(button(v.get(), 2)->findChild<QQuickItem *>(u"pill"_s)->width(), 6.0);
        QCOMPARE(button(v.get(), 0)->findChild<QQuickItem *>(u"pill"_s)->opacity(), 0.0);
        // centred in the bar when no Start button is next to it
        QCOMPARE(v->bar->property("rowX").toInt(), (1000 - 3 * 44) / 2);
        QCOMPARE(button(v.get(), 1)->width(), 40.0);
    }

    // Windows 10: square buttons as tall as the taskbar, side by side, and a
    // line along the bottom of a running app
    void windows10Buttons()
    {
        auto v = view({{u"windows10"_s, true}, {u"alignment"_s, u"left"_s}},
                      {app(u"Files"_s, u"org.kde.dolphin"_s, {{u"IsLauncher"_s, true}, {u"IsWindow"_s, false}}),
                       app(u"Firefox"_s, u"firefox"_s, {{u"IsActive"_s, true}}),
                       app(u"Kate"_s, u"org.kde.kate"_s)});
        QVERIFY(v);
        QQuickItem *active = button(v.get(), 1);
        QCOMPARE(active->width(), 44.0);
        QCOMPARE(active->height(), 48.0);
        QCOMPARE(active->findChild<QQuickItem *>(u"backplate"_s)->property("radius").toReal(), 0.0);
        QTRY_COMPARE(active->findChild<QQuickItem *>(u"pill"_s)->width(), 44.0);
        QCOMPARE(active->findChild<QQuickItem *>(u"pill"_s)->height(), 2.0);
        QTRY_COMPARE(button(v.get(), 2)->findChild<QQuickItem *>(u"pill"_s)->width(), 32.0);
        QCOMPARE(button(v.get(), 0)->findChild<QQuickItem *>(u"pill"_s)->opacity(), 0.0);
        // the line sits on the bottom edge
        QQuickItem *line = button(v.get(), 2)->findChild<QQuickItem *>(u"pill"_s);
        QCOMPARE(line->mapToScene(QPointF(0, line->height())).y(), 48.0);
        QCOMPARE(button(v.get(), 2)->x() - button(v.get(), 1)->x(), 44.0);
    }

    void leftAligned()
    {
        auto v = view({{u"alignment"_s, u"left"_s}}, {app(u"Kate"_s, u"org.kde.kate"_s)});
        QVERIFY(v);
        QCOMPARE(v->bar->property("rowX").toInt(), 0);
    }

    void clicking()
    {
        auto v = view({}, {app(u"Files"_s, u"org.kde.dolphin"_s, {{u"IsLauncher"_s, true}, {u"IsWindow"_s, false}}),
                           app(u"Firefox"_s, u"firefox"_s, {{u"IsActive"_s, true}}),
                           app(u"Kate"_s, u"org.kde.kate"_s)});
        QVERIFY(v);
        QTest::mouseClick(v->window, Qt::LeftButton, {}, button(v.get(), 0)->mapToScene(QPointF(20, 20)).toPoint());
        QTest::mouseClick(v->window, Qt::LeftButton, {}, button(v.get(), 1)->mapToScene(QPointF(20, 20)).toPoint());
        QTest::mouseClick(v->window, Qt::LeftButton, {}, button(v.get(), 2)->mapToScene(QPointF(20, 20)).toPoint());
        QTest::mouseClick(v->window, Qt::MiddleButton, {}, button(v.get(), 2)->mapToScene(QPointF(20, 20)).toPoint());
        QTest::mouseClick(v->window, Qt::LeftButton, Qt::ShiftModifier, button(v.get(), 1)->mapToScene(QPointF(20, 20)).toPoint());
        QCOMPARE(calls(v.get()), (QStringList{u"launch 0"_s, u"minimize 1"_s, u"activate 2"_s, u"newInstance 2"_s, u"newInstance 1"_s}));
        // Meta+3
        QMetaObject::invokeMethod(v->bar, "activateTaskAtIndex", Q_ARG(QVariant, 2));
        QCOMPARE(calls(v.get()).last(), u"activate 2"_s);
    }

    // Dragging a button: it stays under the pointer to the pixel, the others
    // slide over once each as the pointer passes their middle, and what is
    // let go is the order that was shown. Slow and fast, both ways.
    void dragToReorder_data()
    {
        QTest::addColumn<int>("from");
        QTest::addColumn<int>("travel");
        QTest::addColumn<int>("stepPx");
        QTest::addColumn<int>("to");
        QTest::newRow("pinned, slowly right") << 0 << 100 << 2 << 2;
        QTest::newRow("open, fast right") << 1 << 140 << 17 << 4;
        QTest::newRow("open, slowly left") << 3 << -100 << 3 << 1;
        QTest::newRow("pinned, fast left") << 4 << -180 << 23 << 0;
        // let go far past either end of the row
        QTest::newRow("open, far past the end") << 2 << 400 << 20 << 4;
        QTest::newRow("open, far before the start") << 2 << -400 << 20 << 0;
    }

    void dragToReorder()
    {
        QFETCH(int, from);
        QFETCH(int, travel);
        QFETCH(int, stepPx);
        QFETCH(int, to);
        auto v = view({}, {app(u"Files"_s, u"org.kde.dolphin"_s, {{u"IsLauncher"_s, true}, {u"IsWindow"_s, false}}),
                           app(u"Firefox"_s, u"firefox"_s),
                           app(u"Kate"_s, u"org.kde.kate"_s),
                           app(u"Mail"_s, u"org.kde.kmail2"_s),
                           app(u"Store"_s, u"org.raye.store"_s, {{u"IsLauncher"_s, true}, {u"IsWindow"_s, false}})});
        QVERIFY(v);
        QList<QPointer<QQuickItem>> buttons;
        for (int i = 0; i < 5; ++i)
            buttons << button(v.get(), i);
        QQuickItem *dragged = buttons.at(from);
        const qreal pitch = v->bar->property("layout").toMap().value(u"pitch"_s).toReal();
        // the row has finished sliding in
        QTRY_COMPARE(dragged->x(), from * pitch);
        QTRY_COMPARE(dragged->parentItem()->x(), v->bar->property("rowX").toReal());
        const qreal startX = dragged->x();
        const QPoint press = dragged->mapToScene(QPointF(20, 20)).toPoint();

        QTest::mouseMove(v->window, press);
        QTest::mousePress(v->window, Qt::LeftButton, {}, press);
        // each other button: the places it was shown at, in order
        QList<QList<qreal>> places(5);
        const int dir = travel > 0 ? 1 : -1;
        for (int moved = stepPx; moved <= qAbs(travel) + stepPx - 1; moved += stepPx) {
            const int d = dir * qMin(moved, qAbs(travel));
            QTest::mouseMove(v->window, press + QPoint(d, 0));
            if (qAbs(d) <= qApp->styleHints()->startDragDistance())
                continue;
            QVERIFY(dragged->property("dragging").toBool());
            // under the pointer to the pixel, but never out of the row: it
            // stops at the first and the last place (not over Start)
            const qreal shift = qBound(-from * pitch, qreal(d), (4 - from) * pitch);
            QCOMPARE(dragged->mapToItem(dragged->parentItem(), QPointF(0, 0)).x(), startX + shift);
            for (int i = 0; i < 5; ++i) {
                if (i == from)
                    continue;
                const qreal place = buttons.at(i)->property("slot").toInt() * pitch;
                if (places[i].isEmpty() || places[i].last() != place)
                    places[i] << place;
            }
        }
        // the ones passed moved over once, the others not at all
        for (int i = 0; i < 5; ++i) {
            if (i == from)
                continue;
            const bool passed = (from < to && i > from && i <= to) || (from > to && i >= to && i < from);
            QCOMPARE(places[i].size(), passed ? 2 : 1);
            if (passed)
                QCOMPARE(places[i].last() - places[i].first(), from < to ? -pitch : pitch);
        }
        QTest::mouseRelease(v->window, Qt::LeftButton, {}, press + QPoint(travel, 0));
        QCOMPARE(calls(v.get()), QStringList{QString(u"move %1 %2"_s).arg(from).arg(to)});
        // let go, it eases in from where it was shown, still in the row: it
        // used to jump out to the pointer and slide back
        const qreal landing = dragged->mapToItem(dragged->parentItem(), QPointF(0, 0)).x();
        QVERIFY2(landing >= 0 && landing <= 4 * pitch, qPrintable(QString::number(landing)));
        // nobody slides back to the old order on the way
        for (int i = 0; i < 5; ++i) {
            if (i != from)
                QCOMPARE(buttons.at(i)->x(), places[i].last());
        }
        QTRY_COMPARE(dragged->mapToItem(dragged->parentItem(), QPointF(0, 0)).x(), to * pitch);
        QCOMPARE(button(v.get(), to), dragged);
    }

    void badgeAndAttention()
    {
        auto v = view({}, {app(u"Mail"_s, u"org.kde.kmail2"_s, {{u"IsDemandingAttention"_s, true}}),
                           app(u"Chat"_s, u"org.kde.neochat"_s)});
        QVERIFY(v);
        QMetaObject::invokeMethod(v->fixture.get(), "setBadge", Q_ARG(QVariant, u"org.kde.neochat"_s), Q_ARG(QVariant, 120));
        QTRY_COMPARE(button(v.get(), 1)->property("badge").toString(), u"99+"_s);
        QCOMPARE(button(v.get(), 0)->property("badge").toString(), QString());
        QVERIFY(button(v.get(), 0)->property("attention").toBool());
        QVERIFY(!button(v.get(), 1)->property("attention").toBool());
        // badges off in Settings: none shown
        QMetaObject::invokeMethod(v->fixture.get(), "setOption", Q_ARG(QVariant, u"badges"_s), Q_ARG(QVariant, false));
        QTRY_COMPARE(button(v.get(), 1)->property("badge").toString(), QString());
        // flashing off: no amber backplate
        QMetaObject::invokeMethod(v->fixture.get(), "setOption", Q_ARG(QVariant, u"flashing"_s), Q_ARG(QVariant, false));
        QTRY_VERIFY(!button(v.get(), 0)->property("attention").toBool());
    }

    void labels()
    {
        auto v = view({{u"combine"_s, u"never"_s}}, {app(u"Files"_s, u"org.kde.dolphin"_s),
                                                    app(u"Kate"_s, u"org.kde.kate"_s)});
        QVERIFY(v);
        QVERIFY(v->bar->property("labels").toBool());
        QCOMPARE(button(v.get(), 0)->width(), 200.0);
        QVERIFY(button(v.get(), 0)->property("labelShown").toBool());
    }

    void overflow()
    {
        QVariantList many;
        for (int i = 0; i < 20; ++i)
            many << app(u"App %1"_s.arg(i), u"org.kde.kate"_s, {{u"AppId"_s, u"app%1"_s.arg(i)}, {u"LauncherUrlWithoutIcon"_s, QString()}});
        auto v = view({}, many, 400);
        QVERIFY(v);
        QVERIFY(v->bar->property("overflowing").toBool());
        QCOMPARE(v->bar->property("visibleCount").toInt(), 8);
        QVERIFY(button(v.get(), 7)->isVisible());
        QVERIFY(!button(v.get(), 8)->isVisible());
        QQuickItem *more = v->bar->findChild<QQuickItem *>(u"overflowButton"_s);
        QVERIFY(more && more->isVisible());
        QCOMPARE(more->property("hiddenCount").toInt(), 12);
    }

    void smallButtons()
    {
        auto v = view({{u"smallButtons"_s, u"always"_s}}, {app(u"Kate"_s, u"org.kde.kate"_s)});
        QVERIFY(v);
        QVERIFY(v->bar->property("small").toBool());
        QCOMPARE(button(v.get(), 0)->width(), 32.0);
        QCOMPARE(button(v.get(), 0)->height(), 32.0);
        QCOMPARE(button(v.get(), 0)->property("iconSize").toInt(), 16);
    }

    void french()
    {
        // the stub of the overflow flyout and the button names are translated
        QFile po(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_taskbar.po"));
        QVERIFY(po.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(po.readAll());
        QVERIFY(text.contains(u"msgid \"Show more apps\""_s));
        QVERIFY(text.contains(u"msgstr \"Afficher plus d’applications\""_s));
    }
};

QTEST_MAIN(TestTaskbar)

#include "tst_taskbar.moc"
