// Quick settings: its decisions (quicklogic.js) and the panel (QuickPanel.qml)
// on a stand-in for the system services: which tiles show on which PC, how
// they page, what Edit does to the saved list, and what a click asks for.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFontInfo>
#include <QFontMetrics>
#include <QJSEngine>
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>
#include <QtMath>

using namespace Qt::StringLiterals;

class TestQuickSettings : public QObject
{
    Q_OBJECT

private:
    QJSEngine m_js;

    QJSValue call(const char *name, const QJSValueList &args)
    {
        QJSValue f = m_js.globalObject().property(QLatin1String(name));
        const QJSValue r = f.call(args);
        if (r.isError())
            qWarning() << name << r.toString();
        return r;
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

    QJSValue offline(const QStringList &ids)
    {
        QJSValue o = m_js.newObject();
        for (const QString &id : ids)
            o.setProperty(id, false);
        return o;
    }

    struct View {
        std::unique_ptr<QQmlEngine> engine;
        std::unique_ptr<QObject> fixture;
        QQuickWindow *window = nullptr;
        QQuickItem *panel = nullptr;
        QObject *backend = nullptr;
    };

    std::unique_ptr<View> view()
    {
        auto v = std::make_unique<View>();
        v->engine = std::make_unique<QQmlEngine>();
        v->engine->addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        v->engine->addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        KLocalization::setupLocalizedContext(v->engine.get());
        QQmlComponent c(v->engine.get(), QUrl::fromLocalFile(QStringLiteral(RAYE_QS_TESTS "/fixture.qml")));
        QObject *o = c.create(v->engine->rootContext());
        if (!o) {
            qWarning() << c.errors();
            return nullptr;
        }
        v->fixture.reset(o);
        v->window = qobject_cast<QQuickWindow *>(o);
        v->panel = o->property("panel").value<QQuickItem *>();
        v->backend = o->property("backend").value<QObject *>();
        if (!v->window || !v->panel || !v->backend)
            return nullptr;
        v->window->show();
        if (!QTest::qWaitForWindowExposed(v->window))
            return nullptr;
        return v;
    }

    static QStringList shownIds(QQuickItem *panel)
    {
        return panel->property("shownIds").toStringList();
    }

    // Repeaters put what they make in the visual tree: look there
    static QQuickItem *item(QQuickItem *from, const QString &name)
    {
        if (from->objectName() == name)
            return from;
        const auto children = from->childItems();
        for (QQuickItem *child : children) {
            if (QQuickItem *found = item(child, name))
                return found;
        }
        return nullptr;
    }

    // a tile's own object, by the name the panel gives it
    static QQuickItem *tile(QQuickItem *panel, const QString &id)
    {
        return item(panel, u"tile-"_s + id);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QFile f(QStringLiteral(RAYE_QS_DIR "/contents/ui/quicklogic.js"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString code = QString::fromUtf8(f.readAll()).remove(u".pragma library"_s);
        const QJSValue r = m_js.evaluate(code);
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
    }

    void init()
    {
        // every test starts from the default tile list
        QFile::remove(QDir::home().filePath(u".config/rayeshellrc"_s));
    }

    // --- the tile itself, with the mouse ---------------------------------

    static QPoint at(QQuickItem *t, qreal x, qreal y)
    {
        return t->mapToScene(QPointF(x, y)).toPoint();
    }

    void aClickInTheMiddleTogglesAndTheArrowOpensThePage()
    {
        auto v = view();
        QVERIFY(v);
        QQuickItem *t = tile(v->panel, u"bluetooth"_s);
        QVERIFY(t);
        QTest::mouseClick(v->window, Qt::LeftButton, {}, at(t, 30, 24));
        QVERIFY(v->backend->property("calls").toStringList().contains(u"bluetooth:true"_s));
        QCOMPARE(v->panel->property("pageId").toString(), QString());
        QTest::mouseClick(v->window, Qt::LeftButton, {}, at(t, t->width() - 8, 24));
        QCOMPARE(v->panel->property("pageId").toString(), u"bluetooth"_s);
    }

    // the accent grows from the click, it does not snap
    void turningOnFillsTheTileFromTheClick()
    {
        auto v = view();
        QVERIFY(v);
        QQuickItem *t = tile(v->panel, u"airplane"_s);
        QVERIFY(t);
        QCOMPARE(t->property("fillLevel").toReal(), 0.0);
        QTest::mouseClick(v->window, Qt::LeftButton, {}, at(t, 20, 30));
        QVERIFY(t->property("checked").toBool());
        QCOMPARE(t->property("fillOrigin").toPointF(), QPointF(20, 30));
        const qreal start = t->property("fillLevel").toReal();
        QVERIFY2(start < 1.0, "the fill is already full");
        QTRY_COMPARE(t->property("fillLevel").toReal(), 1.0);
        QQuickItem *fill = item(t, u"fill"_s);
        QVERIFY(fill && fill->isVisible());

        QTest::mouseClick(v->window, Qt::LeftButton, {}, at(t, 20, 30));
        QTRY_COMPARE(t->property("fillLevel").toReal(), 0.0);
        QTRY_VERIFY(!fill->isVisible());
    }

    void aRightClickOffersItsSettingsPage_data()
    {
        QTest::addColumn<QString>("tile");
        QTest::addColumn<QString>("page");
        QTest::newRow("bluetooth") << u"bluetooth"_s << u"devices/bluetooth"_s;
        QTest::newRow("energy saver") << u"energysaver"_s << u"system/power"_s;
        // the stand-in is on a cable
        QTest::newRow("network on a cable") << u"wifi"_s << u"network/ethernet"_s;
    }

    void aRightClickOffersItsSettingsPage()
    {
        QFETCH(QString, tile);
        QFETCH(QString, page);
        auto v = view();
        QVERIFY(v);
        QQuickItem *t = TestQuickSettings::tile(v->panel, tile);
        QVERIFY(t);
        const QStringList before = v->backend->property("calls").toStringList();
        QTest::mouseClick(v->window, Qt::RightButton, {}, at(t, 30, 24));
        QCOMPARE(v->panel->property("menuTile").toString(), tile);
        // nothing changed with it
        QCOMPARE(v->backend->property("calls").toStringList(), before);
        QCOMPARE(v->panel->property("pageId").toString(), QString());
        QObject *menu = v->panel->property("tileMenu").value<QObject *>();
        QVERIFY(menu);
        QTRY_VERIFY(menu->property("opened").toBool());
        QQuickItem *go = nullptr;
        QMetaObject::invokeMethod(menu, "itemAt", Q_RETURN_ARG(QQuickItem *, go), Q_ARG(int, 0));
        QVERIFY(go);
        QCOMPARE(go->objectName(), u"goToSettings"_s);
        QCOMPARE(go->property("text").toString(), u"Go to settings"_s);
        QMetaObject::invokeMethod(go, "triggered");
        QCOMPARE(v->fixture->property("opened").toStringList(), QStringList{page});
    }

    void editingHasNoTileMenu()
    {
        auto v = view();
        QVERIFY(v);
        v->panel->setProperty("editing", true);
        QQuickItem *t = tile(v->panel, u"bluetooth"_s);
        QVERIFY(t);
        QTest::mouseClick(v->window, Qt::RightButton, {}, at(t, 30, 24));
        QCOMPARE(v->panel->property("menuTile").toString(), QString());
    }

    // --- the window around the panel -------------------------------------

    // Wayland places a popup when it maps; a window that grows later grows
    // under the taskbar. So the size stays put and the panel sits at the bottom.
    void theWindowKeepsItsSizeAcrossPages()
    {
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        engine.addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        KLocalization::setupLocalizedContext(&engine);
        QQmlComponent c(&engine, QUrl::fromLocalFile(QStringLiteral(RAYE_QS_TESTS "/window_fixture.qml")));
        std::unique_ptr<QObject> o(c.create(engine.rootContext()));
        QVERIFY2(o, qPrintable(c.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(o.get());
        auto *panel = o->property("panel").value<QQuickItem *>();
        auto *backend = o->property("backend").value<QObject *>();
        QVERIFY(window && panel && backend);

        QVariantList many;
        for (int i = 0; i < 8; ++i)
            many << QVariantMap{{u"name"_s, u"Net %1"_s.arg(i)}, {u"kind"_s, u"wifi"_s}, {u"state"_s, u""_s}, {u"signal"_s, 60}};
        backend->setProperty("networkConnections", many);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(window->height() >= qCeil(panel->height()));
        const int height = window->height();
        const int tiles = qCeil(panel->height());
        auto bottomOfPanel = [&] { return qCeil(panel->mapToScene(QPointF(0, panel->height())).y()); };
        QCOMPARE(bottomOfPanel(), height);

        for (const QString &page : {u"network"_s, u"bluetooth"_s, u"sound"_s, u""_s}) {
            QMetaObject::invokeMethod(panel, "openPage", Q_ARG(QVariant, page));
            QTest::qWait(50);
            QCOMPARE(window->height(), height);
            QVERIFY2(qCeil(panel->height()) <= height, qPrintable(page));
            QCOMPARE(bottomOfPanel(), height);
        }
        QMetaObject::invokeMethod(panel, "openPage", Q_ARG(QVariant, u"network"_s));
        QTRY_VERIFY(qCeil(panel->height()) != tiles);
        QCOMPARE(window->height(), height);
        QCOMPARE(window->width(), qCeil(panel->width()));
    }

    // --- decisions -------------------------------------------------------

    void aDefaultListKeepsAllItsTilesWhenThePcHasThemAll()
    {
        const QStringList defaults = {u"wifi"_s, u"bluetooth"_s, u"airplane"_s, u"energysaver"_s, u"nightlight"_s, u"accessibility"_s};
        QCOMPARE(strings(call("shownTiles", {list(defaults), offline({})})), defaults);
    }

    // a desktop PC has no Wi-Fi, no Bluetooth: it must not open on three tiles
    void aDesktopPcStillOpensOnAFullGrid()
    {
        const QStringList defaults = {u"wifi"_s, u"bluetooth"_s, u"airplane"_s, u"energysaver"_s, u"nightlight"_s, u"accessibility"_s};
        const QStringList got = strings(call("shownTiles", {list(defaults), offline({u"wifi"_s, u"bluetooth"_s, u"airplane"_s, u"vpn"_s})}));
        QCOMPARE(got, QStringList({u"energysaver"_s, u"nightlight"_s, u"accessibility"_s, u"project"_s, u"dnd"_s, u"mic"_s}));
    }

    // a list the person chose is left as it is, however short
    void aChosenListIsNotFilledIn()
    {
        QCOMPARE(strings(call("shownTiles", {list({u"nightlight"_s}), offline({})})), QStringList{u"nightlight"_s});
    }

    void aTileThisPcCannotDoIsHiddenEvenWhenChosen()
    {
        QCOMPARE(strings(call("shownTiles", {list({u"wifi"_s, u"dnd"_s}), offline({u"wifi"_s})})), QStringList{u"dnd"_s});
    }

    void anUnknownOrRepeatedTileIsDropped()
    {
        QCOMPARE(strings(call("shownTiles", {list({u"dnd"_s, u"nonsense"_s, u"dnd"_s}), offline({})})), QStringList{u"dnd"_s});
    }

    void addOffersOnlyWhatIsMissingAndPossible()
    {
        const QStringList got = strings(call("addableTiles", {list({u"wifi"_s}), offline({u"vpn"_s, u"hotspot"_s})}));
        QVERIFY(!got.contains(u"wifi"_s));
        QVERIFY(!got.contains(u"vpn"_s));
        QVERIFY(got.contains(u"project"_s));
    }

    void pages_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<int>("pages");
        QTest::newRow("empty") << 0 << 1;
        QTest::newRow("one") << 1 << 1;
        QTest::newRow("full") << 6 << 1;
        QTest::newRow("spill") << 7 << 2;
        QTest::newRow("twelve") << 12 << 2;
    }
    void pages()
    {
        QFETCH(int, count);
        QFETCH(int, pages);
        QCOMPARE(call("pageCount", {count}).toInt(), pages);
    }

    void aPageIsAWindowOfTheList()
    {
        const QStringList all = {u"a"_s, u"b"_s, u"c"_s, u"d"_s, u"e"_s, u"f"_s, u"g"_s};
        QCOMPARE(strings(call("page", {list(all), 1})), QStringList{u"g"_s});
        // past the end: the last page, so removing a tile never strands the view
        QCOMPARE(strings(call("page", {list(all), 5})), QStringList{u"g"_s});
    }

    void movingATileKeepsTheOthersInOrder()
    {
        const QStringList all = {u"a"_s, u"b"_s, u"c"_s};
        QCOMPARE(strings(call("moveTile", {list(all), 0, 2})), QStringList({u"b"_s, u"c"_s, u"a"_s}));
        QCOMPARE(strings(call("moveTile", {list(all), 2, 0})), QStringList({u"c"_s, u"a"_s, u"b"_s}));
        QCOMPARE(strings(call("moveTile", {list(all), 1, 9})), QStringList({u"a"_s, u"c"_s, u"b"_s}));
        QCOMPARE(strings(call("moveTile", {list(all), 9, 0})), all);
    }

    void aDropLandsOnTheCellUnderIt()
    {
        QCOMPARE(call("cellAt", {10, 10, 104, 80, 3}).toInt(), 0);
        QCOMPARE(call("cellAt", {110, 10, 104, 80, 3}).toInt(), 1);
        QCOMPARE(call("cellAt", {10, 90, 104, 80, 3}).toInt(), 3);
        // off the right edge stays in the row
        QCOMPARE(call("cellAt", {999, 90, 104, 80, 3}).toInt(), 5);
    }

    void addingAndRemoving()
    {
        QCOMPARE(strings(call("addTile", {list({u"wifi"_s}), u"dnd"_s})), QStringList({u"wifi"_s, u"dnd"_s}));
        QCOMPARE(strings(call("addTile", {list({u"wifi"_s}), u"wifi"_s})), QStringList{u"wifi"_s});
        QCOMPARE(strings(call("addTile", {list({u"wifi"_s}), u"nonsense"_s})), QStringList{u"wifi"_s});
        QCOMPARE(strings(call("removeTile", {list({u"wifi"_s, u"dnd"_s}), u"wifi"_s})), QStringList{u"dnd"_s});
    }

    QJSValue conn(const QString &name, const QString &kind, const QString &state, int signal = 0)
    {
        QJSValue o = m_js.newObject();
        o.setProperty(u"name"_s, name);
        o.setProperty(u"kind"_s, kind);
        o.setProperty(u"state"_s, state);
        o.setProperty(u"signal"_s, signal);
        return o;
    }
    QJSValue conns(const QList<QJSValue> &items)
    {
        QJSValue a = m_js.newArray(items.size());
        for (int i = 0; i < items.size(); ++i)
            a.setProperty(i, items.at(i));
        return a;
    }

    // on a cable with Wi-Fi also connected, the tile says Ethernet
    void theCableWinsOverWifi()
    {
        const QJSValue s = call("networkState", {conns({conn(u"Maison"_s, u"wifi"_s, u"active"_s, 80),
                                                        conn(u"Wired connection 1"_s, u"wired"_s, u"active"_s)}), true, true});
        QCOMPARE(s.property(u"kind"_s).toString(), u"wired"_s);
        QCOMPARE(s.property(u"name"_s).toString(), u"Wired connection 1"_s);
    }

    void networkStates()
    {
        QJSValue s = call("networkState", {conns({conn(u"Maison"_s, u"wifi"_s, u"active"_s, 80)}), true, true});
        QCOMPARE(s.property(u"kind"_s).toString(), u"wifi"_s);
        QCOMPARE(s.property(u"name"_s).toString(), u"Maison"_s);
        s = call("networkState", {conns({conn(u"Maison"_s, u"wifi"_s, u""_s, 80)}), true, true});
        QCOMPARE(s.property(u"kind"_s).toString(), u"none"_s);
        s = call("networkState", {conns({}), true, false});
        QCOMPARE(s.property(u"kind"_s).toString(), u"off"_s);
        s = call("networkState", {conns({conn(u"Maison"_s, u"wifi"_s, u"connecting"_s)}), true, true});
        QCOMPARE(s.property(u"kind"_s).toString(), u"connecting"_s);
    }

    // the tile and its page are about the cable when it is in use, or when
    // the PC has no Wi-Fi card at all
    void theNetworkTileFollowsTheCable()
    {
        const QJSValue cable = conn(u"Wired connection 1"_s, u"wired"_s, u"active"_s);
        const QJSValue idleCable = conn(u"Wired connection 1"_s, u"wired"_s, u""_s);
        const QJSValue home = conn(u"Maison"_s, u"wifi"_s, u"active"_s, 80);
        QCOMPARE(call("networkMode", {conns({}), false}).toString(), u"ethernet"_s);
        QCOMPARE(call("networkMode", {conns({idleCable}), false}).toString(), u"ethernet"_s);
        QCOMPARE(call("networkMode", {conns({home, cable}), true}).toString(), u"ethernet"_s);
        QCOMPARE(call("networkMode", {conns({conn(u"Wired connection 1"_s, u"wired"_s, u"connecting"_s)}), true}).toString(),
                 u"ethernet"_s);
        QCOMPARE(call("networkMode", {conns({home}), true}).toString(), u"wifi"_s);
        QCOMPARE(call("networkMode", {conns({idleCable, home}), true}).toString(), u"wifi"_s);
        QCOMPARE(call("networkMode", {conns({}), true}).toString(), u"wifi"_s);
    }

    // NetworkManager's own names for a cable read "Ethernet"; a name the
    // user gave it stays
    void cableName()
    {
        QCOMPARE(call("cableName", {u"Wired connection 1"_s}).toString(), QString());
        QCOMPARE(call("cableName", {u"Wired connection"_s}).toString(), QString());
        QCOMPARE(call("cableName", {u"wired connection 12"_s}).toString(), QString());
        QCOMPARE(call("cableName", {u"Connexion filaire 2"_s}).toString(), QString());
        QCOMPARE(call("cableName", {u"Connexion filaire"_s}).toString(), QString());
        QCOMPARE(call("cableName", {u""_s}).toString(), QString());
        QCOMPARE(call("cableName", {QJSValue()}).toString(), QString());
        QCOMPARE(call("cableName", {u"Bureau"_s}).toString(), u"Bureau"_s);
        QCOMPARE(call("cableName", {u"Wired connection to the office"_s}).toString(), u"Wired connection to the office"_s);
    }

    void theAddressIsTheFirstIpv4()
    {
        QCOMPARE(call("ipv4Address", {list({u"Ethernet"_s, u"AA:BB:CC:DD:EE:FF"_s, u"1 Gb/s"_s, u"192.168.50.50/24"_s, u"192.168.50.1"_s})})
                     .toString(),
                 u"192.168.50.50"_s);
        QCOMPARE(call("ipv4Address", {list({u"fe80::1"_s, u"999.1.1.1"_s})}).toString(), QString());
        QCOMPARE(call("ipv4Address", {list({})}).toString(), QString());
        QCOMPARE(call("ipv4Address", {QJSValue()}).toString(), QString());
    }

    void theNetworkListPutsTheCableFirstThenTheStrongest()
    {
        const QJSValue sorted = call("sortNetworks", {conns({conn(u"Weak"_s, u"wifi"_s, u""_s, 20),
                                                             conn(u"Strong"_s, u"wifi"_s, u""_s, 90),
                                                             conn(u"Home"_s, u"wifi"_s, u"active"_s, 40),
                                                             conn(u"Strong"_s, u"wifi"_s, u""_s, 85),
                                                             conn(u"Cable"_s, u"wired"_s, u""_s)})});
        QStringList names;
        for (int i = 0; i < sorted.property(u"length"_s).toInt(); ++i)
            names << sorted.property(i).property(u"name"_s).toString();
        QCOMPARE(names, (QStringList{u"Cable"_s, u"Home"_s, u"Strong"_s, u"Weak"_s}));
    }

    void whatConnectDoes()
    {
        QJSValue c = conn(u"Cafe"_s, u"wifi"_s, u""_s, 50);
        c.setProperty(u"askPassword"_s, true);
        QCOMPARE(call("networkAction", {c, u""_s}).toString(), u"password"_s);
        QCOMPARE(call("networkAction", {c, u"key"_s}).toString(), u"connect"_s);
        QCOMPARE(call("networkAction", {conn(u"Home"_s, u"wifi"_s, u"active"_s), u""_s}).toString(), u"disconnect"_s);
    }

    // every "›" leads to a page of the panel; night light has none, like Windows
    void everyArrowHasAPage()
    {
        QCOMPARE(call("tilePage", {u"wifi"_s}).toString(), u"network"_s);
        QCOMPARE(call("tilePage", {u"bluetooth"_s}).toString(), u"bluetooth"_s);
        QCOMPARE(call("tilePage", {u"vpn"_s}).toString(), u"vpn"_s);
        QVERIFY(!call("isSplit", {u"nightlight"_s}).toBool());
        QCOMPARE(call("wifiIcon", {60, true}).toString(), u"network-wireless-60-locked-symbolic"_s);
        QCOMPARE(call("wifiIcon", {3, false}).toString(), u"network-wireless-signal-none-symbolic"_s);
    }

    void volumeGlyph_data()
    {
        QTest::addColumn<int>("percent");
        QTest::addColumn<bool>("muted");
        QTest::addColumn<QString>("icon");
        QTest::newRow("muted") << 80 << true << u"audio-volume-muted-symbolic"_s;
        QTest::newRow("zero") << 0 << false << u"audio-volume-muted-symbolic"_s;
        QTest::newRow("low") << 20 << false << u"audio-volume-low-symbolic"_s;
        QTest::newRow("medium") << 50 << false << u"audio-volume-medium-symbolic"_s;
        QTest::newRow("high") << 90 << false << u"audio-volume-high-symbolic"_s;
    }
    void volumeGlyph()
    {
        QFETCH(int, percent);
        QFETCH(bool, muted);
        QFETCH(QString, icon);
        QCOMPARE(call("volumeIcon", {percent, muted}).toString(), icon);
    }

    void batteryGlyph_data()
    {
        QTest::addColumn<int>("percent");
        QTest::addColumn<bool>("charging");
        QTest::addColumn<QString>("icon");
        QTest::newRow("empty") << 0 << false << u"battery-000-symbolic"_s;
        QTest::newRow("low") << 7 << false << u"battery-010-symbolic"_s;
        QTest::newRow("high") << 84 << false << u"battery-080-symbolic"_s;
        QTest::newRow("full") << 100 << false << u"battery-100-symbolic"_s;
        QTest::newRow("charging") << 56 << true << u"battery-060-charging-symbolic"_s;
    }
    void batteryGlyph()
    {
        QFETCH(int, percent);
        QFETCH(bool, charging);
        QFETCH(QString, icon);
        QCOMPARE(call("batteryIcon", {percent, charging}).toString(), icon);
    }

    void theWheelStepsTheVolumeAndStopsAtTheEnds()
    {
        QCOMPARE(call("wheelVolume", {50, 120}).toInt(), 52);
        QCOMPARE(call("wheelVolume", {50, -120}).toInt(), 48);
        QCOMPARE(call("wheelVolume", {100, 120}).toInt(), 100);
        QCOMPARE(call("wheelVolume", {1, -120}).toInt(), 0);
        QCOMPARE(call("wheelVolume", {50, 0}).toInt(), 50);
    }

    void aPercentOfNothingIsZero()
    {
        QCOMPARE(call("clampPercent", {5, 0}).toInt(), 0);
        QCOMPARE(call("clampPercent", {50, 200}).toInt(), 25);
    }

    // every tile opens a page that exists in Raye Settings
    void everyTileLeadsSomewhere()
    {
        const QStringList tiles = strings(m_js.globalObject().property(u"TILES"_s));
        QVERIFY(!tiles.isEmpty());
        for (const QString &id : tiles) {
            QVERIFY2(!call("settingsPage", {id}).toString().isEmpty(), qPrintable(id));
            QVERIFY2(!call("tileIcon", {id}).toString().isEmpty(), qPrintable(id));
        }
    }

    // the two rules a real plasmashell enforces and the harness does not:
    // nothing shows in a panel without a fullRepresentation, and a property
    // bound to itself stays undefined. Both have left a taskbar applet blank.
    void plasmaRules()
    {
        static const QRegularExpression selfBinding(u"^\\s*(\\w+)\\s*:\\s*\\1\\s*$"_s);
        QDirIterator it(QStringLiteral(RAYE_QS_DIR "/contents/ui"), {u"*.qml"_s}, QDir::Files, QDirIterator::Subdirectories);
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
        QFile main(QStringLiteral(RAYE_QS_DIR "/contents/ui/main.qml"));
        QVERIFY(main.open(QIODevice::ReadOnly));
        const QString code = QString::fromUtf8(main.readAll());
        QVERIFY2(code.contains(u"fullRepresentation:"_s), "main.qml needs a fullRepresentation");
        QVERIFY2(code.contains(u"sequence: \"Meta+A\""_s), "Win+A opens the panel");
        // the Dialog sets its main item's size: a Loader there stops
        // following the panel's pages (see PanelFrame.qml)
        static const QRegularExpression frame(u"mainItem:\\s*PanelFrame\\s*\\{[^{}]*\\bfocus:\\s*true"_s);
        QVERIFY2(frame.match(code).hasMatch(), "the window's item is a PanelFrame with focus: true");
        // a Loader is a focus scope: without focus the panel never gets the
        // keys of its window, and Esc does not close it
        static const QRegularExpression loaderFocus(u"Loader\\s*\\{\\s*id:\\s*panelLoader[^{}]*\\bfocus:\\s*true"_s);
        QVERIFY2(loaderFocus.match(code).hasMatch(), "the panel's Loader needs focus: true");
    }

    // a tile's name sits on one line under 96 px: in both languages it fits
    void everyTileNameFitsUnderItsTile()
    {
        auto v = view();
        QVERIFY(v);
        const QFontInfo info{QFont(u"Inter"_s)};
        if (info.family() != u"Inter"_s)
            QSKIP("needs the Inter font");
        QFont font(u"Inter"_s);
        font.setPixelSize(12);
        const QFontMetrics metrics(font);

        QFile po(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_quicksettings.po"));
        QVERIFY(po.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromUtf8(po.readAll()).split(u'\n');

        const QStringList tiles = strings(m_js.globalObject().property(u"TILES"_s));
        for (const QString &id : tiles) {
            QVariant result;
            QVERIFY(QMetaObject::invokeMethod(v->panel, "title", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, id)));
            const QString english = result.toString();
            QVERIFY2(!english.isEmpty(), qPrintable(id));
            QVERIFY2(metrics.horizontalAdvance(english) <= 96, qPrintable(english));

            const int at = lines.indexOf(u"msgid \"%1\""_s.arg(english));
            QVERIFY2(at >= 0, qPrintable(english));
            QString french = lines.at(at + 1);
            french = french.mid(8, french.size() - 9);
            QVERIFY2(!french.isEmpty(), qPrintable(english));
            QVERIFY2(metrics.horizontalAdvance(french) <= 96, qPrintable(french));
        }
    }

    // --- the panel -------------------------------------------------------

    void thePanelIs360WideAroundItsMargin()
    {
        auto v = view();
        QVERIFY(v);
        const int margin = v->panel->property("margin").toInt();
        QCOMPARE(v->panel->width(), 360.0 + 2 * margin);
    }

    void aClickOnATileAsksForTheChange()
    {
        auto v = view();
        QVERIFY(v);
        QQuickItem *t = tile(v->panel, u"bluetooth"_s);
        QVERIFY(t);
        QVERIFY(!t->property("checked").toBool());
        QMetaObject::invokeMethod(t, "toggled");
        QVERIFY(v->backend->property("calls").toStringList().contains(u"bluetooth:true"_s));
        QTRY_VERIFY(t->property("checked").toBool());
    }

    // the "›" opens a page of the panel: Settings only from its bottom link
    void theArrowOfTheNetworkTileOpensItsPageNotSettings()
    {
        auto v = view();
        QVERIFY(v);
        // on Wi-Fi only: the cable is not plugged in
        QVariantList wifiOnly = v->backend->property("networkConnections").toList();
        wifiOnly.removeFirst();
        v->backend->setProperty("networkConnections", wifiOnly);
        QQuickItem *t = tile(v->panel, u"wifi"_s);
        QVERIFY(t);
        QVERIFY(t->property("split").toBool());
        QMetaObject::invokeMethod(t, "expandRequested");
        QCOMPARE(v->panel->property("pageId").toString(), u"network"_s);
        QCOMPARE(v->fixture->property("opened").toStringList(), QStringList());
        QVERIFY(v->backend->property("calls").toStringList().contains(u"scan"_s));
        QQuickItem *page = item(v->panel, u"networkPage"_s);
        QVERIFY(page);
        QTRY_VERIFY(page->isVisible());
        QCOMPARE(page->property("title").toString(), u"Wi-Fi"_s);

        // a new network asks its key, then connects
        QQuickItem *cafe = nullptr;
        QTRY_VERIFY((cafe = item(v->panel, u"network-1"_s)));
        QCOMPARE(cafe->property("name").toString(), u"Cafe"_s);
        QMetaObject::invokeMethod(cafe, "clicked");
        QTRY_VERIFY(cafe->property("selected").toBool());
        QVERIFY(cafe->property("askPassword").toBool());
        QQuickItem *field = item(cafe, u"passwordField"_s);
        QVERIFY(field);
        field->setProperty("text", u"secret-key"_s);
        QMetaObject::invokeMethod(cafe, "action");
        QVERIFY(v->backend->property("calls").toStringList().contains(u"network:1:secret-key"_s));
        QCOMPARE(v->fixture->property("opened").toStringList(), QStringList());

        QMetaObject::invokeMethod(page, "linkClicked");
        QCOMPARE(v->fixture->property("opened").toStringList(), QStringList{u"network"_s});
    }

    static bool shown(QQuickItem *from, const QString &name)
    {
        QQuickItem *i = item(from, name);
        return i && i->isVisible();
    }

    // no Wi-Fi card: an Ethernet tile, and nothing on its page talks of Wi-Fi
    void withoutAWifiCardThereIsNoWifi()
    {
        auto v = view();
        QVERIFY(v);
        v->backend->setProperty("wifiAvailable", false);
        QQuickItem *t = nullptr;
        QTRY_VERIFY((t = tile(v->panel, u"wifi"_s)));
        QTRY_COMPARE(t->property("text").toString(), u"Ethernet"_s);
        QCOMPARE(t->property("subtitle").toString(), u"Ethernet"_s);
        QCOMPARE(t->property("iconName").toString(), u"network-wired-symbolic"_s);
        QVERIFY(t->property("checked").toBool());

        QMetaObject::invokeMethod(t, "toggled");
        QCOMPARE(v->panel->property("pageId").toString(), u"network"_s);
        QQuickItem *page = item(v->panel, u"networkPage"_s);
        QVERIFY(page);
        QTRY_VERIFY(page->isVisible());
        QCOMPARE(page->property("title").toString(), u"Ethernet"_s);
        QVERIFY(!shown(page, u"pageSwitch"_s));
        QVERIFY(!shown(page, u"wifiFold"_s));
        QTRY_VERIFY(shown(page, u"network-0"_s));
        QQuickItem *cable = item(page, u"network-0"_s);
        QCOMPARE(cable->property("name").toString(), u"Ethernet"_s);
        QCOMPARE(cable->property("status").toString(), u"Connected · 192.168.50.50"_s);
        QVERIFY(!shown(page, u"network-1"_s));
        QVERIFY(!shown(page, u"network-2"_s));
        QVERIFY(!shown(page, u"emptyNetworks"_s));
        const QStringList calls = v->backend->property("calls").toStringList();
        QVERIFY(!calls.contains(u"scan"_s));
        QVERIFY(!calls.contains(u"wifi:false"_s));
    }

    // a cable in use: the tile is Ethernet, Wi-Fi waits folded on its page
    void onACableTheWifiListStaysFolded()
    {
        auto v = view();
        QVERIFY(v);
        QQuickItem *t = tile(v->panel, u"wifi"_s);
        QVERIFY(t);
        QCOMPARE(t->property("text").toString(), u"Ethernet"_s);
        QCOMPARE(t->property("subtitle").toString(), u"Ethernet"_s);
        QMetaObject::invokeMethod(t, "toggled");
        QCOMPARE(v->panel->property("pageId").toString(), u"network"_s);
        QVERIFY(!v->backend->property("calls").toStringList().contains(u"wifi:false"_s));
        QVERIFY(!v->backend->property("calls").toStringList().contains(u"scan"_s));

        QQuickItem *page = item(v->panel, u"networkPage"_s);
        QTRY_VERIFY(page && page->isVisible());
        QCOMPARE(page->property("title").toString(), u"Ethernet"_s);
        QVERIFY(!shown(page, u"pageSwitch"_s));
        QTRY_VERIFY(shown(page, u"network-0"_s));
        QVERIFY(shown(page, u"wifiFold"_s));
        QVERIFY(!shown(page, u"network-1"_s));

        QQuickItem *fold = item(page, u"wifiFold"_s);
        QMetaObject::invokeMethod(fold, "clicked");
        QTRY_VERIFY(shown(page, u"network-1"_s));
        QVERIFY(shown(page, u"network-2"_s));
        QVERIFY(v->backend->property("calls").toStringList().contains(u"scan"_s));
        QQuickItem *wifiSwitch = item(fold, u"wifiSwitch"_s);
        QVERIFY(wifiSwitch && wifiSwitch->isVisible());
        QVERIFY(wifiSwitch->property("checked").toBool());

        QMetaObject::invokeMethod(fold, "clicked");
        QTRY_VERIFY(!shown(page, u"network-1"_s));
    }

    // pulling the cable out turns the open panel back to Wi-Fi
    void unpluggingTheCableBringsWifiBack()
    {
        auto v = view();
        QVERIFY(v);
        QSignalSpy closing(v->panel, SIGNAL(requestClose()));
        QQuickItem *t = tile(v->panel, u"wifi"_s);
        QVERIFY(t);
        QMetaObject::invokeMethod(t, "toggled");
        QQuickItem *page = item(v->panel, u"networkPage"_s);
        QTRY_VERIFY(page && page->isVisible());

        const QVariantList withCable = v->backend->property("networkConnections").toList();
        QVariantList wifiOnly = withCable;
        wifiOnly.removeFirst();
        v->backend->setProperty("networkConnections", wifiOnly);
        QTRY_COMPARE(t->property("text").toString(), u"Wi-Fi"_s);
        QCOMPARE(v->panel->property("pageId").toString(), u"network"_s);
        QVERIFY(page->isVisible());
        QCOMPARE(page->property("title").toString(), u"Wi-Fi"_s);
        QVERIFY(shown(page, u"pageSwitch"_s));
        QVERIFY(!shown(page, u"wifiFold"_s));
        QTRY_VERIFY(shown(page, u"network-0"_s));
        QCOMPARE(item(page, u"network-0"_s)->property("name").toString(), u"Maison"_s);
        QVERIFY(v->backend->property("calls").toStringList().contains(u"scan"_s));

        // plugged back in: Ethernet again, Wi-Fi folded
        v->backend->setProperty("networkConnections", withCable);
        QTRY_COMPARE(t->property("text").toString(), u"Ethernet"_s);
        QCOMPARE(page->property("title").toString(), u"Ethernet"_s);
        QTRY_VERIFY(shown(page, u"wifiFold"_s));
        QVERIFY(!shown(page, u"network-1"_s));
        QCOMPARE(closing.count(), 0);
    }

    // a cable the user named keeps its name on the page
    void namedCableKeepsItsName()
    {
        auto v = view();
        QVERIFY(v);
        QVariantList list = v->backend->property("networkConnections").toList();
        QVariantMap cable = list.first().toMap();
        cable[u"name"_s] = u"Bureau"_s;
        list[0] = cable;
        v->backend->setProperty("networkConnections", list);
        QQuickItem *t = tile(v->panel, u"wifi"_s);
        QVERIFY(t);
        QMetaObject::invokeMethod(t, "toggled");
        QQuickItem *page = item(v->panel, u"networkPage"_s);
        QTRY_VERIFY(page && page->isVisible());
        QTRY_VERIFY(shown(page, u"network-0"_s));
        QCOMPARE(item(page, u"network-0"_s)->property("name").toString(), u"Bureau"_s);
    }

    // Wi-Fi opened by hand, cable out and back in: folded again
    void replugFoldsWifi()
    {
        auto v = view();
        QVERIFY(v);
        QQuickItem *t = tile(v->panel, u"wifi"_s);
        QVERIFY(t);
        QMetaObject::invokeMethod(t, "toggled");
        QQuickItem *page = item(v->panel, u"networkPage"_s);
        QTRY_VERIFY(page && page->isVisible());
        QQuickItem *fold = item(page, u"wifiFold"_s);
        QVERIFY(fold);
        QMetaObject::invokeMethod(fold, "clicked");
        QTRY_VERIFY(shown(page, u"network-1"_s));

        const QVariantList withCable = v->backend->property("networkConnections").toList();
        QVariantList wifiOnly = withCable;
        wifiOnly.removeFirst();
        v->backend->setProperty("networkConnections", wifiOnly);
        QTRY_COMPARE(t->property("text").toString(), u"Wi-Fi"_s);

        v->backend->setProperty("networkConnections", withCable);
        QTRY_COMPARE(t->property("text").toString(), u"Ethernet"_s);
        QTRY_VERIFY(shown(page, u"wifiFold"_s));
        QVERIFY(!item(page, u"wifiFold"_s)->property("expanded").toBool());
        QVERIFY(!shown(page, u"network-1"_s));
        QVERIFY(!shown(page, u"network-2"_s));
    }

    void neitherCableNorWifiIsNotConnected()
    {
        auto v = view();
        QVERIFY(v);
        QQuickItem *t = tile(v->panel, u"wifi"_s);
        QVERIFY(t);
        QVariantMap home{{u"name"_s, u"Maison"_s}, {u"kind"_s, u"wifi"_s}, {u"state"_s, u""_s}, {u"signal"_s, 90}};
        v->backend->setProperty("networkConnections", QVariantList{home});
        QTRY_COMPARE(t->property("text").toString(), u"Wi-Fi"_s);
        QCOMPARE(t->property("subtitle").toString(), u"Not connected"_s);

        // no card and the cable out
        v->backend->setProperty("wifiAvailable", false);
        v->backend->setProperty("networkConnections", QVariantList{});
        QTRY_COMPARE(t->property("text").toString(), u"Ethernet"_s);
        QCOMPARE(t->property("subtitle").toString(), u"Not connected"_s);
        QVERIFY(!t->property("checked").toBool());
        QMetaObject::invokeMethod(t, "toggled");
        QQuickItem *page = item(v->panel, u"networkPage"_s);
        QTRY_VERIFY(page && page->isVisible());
        QTRY_VERIFY(shown(page, u"emptyNetworks"_s));
        QVERIFY(!item(page, u"emptyNetworks"_s)->property("text").toString().contains(u"Wi-Fi"_s));
    }

    void backLeavesThePageForTheTiles()
    {
        auto v = view();
        QVERIFY(v);
        QMetaObject::invokeMethod(v->panel, "openPage", Q_ARG(QVariant, u"network"_s));
        QQuickItem *page = item(v->panel, u"networkPage"_s);
        QVERIFY(page);
        QQuickItem *back = item(page, u"backButton"_s);
        QVERIFY(back);
        QMetaObject::invokeMethod(back, "clicked");
        QCOMPARE(v->panel->property("pageId").toString(), QString());
        QTRY_VERIFY(tile(v->panel, u"wifi"_s));
    }

    void theNetworkTileSaysWhatThePcIsOn()
    {
        auto v = view();
        QVERIFY(v);
        QQuickItem *t = tile(v->panel, u"wifi"_s);
        QVERIFY(t);
        QCOMPARE(t->property("subtitle").toString(), u"Ethernet"_s);
        QCOMPARE(t->property("iconName").toString(), u"network-wired-symbolic"_s);

        QVariantMap home{{u"name"_s, u"Maison"_s}, {u"kind"_s, u"wifi"_s}, {u"state"_s, u"active"_s}, {u"signal"_s, 90}};
        v->backend->setProperty("networkConnections", QVariantList{home});
        QTRY_COMPARE(t->property("subtitle").toString(), u"Maison"_s);
        QCOMPARE(t->property("iconName").toString(), u"network-wireless-signal-excellent-symbolic"_s);

        v->backend->setProperty("networkConnections", QVariantList{});
        QTRY_COMPARE(t->property("subtitle").toString(), u"Not connected"_s);
    }

    // a PC on a cable without Wi-Fi keeps a network tile, and it opens the list
    void aCableOnlyPcKeepsANetworkTile()
    {
        auto v = view();
        QVERIFY(v);
        v->backend->setProperty("wifiAvailable", false);
        QQuickItem *t = nullptr;
        QTRY_VERIFY((t = tile(v->panel, u"wifi"_s)));
        QTRY_COMPARE(t->property("text").toString(), u"Ethernet"_s);
        QVERIFY(t->property("checked").toBool());
        QMetaObject::invokeMethod(t, "toggled");
        QCOMPARE(v->panel->property("pageId").toString(), u"network"_s);
        QVERIFY(!v->backend->property("calls").toStringList().contains(u"wifi:false"_s));
    }

    void theBluetoothArrowOpensItsDevices()
    {
        auto v = view();
        QVERIFY(v);
        v->backend->setProperty("bluetoothOn", true);
        QQuickItem *t = tile(v->panel, u"bluetooth"_s);
        QVERIFY(t);
        QMetaObject::invokeMethod(t, "expandRequested");
        QCOMPARE(v->panel->property("pageId").toString(), u"bluetooth"_s);
        QQuickItem *mouse = nullptr;
        QTRY_VERIFY((mouse = item(v->panel, u"bluetooth-1"_s)));
        QMetaObject::invokeMethod(mouse, "action");
        QVERIFY(v->backend->property("calls").toStringList().contains(u"btdevice:1:true"_s));
        QCOMPARE(v->fixture->property("opened").toStringList(), QStringList());
        QMetaObject::invokeMethod(item(v->panel, u"bluetoothPage"_s), "linkClicked");
        QCOMPARE(v->fixture->property("opened").toStringList(), QStringList{u"devices/bluetooth"_s});
    }

    void theVolumeArrowOpensTheOutputs()
    {
        auto v = view();
        QVERIFY(v);
        auto *row = item(v->panel, u"volumeRow"_s);
        QVERIFY(row);
        QMetaObject::invokeMethod(row, "arrowClicked");
        QCOMPARE(v->panel->property("pageId").toString(), u"sound"_s);
        QCOMPARE(v->fixture->property("opened").toStringList(), QStringList());
        QQuickItem *phones = nullptr;
        QTRY_VERIFY((phones = item(v->panel, u"output-1"_s)));
        QMetaObject::invokeMethod(phones, "clicked");
        QVERIFY(v->backend->property("calls").toStringList().contains(u"output:1"_s));
        QMetaObject::invokeMethod(item(v->panel, u"soundPage"_s), "linkClicked");
        QCOMPARE(v->fixture->property("opened").toStringList(), QStringList{u"system/sound"_s});
    }

    // a desktop PC without Wi-Fi, Bluetooth, airplane mode: gone, not grey
    void whatThePcCannotDoIsNotDrawn()
    {
        auto v = view();
        QVERIFY(v);
        // no Wi-Fi and no network port at all: the network tile goes too
        v->backend->setProperty("wifiAvailable", false);
        v->backend->setProperty("wiredAvailable", false);
        QTRY_VERIFY(!shownIds(v->panel).contains(u"wifi"_s));
        QVERIFY(!tile(v->panel, u"wifi"_s));
    }

    void noBatteryNoBatteryText()
    {
        auto v = view();
        QVERIFY(v);
        QQuickItem *battery = item(v->panel, u"batteryButton"_s);
        QVERIFY(battery);
        QVERIFY(battery->isVisible());
        v->backend->setProperty("hasBattery", false);
        QTRY_VERIFY(!battery->isVisible());
    }

    void noScreenControlNoBrightnessSlider()
    {
        auto v = view();
        QVERIFY(v);
        auto *row = item(v->panel, u"brightnessRow"_s);
        QVERIFY(row);
        QVERIFY(row->isVisible());
        const qreal tall = v->panel->height();
        v->backend->setProperty("brightnessAvailable", false);
        QTRY_VERIFY(!row->isVisible());
        QTRY_VERIFY(v->panel->height() < tall);
    }

    void theMediaCardComesAndGoesWithThePlayer()
    {
        auto v = view();
        QVERIFY(v);
        auto *card = item(v->panel, u"mediaCard"_s);
        QVERIFY(card);
        QVERIFY(card->isVisible());
        QMetaObject::invokeMethod(card, "playPause");
        QVERIFY(v->backend->property("calls").toStringList().contains(u"playpause"_s));
        v->backend->setProperty("hasPlayer", false);
        QTRY_VERIFY(!card->isVisible());
    }

    // Edit: taking a tile out and putting one in is saved, and survives
    void editSavesTheList()
    {
        auto v = view();
        QVERIFY(v);
        v->panel->setProperty("editing", true);
        QMetaObject::invokeMethod(v->panel, "removeTile", Q_ARG(QVariant, u"airplane"_s));
        QTRY_VERIFY(!shownIds(v->panel).contains(u"airplane"_s));
        QMetaObject::invokeMethod(v->panel, "addTile", Q_ARG(QVariant, u"project"_s));
        QTRY_VERIFY(shownIds(v->panel).contains(u"project"_s));
        v->panel->setProperty("editing", false);

        QFile rc(QDir::home().filePath(u".config/rayeshellrc"_s));
        QTRY_VERIFY(rc.exists());
        QVERIFY(rc.open(QIODevice::ReadOnly));
        const QString saved = QString::fromUtf8(rc.readAll());
        QVERIFY2(saved.contains(u"project"_s), qPrintable(saved));
        QVERIFY2(!saved.contains(u"airplane"_s), qPrintable(saved));
    }

    void aSeventhTileMakesASecondPage()
    {
        auto v = view();
        QVERIFY(v);
        QMetaObject::invokeMethod(v->panel, "addTile", Q_ARG(QVariant, u"dnd"_s));
        QTRY_COMPARE(v->panel->property("pageCount").toInt(), 2);
        auto *dots = item(v->panel, u"pageDots"_s);
        QVERIFY(dots);
        QVERIFY(dots->isVisible());
        QVERIFY(!tile(v->panel, u"dnd"_s));
        v->panel->setProperty("currentPage", 1);
        QTRY_VERIFY(tile(v->panel, u"dnd"_s));
    }

    // The project tile starts the Project panel, the VPN arrow opens its page
    void theProjectTileStartsTheProjectPanel()
    {
        auto v = view();
        QVERIFY(v);
        QMetaObject::invokeMethod(v->panel, "addTile", Q_ARG(QVariant, u"project"_s));
        v->panel->setProperty("currentPage", 1);
        QQuickItem *t = nullptr;
        QTRY_VERIFY((t = tile(v->panel, u"project"_s)));
        QMetaObject::invokeMethod(t, "toggled");
        QCOMPARE(v->fixture->property("launched").toStringList(), QStringList{u"org.raye.project.desktop"_s});
    }

    void theVpnPageListsAndSwitchesConnections()
    {
        auto v = view();
        QVERIFY(v);
        v->backend->setProperty("vpnAvailable", true);
        QVariantMap a{{u"name"_s, u"Work"_s}, {u"active"_s, true}};
        QVariantMap b{{u"name"_s, u"Home"_s}, {u"active"_s, false}};
        v->backend->setProperty("vpnConnections", QVariantList{a, b});
        QMetaObject::invokeMethod(v->panel, "openPage", Q_ARG(QVariant, u"vpn"_s));
        QQuickItem *row = nullptr;
        QTRY_VERIFY((row = item(v->panel, u"vpn-1"_s)));
        QMetaObject::invokeMethod(row, "clicked");
        QTRY_VERIFY(v->backend->property("calls").toStringList().contains(u"vpnconn:1:true"_s));
    }

    void theVolumeSliderAndTheMuteButtonReachTheService()
    {
        auto v = view();
        QVERIFY(v);
        auto *row = item(v->panel, u"volumeRow"_s);
        QVERIFY(row);
        QMetaObject::invokeMethod(row, "moved", Q_ARG(QVariant, 30));
        QCOMPARE(v->backend->property("volume").toInt(), 30);
        QMetaObject::invokeMethod(row, "iconClicked");
        QVERIFY(v->backend->property("muted").toBool());
    }

    // holding the slider is told to the backend, so the sound waits
    void theVolumeSliderIsHeld()
    {
        auto v = view();
        QVERIFY(v);
        auto *row = item(v->panel, u"volumeRow"_s);
        QVERIFY(row);
        QQuickItem *slider = nullptr;
        for (QQuickItem *c : row->childItems())
            if (QByteArray(c->metaObject()->className()).contains("RayeSlider"))
                slider = c;
        QVERIFY(slider);
        const QPoint p = slider->mapToScene(QPointF(slider->width() / 2, slider->height() / 2)).toPoint();
        QTest::mousePress(v->window, Qt::LeftButton, {}, p);
        QTRY_VERIFY(row->property("pressed").toBool());
        QVERIFY(v->backend->property("calls").toStringList().contains(u"holdvolume"_s));
        QVERIFY(!v->backend->property("calls").toStringList().contains(u"releasevolume"_s));
        QTest::mouseRelease(v->window, Qt::LeftButton, {}, p + QPoint(20, 0));
        QTRY_VERIFY(v->backend->property("calls").toStringList().contains(u"releasevolume"_s));
    }
};

QTEST_MAIN(TestQuickSettings)
#include "tst_quicksettings.moc"
