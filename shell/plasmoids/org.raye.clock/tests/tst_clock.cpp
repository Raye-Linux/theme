// The clock: its decisions (clocklogic.js), the clock button on the
// taskbar (ClockButton.qml) and the notification centre (Centre.qml) on
// a stand-in for Plasma's notification model: groups, actions, jobs,
// "Clear all", Do not disturb, the calendar and the Focus strip.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../toastsettings.h"

#include <KConfigGroup>
#include <KLocalizedQmlContext>
#include <KSharedConfig>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QJSEngine>
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

using namespace Qt::StringLiterals;

class TestClock : public QObject
{
    Q_OBJECT

private:
    QJSEngine m_js;
    QJSEngine m_toast;
    QStringList m_warnings;
    static inline TestClock *s_self = nullptr;

    static void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
    {
        if (type != QtDebugMsg && type != QtInfoMsg && msg.contains(u"org.raye.clock"_s))
            s_self->m_warnings << msg;
    }

    QJSValue call(const char *name, const QJSValueList &args)
    {
        const QJSValue r = m_js.globalObject().property(QLatin1String(name)).call(args);
        if (r.isError())
            qWarning() << name << r.toString();
        return r;
    }

    QJSValue date(int y, int m, int d, int h = 0, int min = 0, int s = 0)
    {
        return m_js.toScriptValue(QDateTime(QDate(y, m, d), QTime(h, min, s)));
    }

    struct View {
        std::unique_ptr<QQmlEngine> engine;
        std::unique_ptr<QObject> fixture;
        QQuickWindow *window = nullptr;
        QQuickItem *item = nullptr;
    };

    std::unique_ptr<View> view(const QString &what, const QVariantMap &toast = {}, const QVariantMap &extra = {})
    {
        auto v = std::make_unique<View>();
        v->engine = std::make_unique<QQmlEngine>();
        v->engine->addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        v->engine->addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        KLocalization::setupLocalizedContext(v->engine.get());
        QQmlComponent c(v->engine.get(), QUrl::fromLocalFile(QStringLiteral(RAYE_CLOCK_TESTS "/fixture.qml")));
        QObject *o = c.beginCreate(v->engine->rootContext());
        if (!o) {
            qWarning() << c.errors();
            return nullptr;
        }
        o->setProperty("uiDir", QUrl::fromLocalFile(QStringLiteral(RAYE_CLOCK_DIR "/contents/ui/")));
        o->setProperty("what", what);
        o->setProperty("toast", toast);
        for (auto it = extra.cbegin(); it != extra.cend(); ++it)
            o->setProperty(it.key().toUtf8().constData(), it.value());
        c.completeCreate();
        v->fixture.reset(o);
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

    // by the visual tree: items made by a ListView or a Repeater are not
    // QObject children of anything findChild() reaches
    static void collectItems(QQuickItem *root, const QString &name, QList<QQuickItem *> *out)
    {
        const auto kids = root->childItems();
        for (QQuickItem *kid : kids) {
            if (kid->objectName() == name)
                out->append(kid);
            collectItems(kid, name, out);
        }
    }

    static QQuickItem *child(QQuickItem *root, const QString &name)
    {
        QList<QQuickItem *> found;
        collectItems(root, name, &found);
        return found.isEmpty() ? root->findChild<QQuickItem *>(name) : found.first();
    }

    QStringList calls(View *v)
    {
        return v->fixture->property("calls").toStringList();
    }

    void click(View *v, QQuickItem *item)
    {
        QVERIFY(item);
        QTest::mouseClick(v->window, Qt::LeftButton, {}, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
    }

    QStringList texts(QQuickItem *root, const QString &name)
    {
        QStringList out;
        QList<QQuickItem *> items;
        collectItems(root, name, &items);
        for (QQuickItem *i : items) {
            if (i->isVisible())
                out << i->property("text").toString();
        }
        return out;
    }

private Q_SLOTS:
    void initTestCase()
    {
        s_self = this;
        QFile f(QStringLiteral(RAYE_CLOCK_DIR "/contents/ui/clocklogic.js"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJSValue r = m_js.evaluate(QString::fromUtf8(f.readAll()).remove(u".pragma library"_s));
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
        QFile t(QStringLiteral(RAYE_CLOCK_DIR "/contents/ui/toasts/toastlogic.js"));
        QVERIFY(t.open(QIODevice::ReadOnly));
        const QJSValue rt = m_toast.evaluate(QString::fromUtf8(t.readAll()).remove(u".pragma library"_s));
        QVERIFY2(!rt.isError(), qPrintable(rt.toString()));
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
        QDirIterator it(QStringLiteral(RAYE_CLOCK_DIR "/contents/ui"), {u"*.qml"_s}, QDir::Files, QDirIterator::Subdirectories);
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
        QFile main(QStringLiteral(RAYE_CLOCK_DIR "/contents/ui/main.qml"));
        QVERIFY(main.open(QIODevice::ReadOnly));
        const QString code = QString::fromUtf8(main.readAll());
        QVERIFY2(code.contains(u"fullRepresentation:"_s), "main.qml needs a fullRepresentation");
        // the holder and the Loader are focus scopes: without focus on both
        // the centre never gets the keys of its window, and Esc does nothing
        static const QRegularExpression loaderFocus(
            u"mainItem:\\s*FocusScope\\s*\\{[^{}]*\\bfocus:\\s*true[^{}]*Loader\\s*\\{[^{}]*\\bfocus:\\s*true"_s);
        QVERIFY2(loaderFocus.match(code).hasMatch(), "the centre's holder and Loader need focus: true");
        // the history list goes through historyBlacklist(), not the raw setting
        QVERIFY2(code.contains(u"Logic.historyBlacklist(notificationSettings.historyBlacklistedApplications)"_s),
                 "the centre keeps apps without a desktop file");
    }

    // --- clocklogic.js ----------------------------------------------------

    // Plasma's default keeps every app without a desktop file ("@other")
    // out of the history, so a notification that ended on its own was gone
    // when the centre opened. Windows lists them all; an app the user took
    // out stays out.
    void historyKeepsAppsWithoutADesktopFile()
    {
        QJSValue in = m_js.newArray(3);
        in.setProperty(0, u"vlc"_s);
        in.setProperty(1, u"@other"_s);
        in.setProperty(2, u"org.kde.klipper"_s);
        const QJSValue out = call("historyBlacklist", {in});
        QCOMPARE(out.property(u"length"_s).toInt(), 2);
        QCOMPARE(out.property(0).toString(), u"vlc"_s);
        QCOMPARE(out.property(1).toString(), u"org.kde.klipper"_s);
        QCOMPARE(call("historyBlacklist", {QJSValue()}).property(u"length"_s).toInt(), 0);
    }

    void twentyFour()
    {
        QVERIFY(call("use24h", {u"auto"_s, u"HH:mm"_s}).toBool());
        QVERIFY(!call("use24h", {u"auto"_s, u"h:mm AP"_s}).toBool());
        QVERIFY(!call("use24h", {u"auto"_s, u"h:mm ap"_s}).toBool());
        // a quoted literal is not the AM/PM marker
        QVERIFY(call("use24h", {u"auto"_s, u"HH 'h' mm"_s}).toBool());
        QVERIFY(call("use24h", {u"true"_s, u"h:mm AP"_s}).toBool());
        QVERIFY(!call("use24h", {u"false"_s, u"HH:mm"_s}).toBool());
        QCOMPARE(call("timeFormat", {true, false}).toString(), u"HH:mm"_s);
        QCOMPARE(call("timeFormat", {true, true}).toString(), u"HH:mm:ss"_s);
        QCOMPARE(call("timeFormat", {false, false}).toString(), u"h:mm AP"_s);
        QCOMPARE(call("timeFormat", {false, true}).toString(), u"h:mm:ss AP"_s);
        // the clock's text, as Windows writes it: "9:24 PM" (never the
        // locale's "p.m."), "21:24", "09:05" stays "9:05 AM"
        QCOMPARE(call("timeText", {21, 24, 7, false, false}).toString(), u"9:24 PM"_s);
        QCOMPARE(call("timeText", {9, 5, 7, false, false}).toString(), u"9:05 AM"_s);
        QCOMPARE(call("timeText", {0, 0, 0, false, false}).toString(), u"12:00 AM"_s);
        QCOMPARE(call("timeText", {12, 30, 0, false, true}).toString(), u"12:30:00 PM"_s);
        QCOMPARE(call("timeText", {21, 24, 7, true, false}).toString(), u"21:24"_s);
        QCOMPARE(call("timeText", {7, 4, 9, true, true}).toString(), u"07:04:09"_s);
    }

    // The room between the time and the date. They used to sit flush --
    // the Column set no spacing, two 15 px lines, 30 px of stack -- which
    // is what "the date is too close to the time" was. The gap gives way
    // before the stack outgrows the 40 px button, so a larger type size
    // (or a script with taller fallback metrics) cannot make the clock
    // overflow the taskbar. tests/shell/test_clock_lines.py measures the
    // same thing against the real font, in nine locales.
    void lineGap()
    {
        QCOMPARE(call("lineGap", {15, 15, 40}).toInt(), 3);
        QCOMPARE(call("lineGap", {19, 19, 40}).toInt(), 2);
        QCOMPARE(call("lineGap", {30, 30, 40}).toInt(), 0);
        QCOMPARE(call("lineGap", {100, 100, 40}).toInt(), 0);
        // nothing measured yet: keep the designed gap, do not flash 0
        QCOMPARE(call("lineGap", {0, 0, 40}).toInt(), 3);
        QCOMPARE(call("lineGap", {15, 15, 0}).toInt(), 3);
        // the date hidden: one line, the whole gap is still available
        QCOMPARE(call("lineGap", {15, 0, 40}).toInt(), 3);
        // a caller's own gap, and a nonsense one falls back on the design
        QCOMPARE(call("lineGap", {15, 15, 40, 6}).toInt(), 6);
        QCOMPARE(call("lineGap", {15, 15, 40, -4}).toInt(), 3);
    }

    void ticks()
    {
        QCOMPARE(call("nextTick", {date(2026, 11, 1, 14, 5, 20), false}).toInt(), 40000);
        QCOMPARE(call("nextTick", {date(2026, 11, 1, 14, 5, 20), true}).toInt(), 1000);
    }

    void counts()
    {
        QCOMPARE(call("countText", {0}).toString(), QString());
        QCOMPARE(call("countText", {3}).toString(), u"3"_s);
        QCOMPARE(call("countText", {9}).toString(), u"9"_s);
        QCOMPARE(call("countText", {10}).toString(), u"9+"_s);
    }

    void ages()
    {
        const QJSValue now = date(2026, 11, 1, 14, 5, 0);
        auto kind = [&](QJSValue created) { return call("age", {created, now}).property(u"kind"_s).toString(); };
        QCOMPARE(kind(date(2026, 11, 1, 14, 4, 30)), u"now"_s);
        QJSValue mins = call("age", {date(2026, 11, 1, 13, 50, 0), now});
        QCOMPARE(mins.property(u"kind"_s).toString(), u"minutes"_s);
        QCOMPARE(mins.property(u"value"_s).toInt(), 15);
        QCOMPARE(kind(date(2026, 11, 1, 8, 0, 0)), u"today"_s);
        QCOMPARE(kind(date(2026, 10, 31, 23, 0, 0)), u"yesterday"_s);
        QCOMPARE(kind(date(2026, 10, 20, 9, 0, 0)), u"date"_s);
        QCOMPARE(kind(QJSValue()), u"now"_s);
    }

    void dnd()
    {
        const QJSValue now = date(2026, 11, 1, 14, 0, 0);
        QVERIFY(call("dndActive", {date(2026, 11, 1, 15, 0, 0), now, false}).toBool());
        QVERIFY(!call("dndActive", {date(2026, 11, 1, 13, 0, 0), now, false}).toBool());
        QVERIFY(!call("dndActive", {QJSValue(), now, false}).toBool());
        QVERIFY(call("dndActive", {QJSValue(), now, true}).toBool());
    }

    void monthGrid()
    {
        // November 2026 starts on a Sunday
        QJSValue sunday = call("monthGrid", {2026, 10, 0});
        QCOMPARE(sunday.property(u"length"_s).toInt(), 42);
        QCOMPARE(sunday.property(0).property(u"day"_s).toInt(), 1);
        QVERIFY(sunday.property(0).property(u"inMonth"_s).toBool());
        QCOMPARE(sunday.property(29).property(u"day"_s).toInt(), 30);
        QCOMPARE(sunday.property(30).property(u"day"_s).toInt(), 1);
        QVERIFY(!sunday.property(30).property(u"inMonth"_s).toBool());
        QCOMPARE(sunday.property(30).property(u"month"_s).toInt(), 11);
        // weeks starting on Monday: six days of October first
        QJSValue monday = call("monthGrid", {2026, 10, 1});
        QCOMPARE(monday.property(0).property(u"day"_s).toInt(), 26);
        QCOMPARE(monday.property(0).property(u"month"_s).toInt(), 9);
        QCOMPARE(monday.property(6).property(u"day"_s).toInt(), 1);
        // Qt may give Sunday as 7
        QCOMPARE(call("monthGrid", {2026, 10, 7}).property(0).property(u"day"_s).toInt(), 1);
        // January: December of the year before
        QJSValue jan = call("monthGrid", {2027, 0, 1});
        QCOMPARE(jan.property(0).property(u"year"_s).toInt(), 2026);
        QCOMPARE(jan.property(0).property(u"month"_s).toInt(), 11);

        QJSValue days = call("weekdays", {1});
        QCOMPARE(days.property(0).toInt(), 1);
        QCOMPARE(days.property(6).toInt(), 0);
        QJSValue next = call("addMonths", {2026, 11, 1});
        QCOMPARE(next.property(u"year"_s).toInt(), 2027);
        QCOMPARE(next.property(u"month"_s).toInt(), 0);
        QJSValue back = call("addMonths", {2027, 0, -1});
        QCOMPARE(back.property(u"year"_s).toInt(), 2026);
        QCOMPARE(back.property(u"month"_s).toInt(), 11);
    }

    void agenda()
    {
        const QString json = u"[{\"title\":\"Dentist\",\"start\":\"2026-11-01T09:30:00\",\"end\":\"2026-11-01T10:30:00\",\"allDay\":false,\"color\":\"#2f80ed\"},"
                             "{\"title\":\"Trip\",\"start\":\"2026-10-31T00:00:00\",\"end\":\"2026-11-03T00:00:00\",\"allDay\":true},"
                             "{\"title\":\"broken\",\"start\":\"never\"}]"_s;
        QJSValue events = call("parseAgenda", {json});
        QCOMPARE(events.property(u"length"_s).toInt(), 2);
        // the trip runs over the 1st: it is listed first, all day
        QJSValue first = call("eventsOn", {events, 2026, 10, 1});
        QCOMPARE(first.property(u"length"_s).toInt(), 2);
        QCOMPARE(first.property(0).property(u"title"_s).toString(), u"Trip"_s);
        QCOMPARE(first.property(1).property(u"title"_s).toString(), u"Dentist"_s);
        QCOMPARE(call("eventsOn", {events, 2026, 10, 2}).property(u"length"_s).toInt(), 1);
        // the end of an all-day event is the next midnight, not one more day
        QCOMPARE(call("eventsOn", {events, 2026, 10, 3}).property(u"length"_s).toInt(), 0);
        QCOMPARE(call("parseAgenda", {u"not json"_s}).property(u"length"_s).toInt(), 0);
        QCOMPARE(call("isoDay", {2026, 0, 5}).toString(), u"2026-01-05"_s);
    }

    void focus()
    {
        QCOMPARE(call("focusStep", {30, true}).toInt(), 45);
        QCOMPARE(call("focusStep", {25, true}).toInt(), 30);
        QCOMPARE(call("focusStep", {60, true}).toInt(), 90);
        QCOMPARE(call("focusStep", {240, true}).toInt(), 240);
        QCOMPARE(call("focusStep", {30, false}).toInt(), 25);
        QCOMPARE(call("focusStep", {45, false}).toInt(), 30);
        QCOMPARE(call("focusStep", {90, false}).toInt(), 60);
        QCOMPARE(call("focusStep", {5, false}).toInt(), 5);
        // up then down comes back
        for (int m : {5, 10, 25, 30, 45, 60, 90, 180})
            QCOMPARE(call("focusStep", {call("focusStep", {m, true}), false}).toInt(), m);

        const QJSValue now = date(2026, 11, 1, 14, 0, 0);
        QCOMPARE(call("focusRemaining", {date(2026, 11, 1, 14, 25, 0), now}).toInt(), 1500);
        QCOMPARE(call("focusRemaining", {date(2026, 11, 1, 13, 0, 0), now}).toInt(), 0);
        QCOMPARE(call("focusRemaining", {QJSValue(), now}).toInt(), 0);
        QCOMPARE(call("focusFraction", {900, 30}).toNumber(), 0.5);
        QCOMPARE(call("focusFraction", {0, 30}).toNumber(), -1.0);
        QCOMPARE(call("focusText", {1500}).toString(), u"25:00"_s);
        QCOMPARE(call("focusText", {3905}).toString(), u"1:05:05"_s);
        QCOMPARE(call("focusText", {59}).toString(), u"0:59"_s);
    }

    // --- ClockButton.qml --------------------------------------------------

    void clockButton()
    {
        auto v = view(u"clock"_s);
        QVERIFY(v);
        QCOMPARE(child(v->item, u"time"_s)->property("text").toString(), u"14:05"_s);
        QVERIFY(child(v->item, u"date"_s)->isVisible());
        QVERIFY(child(v->item, u"bell"_s)->isVisible());
        QVERIFY(!child(v->item, u"count"_s)->isVisible());
        QVERIFY(!child(v->item, u"focusRing"_s)->isVisible());

        v->item->setProperty("showSeconds", true);
        QCOMPARE(child(v->item, u"time"_s)->property("text").toString(), u"14:05:09"_s);
        v->item->setProperty("twentyFour", false);
        QCOMPARE(child(v->item, u"time"_s)->property("text").toString(), u"2:05:09 PM"_s);
        v->item->setProperty("showDate", false);
        QVERIFY(!child(v->item, u"date"_s)->isVisible());

        v->item->setProperty("unreadCount", 12);
        QVERIFY(child(v->item, u"count"_s)->isVisible());
        QVERIFY(!child(v->item, u"bell"_s)->isVisible());
        QCOMPARE(v->item->property("countText").toString(), u"9+"_s);

        v->item->setProperty("unreadCount", 0);
        v->item->setProperty("doNotDisturb", true);
        QVERIFY(child(v->item, u"bell"_s)->property("sleeping").toBool());

        v->item->setProperty("focusFraction", 0.5);
        QVERIFY(child(v->item, u"focusRing"_s)->isVisible());

        click(v.get(), v->item);
        QCOMPARE(calls(v.get()), QStringList{u"clicked"_s});

        // the screen's bottom edge, under the button, is the clock too
        const QPoint below = v->item->mapToScene(QPointF(v->item->width() / 2, 46)).toPoint();
        QTest::mouseClick(v->window, Qt::LeftButton, {}, below);
        QCOMPARE(calls(v.get()).size(), 1);
        v->item->setProperty("reachBottom", 48);
        QTest::mouseClick(v->window, Qt::LeftButton, {}, below);
        QCOMPARE(calls(v.get()), (QStringList{u"clicked"_s, u"clicked"_s}));
    }

    // --- Centre.qml -------------------------------------------------------

    void centreEmpty()
    {
        auto v = view(u"centreEmpty"_s);
        QVERIFY(v);
        QVERIFY(child(v->item, u"empty"_s)->isVisible());
        QVERIFY(!child(v->item, u"clearAll"_s)->isVisible());
        QCOMPARE(v->item->width(), 364.0 + 24);
    }

    void centreNotifications()
    {
        auto v = view(u"centre"_s);
        QVERIFY(v);
        QVERIFY(!child(v->item, u"empty"_s)->isVisible());
        QTRY_VERIFY(texts(v->item, u"summary"_s).contains(u"New message"_s));
        // markup is shown as text, never read
        QVERIFY(texts(v->item, u"body"_s).contains(u"Lunch at noon? <script>"_s));
        QVERIFY(texts(v->item, u"age"_s).contains(u"5m"_s));

        click(v.get(), child(v->item, u"clearAll"_s));
        click(v.get(), child(v->item, u"dndButton"_s));
        QStringList asked = calls(v.get());
        // the month's own question to the calendar is not a click
        asked.removeIf([](const QString &c) { return c.startsWith(u"agenda "_s); });
        QCOMPARE(asked, (QStringList{u"clearAll"_s, u"toggleDnd"_s}));
    }

    void calendarAndFocus()
    {
        auto v = view(u"centre"_s);
        QVERIFY(v);
        QQuickItem *today = child(v->item, u"today"_s);
        QVERIFY(today);
        QCOMPARE(today->property("text").toString(), u"1"_s);
        QCOMPARE(child(v->item, u"focusLength"_s)->property("text").toString(), u"30 mins"_s);
        click(v.get(), child(v->item, u"focusMore"_s));
        QCOMPARE(child(v->item, u"focusLength"_s)->property("text").toString(), u"45 mins"_s);
        click(v.get(), child(v->item, u"focusButton"_s));
        QCOMPARE(calls(v.get()).last(), u"startFocus 45"_s);
        // a session runs: what is left, and "End session"
        v->item->setProperty("focusRemaining", 600);
        QCOMPARE(child(v->item, u"focusButton"_s)->property("text").toString(), u"End session"_s);
        click(v.get(), child(v->item, u"focusButton"_s));
        QCOMPARE(calls(v.get()).last(), u"stopFocus"_s);
        // alarms are in the Clock app, one click away
        QQuickItem *alarms = child(v->item, u"clockLink"_s);
        QVERIFY(alarms && alarms->isVisible());
        QCOMPARE(alarms->property("toolTipText").toString(), u"Alarms & clock"_s);
        click(v.get(), alarms);
        QCOMPARE(calls(v.get()).last(), u"openClock"_s);

        // the months move; the title follows
        const QString title = child(v->item, u"monthTitle"_s)->property("text").toString();
        click(v.get(), child(v->item, u"nextMonth"_s));
        QVERIFY(child(v->item, u"monthTitle"_s)->property("text").toString() != title);
        QVERIFY(!child(v->item, u"today"_s));

        // folding the calendar
        click(v.get(), child(v->item, u"collapse"_s));
        QCOMPARE(calls(v.get()).last(), u"collapse"_s);
    }

    void calendarEvents()
    {
        auto v = view(u"centre"_s);
        QVERIFY(v);
        // the month asks for its 42 days as soon as it shows
        QTRY_VERIFY(calls(v.get()).contains(u"agenda 2026-10-26 2026-12-13"_s));
        // today, the 1st, has two events; a dot marks the 2nd
        QTRY_COMPARE(texts(v->item, u"agendaTitle"_s), (QStringList{u"Trip"_s, u"Dentist"_s}));
        QVERIFY(child(v->item, u"dot-2026-11-02"_s));
        QVERIFY(!child(v->item, u"dot-2026-11-05"_s));
        click(v.get(), child(v->item, u"addEvent"_s));
        QCOMPARE(calls(v.get()).last(), u"open 2026-11-01 new"_s);
        // another day: its own list, and "No events"
        click(v.get(), child(v->item, u"day-2026-11-05"_s));
        QTRY_VERIFY(texts(v->item, u"agendaTitle"_s).isEmpty());
        QVERIFY(child(v->item, u"noEvents"_s)->isVisible());
        click(v.get(), child(v->item, u"addEvent"_s));
        QCOMPARE(calls(v.get()).last(), u"open 2026-11-05 new"_s);
    }

    void calendarAbsent()
    {
        // without Raye Calendar the card is the month alone
        auto v = view(u"centreEmpty"_s);
        QVERIFY(v);
        QVERIFY(!child(v->item, u"addEvent"_s) || !child(v->item, u"addEvent"_s)->isVisible());
    }

    // The day's events go first, then the month folds; the card is never cut.
    void calendarFit()
    {
        auto fit = [this](int room, int agenda, int wanted) {
            QJSValue p = m_js.newObject();
            p.setProperty(u"header"_s, 52);
            p.setProperty(u"month"_s, 312);
            p.setProperty(u"agenda"_s, agenda);
            p.setProperty(u"focus"_s, 56);
            p.setProperty(u"gap"_s, 12);
            p.setProperty(u"notificationsMin"_s, 124);
            p.setProperty(u"notificationsWanted"_s, wanted);
            const QJSValue r = call("calendarFit", {room, p});
            return QStringLiteral("%1 %2").arg(r.property(u"month"_s).toBool() ? u"month"_s : u"-"_s,
                                               r.property(u"agenda"_s).toBool() ? u"agenda"_s : u"-"_s);
        };
        QCOMPARE(fit(1011, 152, 2000), u"month agenda"_s);
        QCOMPARE(fit(708, 152, 124), u"month agenda"_s);
        QCOMPARE(fit(696, 152, 124), u"month -"_s);
        // one notification on 800 px: it shows whole, the events make way
        QCOMPARE(fit(728, 152, 156), u"month -"_s);
        QCOMPARE(fit(648, 0, 2000), u"month -"_s);
        QCOMPARE(fit(556, 152, 2000), u"month -"_s);
        QCOMPARE(fit(555, 152, 124), u"- -"_s);
    }

    void centreRoom()
    {
        auto rect = [this](int x, int y, int w, int h) {
            QJSValue r = m_js.newObject();
            r.setProperty(u"x"_s, x);
            r.setProperty(u"y"_s, y);
            r.setProperty(u"width"_s, w);
            r.setProperty(u"height"_s, h);
            return r;
        };
        // 1366 x 768, taskbar at 720
        QCOMPARE(call("centreRoom", {rect(0, 0, 1366, 768), 720, QJSValue(), 12}).toInt(), 696);
        // a 1080 screen to the right of it: its own top and taskbar
        QCOMPARE(call("centreRoom", {rect(1366, 0, 1920, 1080), 1032, rect(1366, 0, 1920, 1032), 12}).toInt(), 1008);
        // a free area smaller than the screen (a panel at the top)
        QCOMPARE(call("centreRoom", {rect(0, 0, 1920, 1080), 1032, rect(0, 40, 1920, 992), 12}).toInt(), 968);
        // an empty area is no information
        QCOMPARE(call("centreRoom", {rect(0, 0, 1280, 720), 672, rect(0, 0, 0, 0), 12}).toInt(), 648);
    }

    // What the owner saw on a 1366 x 768 laptop: the calendar cut at the
    // bottom. Whatever the screen and the count, the centre fits in its
    // room, the calendar card is whole and the list scrolls.
    void centreFitsTheScreen_data()
    {
        QTest::addColumn<int>("screen");
        QTest::addColumn<int>("count");
        QTest::addColumn<bool>("calendar");
        for (int screen : {720, 768, 800, 1080}) {
            for (int count : {0, 1, 5, 50}) {
                for (bool calendar : {false, true})
                    QTest::addRow("%d %d %s", screen, count, calendar ? "agenda" : "month") << screen << count << calendar;
            }
        }
    }

    void centreFitsTheScreen()
    {
        QFETCH(int, screen);
        QFETCH(int, count);
        QFETCH(bool, calendar);
        // what main.qml gives: above a 48 px taskbar, 12 px from each edge
        const int room = screen - 48 - 24;
        auto v = view(u"centre"_s, {}, {{u"notificationCount"_s, count}, {u"room"_s, room}, {u"withCalendar"_s, calendar}});
        QVERIFY(v);
        auto *cal = v->item->property("calendarCard").value<QQuickItem *>();
        auto *notes = v->item->property("notificationsCard").value<QQuickItem *>();
        QVERIFY(cal && notes);
        // the height animation of the calendar card is over
        QTRY_COMPARE(cal->height(), cal->implicitHeight());
        QTRY_VERIFY(qAbs(cal->implicitHeight() - cal->property("targetHeight").toReal()) < 0.5);
        const qreal margin = v->item->property("margin").toReal();
        QVERIFY2(v->item->height() <= room + 2 * margin + 0.5,
                 qPrintable(QStringLiteral("centre %1 in %2").arg(v->item->height()).arg(room + 2 * margin)));
        QVERIFY(cal->mapToItem(v->item, QPointF(0, cal->height())).y() <= v->item->height() - margin + 0.5);
        // the month stays on every screen a laptop has
        QVERIFY(child(v->item, u"today"_s));
        QVERIFY(child(v->item, u"today"_s)->isVisible());
        QQuickItem *list = child(v->item, u"notificationList"_s);
        QVERIFY(list);
        if (count == 50) {
            QVERIFY(list->property("contentHeight").toReal() > list->height());
            QVERIFY(list->property("interactive").toBool());
        }
        // no empty space: the card is as tall as what it holds
        if (count > 0 && count < 5 && screen >= 768)
            QCOMPARE(list->height(), list->property("contentHeight").toReal());
    }

    // --- toasts ------------------------------------------------------------

    QString toastJs(const QString &code)
    {
        const QJSValue r = m_toast.evaluate(code);
        return r.isError() ? u"ERROR "_s + r.toString() : r.toString();
    }

    void toastTime()
    {
        const QString base = u"{critical:false,job:false,jobRunning:false,permanentJobs:true,seconds:5,timeout:-1,urlCount:0"_s;
        const auto ms = [&](const QString &extra) { return toastJs(u"timeoutMs("_s + base + extra + u"})"_s); };
        QCOMPARE(ms(u""_s), u"5000"_s);
        QCOMPARE(ms(u",seconds:30"_s), u"30000"_s);        // the setting
        QCOMPARE(ms(u",timeout:0"_s), u"0"_s);             // the sender: forever
        QCOMPARE(ms(u",timeout:2500"_s), u"2500"_s);       // the sender's own
        QCOMPARE(ms(u",critical:true"_s), u"0"_s);         // critical stays
        QCOMPARE(ms(u",urlCount:1"_s), u"10000"_s);        // a file to look at
        QCOMPARE(ms(u",job:true,jobRunning:true"_s), u"0"_s);
        QCOMPARE(ms(u",job:true,jobRunning:true,permanentJobs:false"_s), u"5000"_s);
        QCOMPARE(ms(u",job:true,jobRunning:false"_s), u"5000"_s);

        QCOMPARE(toastJs(u"whenTimeIsUp({job:false,resident:false,hasActions:false,transient:false})"_s), u"expire"_s);
        QCOMPARE(toastJs(u"whenTimeIsUp({job:false,resident:true,hasActions:false,transient:false})"_s), u"hide"_s);
        QCOMPARE(toastJs(u"whenTimeIsUp({job:false,resident:false,hasActions:true,transient:false})"_s), u"hide"_s);
        QCOMPARE(toastJs(u"whenTimeIsUp({job:false,resident:false,hasActions:true,transient:true})"_s), u"expire"_s);
        QCOMPARE(toastJs(u"whenTimeIsUp({job:true,jobRunning:true})"_s), u"dismiss"_s);
    }

    void toastInhibition()
    {
        const QString off = u"serverValid:true,until:new Date(NaN),now:new Date(2026,1,1),byApplication:false,whenFullscreen:true,fullscreenFocused:false,whenMirrored:true,screensMirrored:false"_s;
        const auto is = [&](const QString &extra) { return toastJs(u"inhibited({"_s + off + extra + u"})"_s); };
        QCOMPARE(is(u""_s), u"false"_s);
        QCOMPARE(is(u",until:new Date(2026,1,2)"_s), u"true"_s);
        QCOMPARE(is(u",until:new Date(2026,0,1)"_s), u"false"_s);
        QCOMPARE(is(u",byApplication:true"_s), u"true"_s);
        QCOMPARE(is(u",fullscreenFocused:true"_s), u"true"_s);
        QCOMPARE(is(u",screensMirrored:true"_s), u"true"_s);
        QCOMPARE(is(u",whenMirrored:false,screensMirrored:true"_s), u"false"_s);
        QCOMPARE(is(u",serverValid:false,byApplication:true"_s), u"false"_s);

        const QString flags = u"critical:4,normal:2,low:1"_s;
        QCOMPARE(toastJs(u"urgencies({inhibited:false,criticalInDnd:false,lowPopups:false,"_s + flags + u"})"_s), u"6"_s);
        QCOMPARE(toastJs(u"urgencies({inhibited:false,criticalInDnd:false,lowPopups:true,"_s + flags + u"})"_s), u"7"_s);
        QCOMPARE(toastJs(u"urgencies({inhibited:true,criticalInDnd:true,lowPopups:true,"_s + flags + u"})"_s), u"4"_s);
        QCOMPARE(toastJs(u"urgencies({inhibited:true,criticalInDnd:false,lowPopups:true,"_s + flags + u"})"_s), u"0"_s);
    }

    void toastStack()
    {
        // windows 100 and 60 tall with margins of 8: cards 84 and 44, 12 apart,
        // the first ending at 700
        QCOMPARE(toastJs(u"var s = stack([100, 60], 700, 8, 12, 500); s[0].y + ' ' + s[1].y + ' ' + s[0].visible + ' ' + s[1].visible"_s),
                 u"608 552 true true"_s);
        // no more than the room, except the newest
        QCOMPARE(toastJs(u"var s = stack([400, 400], 700, 8, 12, 300); s[0].visible + ' ' + s[1].visible"_s), u"true false"_s);
        QCOMPARE(toastJs(u"MAX_VISIBLE"_s), u"3"_s);
    }

    // No part of the desktop's name reaches the screen, whichever way a
    // sender is filed.
    void toastSources()
    {
        const auto src = [&](const QString &args) {
            return toastJs(u"var s = source("_s + args + u"); s.key + '|' + s.name + '|' + s.icon"_s);
        };
        // the photo of the owner's: an error of the desktop's own
        QCOMPARE(src(u"'Plasma Desktop Workspace','plasmashell','org.kde.plasmashell','plasma_workspace',false"_s),
                 u"system||raye-eye"_s);
        QCOMPARE(src(u"'System Notifications','start-here-kde-plasma','','plasma_workspace',false"_s), u"system||raye-eye"_s);
        // and a file copy started from the desktop
        QCOMPARE(src(u"'Plasma Desktop Workspace','plasmashell','org.kde.plasmashell','',true"_s),
                 u"files||system-file-manager"_s);
        QCOMPARE(src(u"'Network Management','network-wired','','networkmanagement',false"_s), u"network||network-wired"_s);
        QCOMPARE(src(u"'KDE Daemon','','','',false"_s), u"system||raye-eye"_s);
        QCOMPARE(src(u"'Plasma','','','',false"_s), u"system||raye-eye"_s);
        // an app stays itself, also when Plasma files it under the default sender
        QCOMPARE(src(u"'Files','system-file-manager','org.kde.dolphin','',false"_s), u"|Files|system-file-manager"_s);
        QCOMPARE(src(u"'Firefox','firefox','firefox','plasma_workspace',false"_s), u"|Firefox|firefox"_s);
        QCOMPARE(src(u"'Chat','','','',false"_s), u"|Chat|preferences-desktop-notification"_s);
        // words that only look like it
        QCOMPARE(src(u"'Plasmatic','x','plasmatic','',false"_s), u"|Plasmatic|x"_s);
        // the logo on its own
        QCOMPARE(src(u"'Chat','start-here-kde','','',false"_s), u"|Chat|raye-eye"_s);

        QCOMPARE(toastJs(u"title('System Notifications')"_s), QString());
        QCOMPARE(toastJs(u"title('Notifications du système')"_s), QString());
        QCOMPARE(toastJs(u"title('Plasma Desktop Workspace')"_s), QString());
        QCOMPARE(toastJs(u"title('Plasma Shell')"_s), QString());
        QCOMPARE(toastJs(u"title('Lunch <b>now</b>')"_s), u"Lunch now"_s);

        QCOMPARE(toastJs(u"JSON.stringify(networkEvent('networkmanagement', \"Connection 'Wired connection 1' activated.\"))"_s),
                 u"{\"kind\":\"connected\",\"name\":\"Ethernet\"}"_s);
        QCOMPARE(toastJs(u"JSON.stringify(networkEvent('networkmanagement', 'Connexion « Maison » désactivée.'))"_s),
                 u"{\"kind\":\"disconnected\",\"name\":\"Maison\"}"_s);
        QCOMPARE(toastJs(u"networkEvent('other', \"Connection 'a' activated.\")"_s), u"null"_s);
        QCOMPARE(toastJs(u"NEVER_SHOWN.indexOf('donationmessage') >= 0"_s), u"true"_s);

        QCOMPARE(toastJs(u"themeIcon('firefox')"_s), u"firefox"_s);
        QCOMPARE(toastJs(u"themeIcon('/home/a/me.png')"_s), QString());
        QCOMPARE(toastJs(u"pictureFile('/home/a/me.png')"_s), u"/home/a/me.png"_s);
        QCOMPARE(toastJs(u"pictureFile('firefox')"_s), QString());

        QCOMPARE(toastJs(u"thumbnail(['file:///a/shot.PNG'])"_s), u"file:///a/shot.PNG"_s);
        QCOMPARE(toastJs(u"thumbnail(['https://evil.example/a.png'])"_s), QString());
        QCOMPARE(toastJs(u"thumbnail(['file:///a.png','file:///b.png'])"_s), QString());
    }

    void toastOwner()
    {
        QCOMPARE(RayeToasts::ownerOf({}), -1);
        QCOMPARE(RayeToasts::ownerOf({1}), 0);
        QCOMPARE(RayeToasts::ownerOf({1, 0}), 1);
        QCOMPARE(RayeToasts::ownerOf({-1, 2}), 1);
        QCOMPARE(RayeToasts::ownerOf({-1, -1}), 0);
    }

    void toastTurnOff()
    {
        const QString file = QDir::temp().filePath(u"raye-toast-notifyrc-%1"_s.arg(QCoreApplication::applicationPid()));
        QFile::remove(file);
        KSharedConfigPtr config = KSharedConfig::openConfig(file, KConfig::SimpleConfig);
        RayeToasts::turnOff(config, u"org.raye.store"_s, u"store"_s);
        RayeToasts::turnOff(config, QString(), u"powerdevil"_s);
        RayeToasts::turnOff(config, QString(), QString());
        KConfigGroup app = config->group(u"Applications"_s).group(u"org.raye.store"_s);
        QCOMPARE(app.readEntry("ShowPopups", true), false);
        QCOMPARE(app.readEntry("ShowInHistory", true), false);
        QCOMPARE(config->group(u"Services"_s).group(u"powerdevil"_s).readEntry("ShowPopups", true), false);
        QVERIFY(!config->group(u"Services"_s).hasGroup(u"store"_s));
        QFile::remove(file);
    }

    QVariantMap toastOf(const QVariantMap &extra = {})
    {
        QVariantMap m{{u"applicationName"_s, u"Chat"_s}, {u"applicationIconName"_s, u"dialog-information"_s},
                      {u"summary"_s, u"Maya"_s}, {u"body"_s, u"<b>Lunch</b> at noon? &lt;script&gt;"_s}, {u"timeoutMs"_s, 0}};
        m.insert(extra);
        return m;
    }

    void toastCard()
    {
        auto v = view(u"toast"_s, toastOf({{u"actionNames"_s, QStringList{u"open"_s, u"later"_s}},
                                           {u"actionLabels"_s, QStringList{u"Open"_s, u"Later"_s}},
                                           {u"hasDefaultAction"_s, true}}));
        QVERIFY(v);
        QCOMPARE(child(v->item, u"appName"_s)->property("text").toString(), u"Chat"_s);
        QCOMPARE(child(v->item, u"summary"_s)->property("text").toString(), u"Maya"_s);
        // markup is shown as text, never read
        QCOMPARE(child(v->item, u"body"_s)->property("text").toString(), u"Lunch at noon? <script>"_s);
        QTRY_VERIFY(child(v->item, u"action:open"_s));
        click(v.get(), child(v->item, u"action:later"_s));
        click(v.get(), child(v->item, u"more"_s));
        QCOMPARE(calls(v.get()), (QStringList{u"action later"_s, u"menu"_s}));
        click(v.get(), child(v->item, u"close"_s));
        QTRY_VERIFY(calls(v.get()).contains(u"close"_s));
        QCOMPARE(v->item->property("width").toInt(), 364 + 8 + 12);
    }

    void toastWordsOfTheDesktop()
    {
        auto v = view(u"toast"_s, toastOf({{u"applicationName"_s, u"Plasma Desktop Workspace"_s}, {u"applicationIconName"_s, u"plasmashell"_s},
                                           {u"desktopEntry"_s, u"org.kde.plasmashell"_s}, {u"notifyRcName"_s, u"plasma_workspace"_s},
                                           {u"summary"_s, u"System Notifications"_s}, {u"body"_s, u"Could not read file x"_s}}));
        QVERIFY(v);
        QCOMPARE(child(v->item, u"appName"_s)->property("text").toString(), u"System"_s);
        QVERIFY(!child(v->item, u"summary"_s)->isVisible());
        QCOMPARE(child(v->item, u"body"_s)->property("text").toString(), u"Could not read file x"_s);

        auto n = view(u"toast"_s, toastOf({{u"applicationName"_s, u"Network Management"_s}, {u"notifyRcName"_s, u"networkmanagement"_s},
                                           {u"summary"_s, u"Wired connection 1"_s}, {u"body"_s, u"Connection 'Wired connection 1' activated."_s}}));
        QVERIFY(n);
        QCOMPARE(child(n->item, u"appName"_s)->property("text").toString(), u"Network"_s);
        QCOMPARE(child(n->item, u"summary"_s)->property("text").toString(), u"Connected to Ethernet"_s);
        QVERIFY(!child(n->item, u"body"_s)->isVisible());
    }

    void toastRunsOutButNotUnderThePointer()
    {
        auto v = view(u"toast"_s, toastOf({{u"timeoutMs"_s, 300}}));
        QVERIFY(v);
        // the pointer pauses the time
        const QPoint middle = v->item->mapToScene(QPointF(v->item->width() / 2, v->item->height() / 2)).toPoint();
        QTest::mouseMove(v->window, middle);
        QTRY_VERIFY(v->item->property("hovered").toBool());
        QVERIFY(!v->item->property("timing").toBool());
        QTest::qWait(500);
        QVERIFY(calls(v.get()).isEmpty());
        // and leaving starts it again; it slides out, then says so
        QTest::mouseMove(v->window, QPoint(v->window->width() - 1, v->window->height() - 1));
        QTest::mouseMove(v->window, QPoint(-5, -5));
        QTRY_VERIFY_WITH_TIMEOUT(!v->item->property("hovered").toBool(), 1000);
        QTRY_COMPARE_WITH_TIMEOUT(calls(v.get()), QStringList{u"timeUp"_s}, 2000);

        // an open menu holds it too; a toast with no time stays
        auto m = view(u"toast"_s, toastOf({{u"timeoutMs"_s, 100}, {u"menuOpen"_s, true}}));
        QVERIFY(m);
        QTest::mouseMove(m->window, QPoint(m->window->width() - 1, m->window->height() - 1));
        QTest::mouseMove(m->window, QPoint(-5, -5));
        QTRY_VERIFY_WITH_TIMEOUT(!m->item->property("hovered").toBool(), 1000);
        QVERIFY(!m->item->property("timing").toBool());
        QTest::qWait(300);
        QVERIFY(calls(m.get()).isEmpty());
        auto s = view(u"toast"_s, toastOf());
        QVERIFY(s);
        QTest::qWait(150);
        QVERIFY(!s->item->property("timing").toBool());
    }

    void toastReply()
    {
        auto v = view(u"toast"_s, toastOf({{u"hasReplyAction"_s, true}, {u"replyActionLabel"_s, u"Answer"_s}, {u"timeoutMs"_s, 100}}));
        QVERIFY(v);
        QTRY_VERIFY(child(v->item, u"reply"_s));
        QCOMPARE(child(v->item, u"reply"_s)->property("text").toString(), u"Answer"_s);
        click(v.get(), child(v->item, u"reply"_s));
        QVERIFY(v->item->property("replying").toBool());
        // the pointer off the card: only the reply holds it now
        QTest::mouseMove(v->window, QPoint(v->window->width() - 1, v->window->height() - 1));
        QTest::mouseMove(v->window, QPoint(-5, -5));
        QTRY_VERIFY_WITH_TIMEOUT(!v->item->property("hovered").toBool(), 1000);
        QVERIFY(!v->item->property("timing").toBool());
        QTest::qWait(250);
        QVERIFY2(calls(v.get()).isEmpty(), "a reply being typed holds the toast");
        QQuickItem *input = child(v->item, u"replyInput"_s);
        QVERIFY(input);
        QVERIFY(input->hasActiveFocus());
        QVERIFY(!child(v->item, u"send"_s)->isEnabled());
        input->setProperty("text", u"Yes!"_s);
        QTest::keyClick(v->window, Qt::Key_Return);
        QCOMPARE(calls(v.get()), QStringList{u"reply Yes!"_s});
    }

    void toastJob()
    {
        auto v = view(u"toast"_s, toastOf({{u"isJob"_s, true}, {u"jobState"_s, 1}, {u"percentage"_s, 45}, {u"killable"_s, true},
                                           {u"suspendable"_s, true}, {u"applicationName"_s, u"Plasma Desktop Workspace"_s},
                                           {u"applicationIconName"_s, u"plasmashell"_s}, {u"desktopEntry"_s, u"org.kde.plasmashell"_s},
                                           {u"summary"_s, u"Copying 3 files"_s}, {u"body"_s, QString()}}));
        QVERIFY(v);
        QCOMPARE(child(v->item, u"appName"_s)->property("text").toString(), u"Files"_s);
        QVERIFY(child(v->item, u"progress"_s)->isVisible());
        QTRY_VERIFY(child(v->item, u"suspend"_s));
        click(v.get(), child(v->item, u"suspend"_s));
        click(v.get(), child(v->item, u"kill"_s));
        QCOMPARE(calls(v.get()), (QStringList{u"suspend"_s, u"kill"_s}));
        QVERIFY(!child(v->item, u"more"_s)->isVisible());
    }

    void french()
    {
        QFile po(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_clock.po"));
        QVERIFY(po.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(po.readAll());
        QVERIFY(text.contains(u"msgstr \"Aucune nouvelle notification\""_s));
        QVERIFY(text.contains(u"msgstr \"Tout effacer\""_s));
        // every word of the toasts is there
        for (const char *word : {"System", "Network", "Devices", "Storage", "Power", "Accessibility", "Passwords", "Security", "Files",
                                 "Connected to %1", "Disconnected from %1", "More options", "Hide", "Close", "See more", "Pause", "Resume",
                                 "Reply", "Write a reply", "Send", "Turn off notifications from %1"}) {
            QVERIFY2(text.contains(u"msgid \""_s + QString::fromLatin1(word) + u"\""_s), word);
        }
    }
};

QTEST_MAIN(TestClock)

#include "tst_clock.moc"
