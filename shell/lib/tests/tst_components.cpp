// The QML side of org.raye.shell: every component loads without a
// warning, the tokens have the shell's sizes, the curves are Windows' and
// the small helpers (Settings pages, shortcuts, blur shape) check what
// they are given.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "blurbehind.h"
#include "globalshortcut.h"
#include "activeoutput.h"
#include "launcher.h"
#include "motion.h"

#include <QElapsedTimer>
#include <QFile>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QDir>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QTest>

#include <cmath>

using namespace Qt::StringLiterals;

class TestComponents : public QObject
{
    Q_OBJECT

private:
    QStringList m_warnings;

    // what the blur asked of the compositor, in order
    struct BlurCall {
        QWindow *window;
        bool enable;
        QRegion region;
    };
    static QList<BlurCall> &blurCalls()
    {
        static QList<BlurCall> calls;
        return calls;
    }

    std::unique_ptr<QObject> load(QQmlEngine &engine, const QByteArray &qml)
    {
        QQmlComponent c(&engine);
        c.setData(qml, QUrl(u"file:///test.qml"_s));
        std::unique_ptr<QObject> o(c.create());
        if (!o)
            qWarning() << c.errors();
        return o;
    }

    static void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
    {
        // what Plasma's dialog window says in a test without a window
        // system; nothing of ours is behind it
        if (msg.startsWith(u"QObject::installEventFilter(): Cannot filter events for objects in a different thread"_s)
            || msg == u"Could not find any platform plugin"_s)
            return;
        if (type != QtDebugMsg && type != QtInfoMsg)
            s_self->m_warnings << msg;
    }
    static inline TestComponents *s_self = nullptr;

    void engineSetup(QQmlEngine &engine)
    {
        engine.addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        engine.addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
    }

    // WCAG relative luminance and contrast, with alpha laid over `below`
    static QColor over(const QColor &top, const QColor &below)
    {
        const qreal a = top.alphaF();
        return QColor::fromRgbF(top.redF() * a + below.redF() * (1 - a), top.greenF() * a + below.greenF() * (1 - a),
                                top.blueF() * a + below.blueF() * (1 - a));
    }
    static qreal luminance(const QColor &c)
    {
        auto lin = [](qreal v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
    }
    static qreal contrast(const QColor &text, const QColor &background)
    {
        const qreal a = luminance(over(text, background)) + 0.05;
        const qreal b = luminance(background) + 0.05;
        return std::max(a, b) / std::min(a, b);
    }
    // What the taskbar is really painted with: its SVG takes the Window
    // background of the colour scheme of that mode (ColorScheme-Background).
    static QColor panelColour(const QString &mode)
    {
        QFile f(QStringLiteral(RAYE_THEME_QML_DIR "/../colors/Raye") + (mode == u"day"_s ? u"Day"_s : u"Night"_s) + u".colors"_s);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        const QString text = QString::fromUtf8(f.readAll());
        const auto m = QRegularExpression(u"\\[Colors:Window\\][^\\[]*?BackgroundNormal=(\\d+),(\\d+),(\\d+)"_s).match(text);
        return m.hasMatch() ? QColor(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()) : QColor();
    }

private Q_SLOTS:
    void initTestCase()
    {
        s_self = this;
    }

    void components_data()
    {
        QTest::addColumn<QByteArray>("qml");
        QTest::newRow("AcrylicSurface") << QByteArray("AcrylicSurface { width: 300; height: 200; focused: true }");
        QTest::newRow("PanelButton") << QByteArray("PanelButton { icon.name: \"raye-eye\"; toolTipText: \"Start\" }");
        QTest::newRow("RayeMenu") << QByteArray("RayeMenu { RayeMenuItem { text: \"Open\"; shortcutText: \"Ctrl+O\" } RayeMenuSeparator {} "
                                                 "RayeMenuItem { text: \"Pinned\"; checkable: true; checked: true } }");
        QTest::newRow("PanelMenu") << QByteArray("PanelMenu { RayeMenuItem { text: \"Task Manager\" } RayeMenuSeparator {} "
                                                  "RayeMenuItem { text: \"Taskbar settings\" } }");
        QTest::newRow("RayeSlider") << QByteArray("RayeSlider { from: 0; to: 100; value: 40 }");
        QTest::newRow("RayeToggleTile") << QByteArray("RayeToggleTile { text: \"Wi-Fi\"; iconName: \"network-wireless\"; split: true; checked: true }");
        QTest::newRow("RayeTooltip") << QByteArray("RayeTooltip { text: \"Search\" }");
    }

    void components()
    {
        QFETCH(QByteArray, qml);
        QQmlEngine engine;
        engineSetup(engine);
        m_warnings.clear();
        const auto old = qInstallMessageHandler(collect);
        auto o = load(engine, "import QtQuick\nimport org.raye.shell\nItem { width: 400; height: 300; " + qml + " }");
        QCoreApplication::processEvents();
        qInstallMessageHandler(old);
        QVERIFY(o);
        QVERIFY2(m_warnings.isEmpty(), qPrintable(m_warnings.join(u'\n')));
    }

    // A menu filled again (a taskbar button's menu, opened for one app
    // then another) holds the new rows only, right away: the old ones
    // made it twice as tall as its content
    void panelMenuRefilled()
    {
        QQmlEngine engine;
        engineSetup(engine);
        auto o = load(engine, "import QtQuick\nimport QtQuick.Window\nimport org.raye.shell\nimport org.raye.ui\n"
                              "Window {\n"
                              "  width: 400; height: 300; visible: true\n"
                              "  property alias menu: m\n"
                              "  Item { id: anchor; y: 250; width: 40; height: 40 }\n"
                              "  function show() { m.open(anchor, 20) }\n"
                              "  PanelMenu { id: m }\n"
                              "  Component { id: row; RayeMenuItem {} }\n"
                              "  Component { id: line; RayeMenuSeparator {} }\n"
                              "  function fill(n) {\n"
                              "    m.clear()\n"
                              "    for (let i = 0; i < n; ++i) {\n"
                              "      m.addItem(row.createObject(null, { text: 'Row ' + i }))\n"
                              "      if (i === 1) m.addItem(line.createObject(null, {}))\n"
                              "    }\n"
                              "  }\n"
                              "  function rows() { return m.rows.length }\n"
                              "}");
        QVERIFY(o);
        auto *menu = o->property("menu").value<QObject *>();
        QVERIFY(menu);
        auto rows = [&] {
            QVariant n;
            QMetaObject::invokeMethod(o.get(), "rows", Q_RETURN_ARG(QVariant, n));
            return n.toInt();
        };
        auto openWith = [&](int n) {
            QMetaObject::invokeMethod(menu, "close");
            QMetaObject::invokeMethod(o.get(), "fill", Q_ARG(QVariant, n));
            QMetaObject::invokeMethod(o.get(), "show");
        };
        auto height = [&] { return menu->property("contentHeight").toReal(); };
        openWith(3);
        QCOMPARE(rows(), 4);
        openWith(7);
        QCOMPARE(rows(), 8);
        QTRY_VERIFY(height() > 200);
        QTest::qWait(100);
        const qreal seven = height();
        // what JumpList does: clear and fill in one go, then open; the rows
        // measured for the opening are the new ones only
        openWith(3);
        QCOMPARE(rows(), 4);
        QTRY_VERIFY(height() < seven);
        QTest::qWait(100);
        const qreal three = height();
        // the same three rows in a menu that held nothing before
        openWith(0);
        QCoreApplication::processEvents();
        openWith(3);
        QTest::qWait(100);
        QCOMPARE(height(), three);
        QMetaObject::invokeMethod(menu, "close");
    }

    void tokens()
    {
        QQmlEngine engine;
        engineSetup(engine);
        auto o = load(engine, "import QtQuick\nimport org.raye.shell\nQtObject {"
                              " property int b: ShellTokens.buttonSize; property int p: ShellTokens.buttonPitch;"
                              " property int t: ShellTokens.taskbarHeight; property int i: ShellTokens.iconSize;"
                              " property int ps: ShellTokens.pillShort; property int pl: ShellTokens.pillLong;"
                              " property int ph: ShellTokens.pillHeight; property real r: ShellTokens.radiusButton;"
                              " property real f: ShellTokens.radiusFlyout; property int d: ShellTokens.tooltipDelay;"
                              " property real s: ShellTokens.pressScale; property color a: ShellTokens.attention }");
        QVERIFY(o);
        QCOMPARE(o->property("b").toInt(), 40);
        QCOMPARE(o->property("p").toInt(), 44);
        QCOMPARE(o->property("t").toInt(), 48);
        QCOMPARE(o->property("i").toInt(), 24);
        QCOMPARE(o->property("ps").toInt(), 6);
        QCOMPARE(o->property("pl").toInt(), 16);
        QCOMPARE(o->property("ph").toInt(), 3);
        QCOMPARE(o->property("r").toReal(), 4.0);
        QCOMPARE(o->property("f").toReal(), 8.0);
        QCOMPARE(o->property("d").toInt(), 500);
        QCOMPARE(o->property("s").toReal(), 0.9);
        QCOMPARE(o->property("a").value<QColor>(), QColor(u"#e0a458"_s));
    }

    // the Windows 10 look: every corner of the shell is square
    void squareInWindows10()
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        QDir().mkpath(dir);
        QFile f(dir + u"/rayeshellrc"_s);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("[Appearance]\nLook=windows10\n");
        f.close();
        auto removeFile = qScopeGuard([&] { QFile::remove(f.fileName()); });
        QQmlEngine engine;
        engineSetup(engine);
        auto o = load(engine, "import QtQuick\nimport org.raye.shell\nQtObject {"
                              " property real r: ShellTokens.radiusButton; property real f: ShellTokens.radiusFlyout;"
                              " property real t: ShellTokens.radiusTooltip; property bool w: ShellTokens.windows10 }");
        QVERIFY(o);
        QVERIFY(o->property("w").toBool());
        QCOMPARE(o->property("r").toReal(), 0.0);
        QCOMPARE(o->property("f").toReal(), 0.0);
        QCOMPARE(o->property("t").toReal(), 0.0);
    }

    void textContrast_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::newRow("night") << u"night"_s;
        QTest::newRow("day") << u"day"_s;
    }

    // Every text of the shell reads at 4.5:1 or better on what it sits
    // on, in Raye Night and in Raye Day: the clock, the search box and the
    // labels on the taskbar, and the words in the flyouts and toasts.
    void textContrast()
    {
        QFETCH(QString, mode);
        QQmlEngine engine;
        engineSetup(engine);
        auto o = load(engine, "import QtQuick\nimport org.raye.ui\nimport org.raye.shell\nQtObject {"
                              " function colours(mode) { Tokens.forceMode = mode; return {"
                              " taskbarSolid: ShellTokens.taskbarSolid, taskbarText: ShellTokens.taskbarText,"
                              " text: ShellTokens.text, textSecondary: ShellTokens.textSecondary,"
                              " acrylic: ShellTokens.acrylicSolid, tooltip: ShellTokens.tooltip,"
                              " accent: ShellTokens.accent, textOnAccent: ShellTokens.textOnAccent } } }");
        QVERIFY(o);
        QVariant v;
        QMetaObject::invokeMethod(o.get(), "colours", Q_RETURN_ARG(QVariant, v), Q_ARG(QVariant, mode));
        const QVariantMap c = v.toMap();
        auto col = [&](const char *key) { return c.value(QString::fromLatin1(key)).value<QColor>(); };

        const QColor panel = panelColour(mode);
        QVERIFY(panel.isValid());
        // the shell picks its taskbar text from the shade it thinks the
        // taskbar has: that shade must be the one on the screen
        const auto taskbar = col("taskbarSolid");
        QVERIFY2(std::abs(luminance(taskbar) - luminance(panel)) < 0.1,
                 qPrintable(u"taskbar token %1, taskbar drawn %2"_s.arg(taskbar.name(), panel.name())));

        struct Pair { const char *what; QColor text; QColor background; };
        QColor dim = col("taskbarText");
        dim.setAlphaF(0.72);
        const Pair pairs[] = {
            {"clock and taskbar labels", col("taskbarText"), panel},
            {"search box words", dim, panel},
            {"flyout text", col("text"), col("acrylic")},
            {"flyout secondary text", col("textSecondary"), col("acrylic")},
            {"tooltip", col("text"), col("tooltip")},
            {"text on the accent", col("textOnAccent"), col("accent")},
        };
        for (const Pair &p : pairs) {
            const qreal ratio = contrast(p.text, p.background);
            QVERIFY2(ratio >= 4.5, qPrintable(u"%1: %2 on %3 is %4:1"_s.arg(QString::fromLatin1(p.what), p.text.name(QColor::HexArgb),
                                                                         p.background.name(), QString::number(ratio, 'f', 2))));
        }
    }

    void panelButtonAndTooltip()
    {
        QQmlEngine engine;
        engineSetup(engine);
        auto o = load(engine, "import QtQuick\nimport org.raye.shell\nItem { property alias b: btn; property alias t: tip;"
                              " PanelButton { id: btn } RayeTooltip { id: tip } }");
        QVERIFY(o);
        auto *b = o->property("b").value<QQuickItem *>();
        QVERIFY(b);
        QCOMPARE(b->implicitWidth(), 40.0);
        QCOMPARE(b->implicitHeight(), 40.0);
        QCOMPARE(o->property("t").value<QObject *>()->property("delay").toInt(), 500);
    }

    void motion()
    {
        for (const QEasingCurve &c : {Motion::entrance(), Motion::exit(), Motion::pointToPoint()}) {
            QCOMPARE(c.type(), QEasingCurve::BezierSpline);
            QCOMPARE(c.valueForProgress(0.0), 0.0);
            QVERIFY(qAbs(c.valueForProgress(1.0) - 1.0) < 1e-6);
        }
        // entrance decelerates hard, exit accelerates hard (the values
        // of cubic-bezier(0,0,0,1) at 0.25 and (1,0,1,1) at 0.75)
        QVERIFY(qAbs(Motion::entrance().valueForProgress(0.25) - 0.69) < 0.02);
        QVERIFY(qAbs(Motion::exit().valueForProgress(0.75) - 0.31) < 0.02);
        QVERIFY(Motion::pointToPoint().valueForProgress(0.5) > 0.5);
        // and QML can put them on an animation as they are
        QQmlEngine engine;
        engineSetup(engine);
        auto o = load(engine, "import QtQuick\nimport org.raye.shell\nNumberAnimation { easing: Motion.entrance }");
        QVERIFY(o);
        QCOMPARE(o->property("easing").value<QEasingCurve>().type(), QEasingCurve::BezierSpline);
    }

    void activeOutput()
    {
        // KWin names the screen in use; an unknown or missing name keeps the fallback
        const QStringList names{u"eDP-1"_s, u"HDMI-A-1"_s};
        QCOMPARE(ActiveOutputLogic::pick(names, u"HDMI-A-1"_s, 0), 1);
        QCOMPARE(ActiveOutputLogic::pick(names, u"eDP-1"_s, 1), 0);
        QCOMPARE(ActiveOutputLogic::pick(names, u"DP-3"_s, 1), 1);
        QCOMPARE(ActiveOutputLogic::pick(names, QString(), 0), 0);
        // no KWin here: the primary screen
        ActiveOutput out;
        QCOMPARE(out.screen(), QGuiApplication::primaryScreen());
        QCOMPARE(out.geometry(), QGuiApplication::primaryScreen()->geometry());
    }

    void endTask()
    {
        // only the user's own processes, never pid 0/1, never as root
        QVERIFY(Launcher::mayEnd(1234, 1000, 1000));
        QVERIFY(!Launcher::mayEnd(1234, 0, 1000));
        QVERIFY(!Launcher::mayEnd(1234, 1001, 1000));
        QVERIFY(!Launcher::mayEnd(1, 1000, 1000));
        QVERIFY(!Launcher::mayEnd(0, 1000, 1000));
        QVERIFY(!Launcher::mayEnd(1234, 0, 0));
        Launcher l;
        QVERIFY(!l.endProcess(1));
        QVERIFY(!l.endProcess(QCoreApplication::applicationPid()));
        QVERIFY(!l.endProcess(-5));
    }

    // what the Ctrl+Alt+Del screen starts must be a normal window
    void appsLeaveLayerShellBehind()
    {
        Launcher l;
        qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");
        l.launch(u"no.such.app.raye-test.desktop"_s);
        QVERIFY(!qEnvironmentVariableIsSet("QT_WAYLAND_SHELL_INTEGRATION"));

        qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");
        l.openUrl(QUrl());
        QVERIFY(!qEnvironmentVariableIsSet("QT_WAYLAND_SHELL_INTEGRATION"));

        // someone's own choice stays
        qputenv("QT_WAYLAND_SHELL_INTEGRATION", "xdg-shell");
        l.launch(u"no.such.app.raye-test.desktop"_s);
        QCOMPARE(qgetenv("QT_WAYLAND_SHELL_INTEGRATION"), QByteArray("xdg-shell"));
        qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");
    }

    void settingsPages()
    {
        QVERIFY(Launcher::isPageId(u"system/display"_s));
        QVERIFY(Launcher::isPageId(u"kcm_kscreen"_s));
        QVERIFY(Launcher::isPageId(u"personalization/taskbar"_s));
        QVERIFY(!Launcher::isPageId(u"--help"_s));
        QVERIFY(!Launcher::isPageId(u"../../etc"_s));
        QVERIFY(!Launcher::isPageId(u"a b"_s));
        QVERIFY(!Launcher::isPageId(u"a;reboot"_s));
        QVERIFY(!Launcher::isPageId(QString(200, u'a')));
        QCOMPARE(Launcher::settingsArguments(u"system/display"_s), QStringList({u"--page"_s, u"system/display"_s}));
        QVERIFY(Launcher::settingsArguments(QString()).isEmpty());

        Launcher l;
        QSignalSpy failed(&l, &Launcher::failed);
        QVERIFY(!l.openSettings(u"-x"_s));
        QCOMPARE(failed.count(), 1);
        QVERIFY(!l.launch(u"no.such.app.raye-test.desktop"_s));
        QVERIFY(!l.launch(u"../etc/x.desktop"_s));
        QVERIFY(!l.openUrl(QUrl()));
        QVERIFY(!l.openUrl(QUrl(u"relative/path"_s)));
        QVERIFY(!l.invokeShortcut(u"kwin/../x"_s, u"Overview"_s));
        QVERIFY(!l.invokeShortcut(u"kwin"_s, QString()));
        QVERIFY(Launcher::isComponentName(u"kwin"_s));
        QVERIFY(Launcher::isComponentName(u"org_kde_powerdevil"_s));
    }

    void shortcutWaitsForItsName()
    {
        GlobalShortcut s;
        s.classBegin();
        s.setText(u"Open quick settings"_s);
        s.setSequence(u"Meta+A"_s);
        QVERIFY(!s.isRegistered());
        s.componentComplete();
        // no name: nothing goes to kglobalaccel
        QVERIFY(!s.isRegistered());
        QCOMPARE(s.component(), u"plasmashell"_s);
        QCOMPARE(s.activeSequence(), u"Meta+A"_s);
        s.setComponent(QString());
        QCOMPARE(s.component(), u"plasmashell"_s);
    }

    void blurShape()
    {
        const QRegion square = BlurBehind::roundedRegion(QRect(0, 0, 100, 50), 0);
        QCOMPARE(square, QRegion(QRect(0, 0, 100, 50)));
        const QRegion r = BlurBehind::roundedRegion(QRect(10, 20, 100, 50), 8);
        QVERIFY(r.contains(QPoint(60, 45)));
        // the corners are cut, the edges between them are not
        QVERIFY(!r.contains(QPoint(10, 20)));
        QVERIFY(!r.contains(QPoint(109, 69)));
        QVERIFY(r.contains(QPoint(60, 20)));
        QVERIFY(r.contains(QPoint(10, 45)));
        QVERIFY(r.boundingRect() == QRect(10, 20, 100, 50));
        QVERIFY(BlurBehind::roundedRegion(QRect(), 8).isEmpty());
        // a radius bigger than the box is held to half of it
        QCOMPARE(BlurBehind::roundedRegion(QRect(0, 0, 10, 10), 50).boundingRect(), QRect(0, 0, 10, 10));
    }

    // A Plasma dialog without its own background turns the blur off each
    // time it is shown or resized: the blur has to be asked for again
    // after that, or the panel stays see-through over sharp text. What is
    // checked is the call that reaches the compositor, not that apply()
    // ran: apply() can return before it asks for anything.
    void blurIsAskedAgainAfterTheWindowChanges()
    {
        blurCalls().clear();
        BlurBehind::setCompositor(
            [] {
                return true;
            },
            [](QWindow *w, bool on, const QRegion &r) {
                blurCalls().append({w, on, r});
            });
        auto restore = qScopeGuard([] {
            BlurBehind::setCompositor(nullptr, nullptr);
        });

        QQuickWindow window;
        window.resize(200, 100);
        QQuickItem item(window.contentItem());
        item.setSize(QSizeF(100, 50));
        BlurBehind blur;
        blur.setItem(&item);
        const QRegion shape = BlurBehind::roundedRegion(QRect(0, 0, 100, 50), blur.radius());
        auto lastIsBlurOn = [&] {
            const BlurCall &c = blurCalls().constLast();
            return c.window == &window && c.enable && c.region == shape;
        };
        QTRY_VERIFY(!blurCalls().isEmpty());
        QVERIFY(lastIsBlurOn());
        QVERIFY(blur.active());

        for (const QEvent::Type type : {QEvent::Resize, QEvent::Expose, QEvent::Show}) {
            const qsizetype before = blurCalls().size();
            if (type == QEvent::Resize) {
                QResizeEvent e(QSize(300, 150), QSize(200, 100));
                QCoreApplication::sendEvent(&window, &e);
            } else if (type == QEvent::Expose) {
                QExposeEvent e(QRegion(0, 0, 300, 150));
                QCoreApplication::sendEvent(&window, &e);
            } else {
                QShowEvent e;
                QCoreApplication::sendEvent(&window, &e);
            }
            QTRY_VERIFY2(blurCalls().size() > before, qPrintable(u"after event %1"_s.arg(int(type))));
            QVERIFY2(lastIsBlurOn(), qPrintable(u"after event %1"_s.arg(int(type))));
        }

        // a key press changes nothing about the window
        const qsizetype before = blurCalls().size();
        QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &key);
        QTest::qWait(50);
        QCOMPARE(blurCalls().size(), before);

        // turned off: the window is told, and the surface draws opaque
        blur.setEnabled(false);
        QTRY_VERIFY(!blurCalls().constLast().enable);
        QCOMPARE(blurCalls().constLast().window, &window);
        QVERIFY(!blur.active());
    }

    // Start and quick settings slide a parent of their acrylic, not the
    // acrylic itself: the blur follows it, or it stays where the slide began
    void blurFollowsASlidingParent()
    {
        blurCalls().clear();
        BlurBehind::setCompositor(
            [] {
                return true;
            },
            [](QWindow *w, bool on, const QRegion &r) {
                blurCalls().append({w, on, r});
            });
        auto restore = qScopeGuard([] {
            BlurBehind::setCompositor(nullptr, nullptr);
        });

        QQuickWindow window;
        window.resize(200, 200);
        QQuickItem slide(window.contentItem());
        slide.setY(40);
        QQuickItem surface(&slide);
        surface.setSize(QSizeF(100, 50));
        BlurBehind blur;
        blur.setItem(&surface);
        QTRY_VERIFY(!blurCalls().isEmpty());
        QCOMPARE(blurCalls().constLast().region.boundingRect(), QRect(0, 40, 100, 50));

        slide.setY(0);
        QTRY_COMPARE(blurCalls().constLast().region.boundingRect(), QRect(0, 0, 100, 50));
        QVERIFY(blurCalls().constLast().enable);
    }

    // The notification centre is two cards in one window, each with its own
    // acrylic. The compositor keeps one blur region per window, so each
    // card asking for its own shape threw the other's away: the upper card
    // was left see-through over sharp text. Both shapes go in one region.
    void twoSurfacesInOneWindowAreBothBlurred()
    {
        blurCalls().clear();
        BlurBehind::setCompositor(
            [] {
                return true;
            },
            [](QWindow *w, bool on, const QRegion &r) {
                blurCalls().append({w, on, r});
            });
        auto restore = qScopeGuard([] {
            BlurBehind::setCompositor(nullptr, nullptr);
        });

        QQuickWindow window;
        window.resize(200, 200);
        QQuickItem top(window.contentItem());
        top.setSize(QSizeF(100, 50));
        QQuickItem bottom(window.contentItem());
        bottom.setPosition(QPointF(0, 60));
        bottom.setSize(QSizeF(100, 80));
        BlurBehind topBlur;
        topBlur.setItem(&top);
        BlurBehind bottomBlur;
        bottomBlur.setItem(&bottom);
        const QRegion topShape = BlurBehind::roundedRegion(QRect(0, 0, 100, 50), topBlur.radius());
        const QRegion bottomShape = BlurBehind::roundedRegion(QRect(0, 60, 100, 80), bottomBlur.radius());

        QTRY_VERIFY(topBlur.active() && bottomBlur.active());
        QTest::qWait(20);
        QCOMPARE(blurCalls().constLast().window, &window);
        QVERIFY(blurCalls().constLast().enable);
        QCOMPARE(blurCalls().constLast().region, topShape + bottomShape);

        // the window shown again: still both
        QExposeEvent e(QRegion(0, 0, 200, 200));
        QCoreApplication::sendEvent(&window, &e);
        QTest::qWait(20);
        QCOMPARE(blurCalls().constLast().region, topShape + bottomShape);

        // one card hidden: only the other one is blurred, the window keeps its blur
        top.setVisible(false);
        QTRY_COMPARE(blurCalls().constLast().region, bottomShape);
        QVERIFY(blurCalls().constLast().enable);
        QVERIFY(!topBlur.active());
        QVERIFY(bottomBlur.active());

        // both gone: the blur is turned off for the window
        bottomBlur.setEnabled(false);
        QTRY_VERIFY(!blurCalls().constLast().enable);
        QCOMPARE(blurCalls().constLast().window, &window);
    }

    // A panel that slides moves its blur on the frame it is drawn in. Left
    // to the next turn of the event loop, the shape went out a frame late
    // and the frost trailed the panel by a few pixels at each step.
    void blurMovesOnTheFrameItIsDrawn()
    {
        blurCalls().clear();
        BlurBehind::setCompositor(
            [] {
                return true;
            },
            [](QWindow *w, bool on, const QRegion &r) {
                blurCalls().append({w, on, r});
            });
        auto restore = qScopeGuard([] {
            BlurBehind::setCompositor(nullptr, nullptr);
        });

        QQuickWindow window;
        window.resize(200, 200);
        QQuickItem slide(window.contentItem());
        slide.setY(40);
        QQuickItem surface(&slide);
        surface.setSize(QSizeF(100, 50));
        BlurBehind blur;
        blur.setItem(&surface);
        QTRY_VERIFY(blur.active());

        slide.setY(12);
        // what QQuickWindow does between moving the items and drawing them
        Q_EMIT window.afterAnimating();
        QCOMPARE(blurCalls().constLast().region.boundingRect(), QRect(0, 12, 100, 50));
    }

    // Nothing is sent again when the shape did not change: a slide used to
    // send the compositor a new region at every frame, also when two moves
    // cancelled out.
    void sameShapeIsNotSentTwice()
    {
        blurCalls().clear();
        BlurBehind::setCompositor(
            [] {
                return true;
            },
            [](QWindow *w, bool on, const QRegion &r) {
                blurCalls().append({w, on, r});
            });
        auto restore = qScopeGuard([] {
            BlurBehind::setCompositor(nullptr, nullptr);
        });

        QQuickWindow window;
        window.resize(200, 200);
        QQuickItem slide(window.contentItem());
        QQuickItem surface(&slide);
        surface.setSize(QSizeF(100, 50));
        BlurBehind blur;
        blur.setItem(&surface);
        QTRY_VERIFY(blur.active());
        QTest::qWait(20);
        const qsizetype before = blurCalls().size();

        slide.setY(10);
        surface.setY(-10);
        QTest::qWait(30);
        QCOMPARE(blurCalls().size(), before);

        slide.setY(20);
        QTRY_COMPARE(blurCalls().size(), before + 1);
    }

    // kdeglobals is read when it changes (KConfigWatcher), never on a move:
    // reading the file at every frame of a slide made menus stutter.
    void aMoveDoesNotReadTheConfigAgain()
    {
        blurCalls().clear();
        BlurBehind::setCompositor(
            [] {
                return true;
            },
            [](QWindow *w, bool on, const QRegion &r) {
                blurCalls().append({w, on, r});
            });
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        QDir().mkpath(dir);
        QFile globals(dir + u"/kdeglobals"_s);
        auto restore = qScopeGuard([&globals] {
            BlurBehind::setCompositor(nullptr, nullptr);
            globals.remove();
        });

        QQuickWindow window;
        window.resize(200, 200);
        QQuickItem slide(window.contentItem());
        QQuickItem surface(&slide);
        surface.setSize(QSizeF(100, 50));
        BlurBehind blur;
        blur.setItem(&surface);
        QTRY_VERIFY(blur.active());

        // written behind its back, with no change notice
        QVERIFY(globals.open(QIODevice::WriteOnly | QIODevice::Truncate));
        globals.write("[Raye]\nTransparencyEffects=false\n");
        globals.close();

        for (int y = 1; y <= 5; ++y) {
            slide.setY(y * 4);
            QTest::qWait(10);
        }
        QTRY_COMPARE(blurCalls().constLast().region.boundingRect(), QRect(0, 20, 100, 50));
        QVERIFY(blurCalls().constLast().enable);
        QVERIFY(blur.active());
    }

    // along the taskbar the second tooltip comes at once, then after a
    // pause the half second wait is back
    void nextTooltipComesQuickly()
    {
        QQmlEngine engine;
        engineSetup(engine);
        auto o = load(engine, "import QtQuick\nimport org.raye.shell\nWindow { width: 400; height: 300; visible: true;"
                              " property alias a: a; property alias b: b;"
                              " RayeTooltip { id: a; text: \"Start\" } RayeTooltip { id: b; text: \"Search\" } }");
        QVERIFY(o);
        auto *a = o->property("a").value<QObject *>();
        auto *b = o->property("b").value<QObject *>();
        QCOMPARE(a->property("delay").toInt(), 500);
        QElapsedTimer t;
        t.start();
        a->setProperty("visible", true);
        QTRY_VERIFY(a->property("visible").toBool());
        QVERIFY(t.elapsed() >= 400);
        a->setProperty("visible", false);
        QTRY_VERIFY(!a->property("visible").toBool());
        QCOMPARE(b->property("delay").toInt(), 100);
        t.restart();
        b->setProperty("visible", true);
        QTRY_VERIFY(b->property("visible").toBool());
        QVERIFY2(t.elapsed() < 300, qPrintable(QString::number(t.elapsed())));
        b->setProperty("visible", false);
        QTRY_VERIFY(!b->property("visible").toBool());
        QTRY_COMPARE_WITH_TIMEOUT(a->property("delay").toInt(), 500, 2000);
    }

    // "Start" came out as "Star" over "t": the tooltip's window is a whole
    // number of pixels and can be a hair narrower than the text measured.
    // A label that fits under the widest tooltip stays on one line.
    void aShortTooltipNeverWraps()
    {
        QQmlEngine engine;
        engineSetup(engine);
        auto o = load(engine, "import QtQuick\nimport org.raye.shell\nItem { property alias t: tip;"
                              " RayeTooltip { id: tip; text: \"Start\" } }");
        QVERIFY(o);
        auto *tip = o->property("t").value<QObject *>();
        QVERIFY(tip);
        auto *label = tip->property("contentItem").value<QQuickItem *>();
        QVERIFY(label);
        const qreal natural = tip->property("implicitWidth").toReal();
        QCOMPARE(natural, std::ceil(natural));
        // the window rounded down, then one pixel less again
        tip->setProperty("width", std::floor(natural) - 1);
        QCOMPARE(label->property("lineCount").toInt(), 1);
        // a long text still wraps under the widest tooltip
        const QMetaObject *meta = tip->metaObject();
        QVERIFY(meta->property(meta->indexOfProperty("width")).reset(tip));
        tip->setProperty("text", QString(u"word "_s.repeated(60)));
        QVERIFY(label->property("lineCount").toInt() > 1);
    }
};

QTEST_MAIN(TestComponents)
#include "tst_components.moc"
