// The desktop and taskbar menu.
//
// The menu model alone: which menu each containment gets, the items in
// Windows' order, what is ticked for the Folder view settings, what each
// item writes, which Raye Settings page it opens, and French. Then the
// plugin itself on real Plasma containments (a desktop Folder view and a
// panel, made by a small Corona): the actions it gives and the settings a
// click writes.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "menumodel.h"

#include <KConfigLoader>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <Plasma/Containment>
#include <Plasma/ContainmentActions>
#include <Plasma/Corona>

#include <QAction>
#include <QFile>
#include <QMenu>
#include <QRegularExpression>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace RayeMenu;

// just enough of a shell for containments to exist
class TestCorona : public Plasma::Corona
{
    Q_OBJECT

public:
    using Plasma::Corona::Corona;
    int numScreens() const override
    {
        return 1;
    }
    QRect screenGeometry(int) const override
    {
        return QRect(0, 0, 1920, 1080);
    }
};

static FolderSettings folder(int iconSize = 2, int sortMode = 0, int filterMode = 0, const QString &pattern = u"*"_s)
{
    return FolderSettings::fromMap({{u"iconSize"_s, iconSize}, {u"sortMode"_s, sortMode},
                                    {u"filterMode"_s, filterMode}, {u"filterPattern"_s, pattern}});
}

static const Entry *find(const QList<Entry> &list, const QString &id)
{
    for (const Entry &e : list) {
        if (e.id == id)
            return &e;
        if (const Entry *c = find(e.children, id))
            return c;
    }
    return nullptr;
}

static QStringList shape(const QList<Entry> &list)
{
    QStringList out;
    for (const Entry &e : list)
        out << (e.separator ? u"-"_s : e.id);
    return out;
}

static QStringList actionShape(const QList<QAction *> &list)
{
    QStringList out;
    for (QAction *a : list)
        out << (a->isSeparator() ? u"-"_s : a->objectName());
    return out;
}

class TestDesktopMenu : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // the French run is its own ctest (LANGUAGE=fr)
        m_french = qgetenv("LANGUAGE") == "fr";
    }

    void kinds()
    {
        // Desktop = 0, Panel = 1, Custom = 127, CustomPanel = 128, CustomEmbedded = 129
        // Planar = 0, MediaCenter = 1, Horizontal = 2, Vertical = 3, Application = 4
        QCOMPARE(kindFor(0, 0), Kind::Desktop);
        QCOMPARE(kindFor(0, 1), Kind::Desktop);
        QCOMPARE(kindFor(127, 0), Kind::Desktop);
        QCOMPARE(kindFor(1, 2), Kind::Panel);
        QCOMPARE(kindFor(1, 3), Kind::Panel);
        QCOMPARE(kindFor(128, 2), Kind::Panel);
        QCOMPARE(kindFor(0, 2), Kind::Panel);
        QCOMPARE(kindFor(129, 0), Kind::None);
        QCOMPARE(kindFor(-1, 0), Kind::None);
    }

    void desktopOrder()
    {
        if (m_french)
            QSKIP("English run");
        const auto list = entries(Kind::Desktop, folder());
        QCOMPARE(shape(list), (QStringList{u"view"_s, u"sort"_s, u"refresh"_s, u"-"_s, u"new"_s, u"-"_s,
                                           u"display-settings"_s, u"personalize"_s, u"-"_s, u"terminal"_s}));
        QCOMPARE(shape(find(list, u"view"_s)->children),
                 (QStringList{u"view-large"_s, u"view-medium"_s, u"view-small"_s, u"-"_s, u"auto-arrange"_s,
                              u"align-grid"_s, u"-"_s, u"show-icons"_s}));
        QCOMPARE(shape(find(list, u"sort"_s)->children),
                 (QStringList{u"sort-name"_s, u"sort-size"_s, u"sort-type"_s, u"sort-date"_s}));
        QCOMPARE(find(list, u"view"_s)->text, u"View"_s);
        QCOMPARE(find(list, u"sort-type"_s)->text, u"Item type"_s);
        QCOMPARE(find(list, u"personalize"_s)->text, u"Personalize"_s);
        QCOMPARE(find(list, u"terminal"_s)->text, u"Open in Terminal"_s);
        QVERIFY(find(list, u"new"_s)->newMenu);
        for (const Entry &e : list) {
            if (!e.separator)
                QVERIFY2(!e.icon.isEmpty(), qPrintable(e.id));
        }
    }

    void panelOrder()
    {
        if (m_french)
            QSKIP("English run");
        const auto list = entries(Kind::Panel, folder());
        QCOMPARE(shape(list), (QStringList{u"task-manager"_s, u"-"_s, u"taskbar-settings"_s}));
        QCOMPARE(list.first().text, u"Task Manager"_s);
        QCOMPARE(list.last().text, u"Taskbar settings"_s);
        QVERIFY(entries(Kind::None, folder()).isEmpty());
    }

    void noFolderView()
    {
        // a desktop that is not a Folder view: no View nor Sort by
        const auto list = entries(Kind::Desktop, FolderSettings::fromMap({}));
        QVERIFY(!find(list, u"view"_s));
        QVERIFY(!find(list, u"sort"_s));
        QVERIFY(find(list, u"refresh"_s));
        QVERIFY(changesFor(u"view-large"_s, FolderSettings::fromMap({})).isEmpty());
    }

    void ticks_data()
    {
        QTest::addColumn<int>("iconSize");
        QTest::addColumn<int>("sortMode");
        QTest::addColumn<QString>("size");
        QTest::addColumn<QString>("sort");
        QTest::addColumn<bool>("autoArrange");
        QTest::newRow("medium, name") << 2 << 0 << u"view-medium"_s << u"sort-name"_s << true;
        QTest::newRow("Plasma's 64 px default reads as medium") << 3 << 1 << u"view-medium"_s << u"sort-size"_s << true;
        QTest::newRow("small, date") << 1 << 2 << u"view-small"_s << u"sort-date"_s << true;
        QTest::newRow("22 px reads as small") << 0 << 6 << u"view-small"_s << u"sort-type"_s << true;
        QTest::newRow("large, manual") << 4 << -1 << u"view-large"_s << QString() << false;
        QTest::newRow("128 px reads as large") << 5 << 0 << u"view-large"_s << u"sort-name"_s << true;
    }

    void ticks()
    {
        QFETCH(int, iconSize);
        QFETCH(int, sortMode);
        QFETCH(QString, size);
        QFETCH(QString, sort);
        QFETCH(bool, autoArrange);
        const auto list = entries(Kind::Desktop, folder(iconSize, sortMode));
        for (const QString &id : {u"view-large"_s, u"view-medium"_s, u"view-small"_s})
            QCOMPARE(find(list, id)->checked, id == size);
        for (const QString &id : {u"sort-name"_s, u"sort-size"_s, u"sort-type"_s, u"sort-date"_s})
            QCOMPARE(find(list, id)->checked, id == sort);
        QCOMPARE(find(list, u"auto-arrange"_s)->checked, autoArrange);
        QVERIFY(find(list, u"view-large"_s)->exclusive);
        QVERIFY(find(list, u"sort-date"_s)->exclusive);
        QVERIFY(!find(list, u"auto-arrange"_s)->exclusive);
        // the desktop always aligns to its grid
        QVERIFY(find(list, u"align-grid"_s)->checked);
        QVERIFY(!find(list, u"align-grid"_s)->enabled);
    }

    void writes()
    {
        const auto f = folder(2, 0);
        QCOMPARE(changesFor(u"view-large"_s, f), (QVariantMap{{u"iconSize"_s, 4}}));
        QCOMPARE(changesFor(u"view-medium"_s, f), (QVariantMap{{u"iconSize"_s, 2}}));
        QCOMPARE(changesFor(u"view-small"_s, f), (QVariantMap{{u"iconSize"_s, 1}}));
        QCOMPARE(changesFor(u"sort-size"_s, f), (QVariantMap{{u"sortMode"_s, 1}, {u"positions"_s, QStringList()}}));
        QCOMPARE(changesFor(u"sort-type"_s, f), (QVariantMap{{u"sortMode"_s, 6}, {u"positions"_s, QStringList()}}));
        QCOMPARE(changesFor(u"sort-date"_s, f), (QVariantMap{{u"sortMode"_s, 2}, {u"positions"_s, QStringList()}}));
        QCOMPARE(changesFor(u"sort-name"_s, f), (QVariantMap{{u"sortMode"_s, 0}, {u"positions"_s, QStringList()}}));
        // auto arrange off keeps icons where they are dragged; on sorts them again
        QCOMPARE(changesFor(u"auto-arrange"_s, f), (QVariantMap{{u"sortMode"_s, -1}}));
        QCOMPARE(changesFor(u"auto-arrange"_s, folder(2, -1)),
                 (QVariantMap{{u"sortMode"_s, 0}, {u"positions"_s, QStringList()}}));
        // hiding icons is a filter that lets nothing through, and back
        QCOMPARE(changesFor(u"show-icons"_s, f), (QVariantMap{{u"filterMode"_s, 2}, {u"filterPattern"_s, u"*"_s}}));
        const auto hidden = folder(2, 0, 2, u"*"_s);
        QVERIFY(!find(entries(Kind::Desktop, hidden), u"show-icons"_s)->checked);
        QCOMPARE(changesFor(u"show-icons"_s, hidden), (QVariantMap{{u"filterMode"_s, 0}, {u"filterPattern"_s, u"*"_s}}));
        // a user's own filter is not "icons hidden"
        QVERIFY(find(entries(Kind::Desktop, folder(2, 0, 2, u"*.tmp"_s)), u"show-icons"_s)->checked);
        // the rest writes nothing
        for (const QString &id : {u"refresh"_s, u"new"_s, u"terminal"_s, u"display-settings"_s, u"align-grid"_s})
            QVERIFY2(changesFor(id, f).isEmpty(), qPrintable(id));
    }

    void pages()
    {
        QCOMPARE(settingsPageFor(u"display-settings"_s), u"system/display"_s);
        QCOMPARE(settingsPageFor(u"personalize"_s), u"personalization/background"_s);
        QCOMPARE(settingsPageFor(u"taskbar-settings"_s), u"personalization/taskbar"_s);
        QVERIFY(settingsPageFor(u"refresh"_s).isEmpty());
    }

    void french()
    {
        if (!m_french)
            QSKIP("French runs in desktopmenu-fr");
        const auto list = entries(Kind::Desktop, folder());
        QCOMPARE(find(list, u"view"_s)->text, u"Affichage"_s);
        QCOMPARE(find(list, u"sort"_s)->text, u"Trier par"_s);
        QCOMPARE(find(list, u"refresh"_s)->text, u"Actualiser"_s);
        QCOMPARE(find(list, u"new"_s)->text, u"Nouveau"_s);
        QCOMPARE(find(list, u"terminal"_s)->text, u"Ouvrir dans le Terminal"_s);
        QCOMPARE(entries(Kind::Panel, folder()).first().text, u"Gestionnaire des tâches"_s);

        // every string of the sources has its French, different from the English
        QFile po(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_desktopmenu.po"));
        QVERIFY(po.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(po.readAll());
        QFile src(QStringLiteral(RAYE_DESKTOPMENU_SRC "/menumodel.cpp"));
        QVERIFY(src.open(QIODevice::ReadOnly));
        const QString source = QString::fromUtf8(src.readAll());
        static const QRegularExpression call(u"i18n\\(\"([^\"]+)\"\\)"_s);
        int count = 0;
        for (auto it = call.globalMatch(source); it.hasNext(); ++count) {
            const QString id = it.next().captured(1);
            const QRegularExpression entry(u"msgid \"%1\"\\nmsgstr \"([^\"]+)\""_s.arg(QRegularExpression::escape(id)));
            const auto m = entry.match(text);
            QVERIFY2(m.hasMatch(), qPrintable(id));
            QVERIFY2(m.captured(1) != id, qPrintable(id));
        }
        QCOMPARE(count, 19);
    }

    // --- the plugin on real containments --------------------------------------

    void pluginOnDesktop()
    {
        if (m_french)
            QSKIP("English run");
        auto plugin = loadPlugin();
        QVERIFY(plugin);
        TestCorona corona;
        Plasma::Containment *desk = corona.createContainment(u"org.kde.desktopcontainment"_s);
        QVERIFY(desk);
        QVERIFY(desk->configScheme());
        plugin->setContainment(desk);

        // start from Raye's layout: medium icons, sorted by name
        QVERIFY(QMetaObject::invokeMethod(plugin.get(), "applyFolderValues", Q_ARG(QVariantMap, (QVariantMap{{u"iconSize"_s, 2}, {u"sortMode"_s, 0}}))));
        QList<QAction *> actions = plugin->contextualActions();
        QCOMPARE(actionShape(actions), (QStringList{u"view"_s, u"sort"_s, u"refresh"_s, u"-"_s, u"new"_s, u"-"_s,
                                                    u"display-settings"_s, u"personalize"_s, u"-"_s, u"terminal"_s}));
        QAction *view = actions.at(0);
        QVERIFY(view->menu());
        QAction *large = view->menu()->findChild<QAction *>(u"view-large"_s);
        if (!large) {
            for (QAction *a : view->menu()->actions()) {
                if (a->objectName() == u"view-large"_s)
                    large = a;
            }
        }
        QVERIFY(large);
        QVERIFY(!large->isChecked());
        large->trigger();
        QCOMPARE(desk->configScheme()->findItemByName(u"iconSize"_s)->property().toInt(), 4);

        // a new menu sees the new value
        actions = plugin->contextualActions();
        QAction *sort = actions.at(1);
        QAction *date = nullptr;
        QAction *medium = nullptr;
        for (QAction *a : sort->menu()->actions()) {
            if (a->objectName() == u"sort-date"_s)
                date = a;
        }
        for (QAction *a : actions.at(0)->menu()->actions()) {
            if (a->objectName() == u"view-large"_s)
                QVERIFY(a->isChecked());
            if (a->objectName() == u"view-medium"_s)
                medium = a;
        }
        QVERIFY(date && medium);
        QVERIFY(!medium->isChecked());
        date->trigger();
        QCOMPARE(desk->configScheme()->findItemByName(u"sortMode"_s)->property().toInt(), 2);

        // Settings pages travel with their action
        QCOMPARE(actions.at(6)->data().toString(), u"system/display"_s);
        QCOMPARE(actions.at(7)->data().toString(), u"personalization/background"_s);
        // "New" is KIO's own menu, pointed at the desktop
        QVERIFY(actions.at(4)->menu());
    }

    // Ctrl + wheel on the desktop, the Windows habit the owner asked for
    void wheelResizesTheDesktopIcons()
    {
        if (m_french)
            QSKIP("English run");
        auto plugin = loadPlugin();
        QVERIFY(plugin);
        TestCorona corona;
        Plasma::Containment *desk = corona.createContainment(u"org.kde.desktopcontainment"_s);
        QVERIFY(desk);
        QVERIFY(desk->configScheme());
        plugin->setContainment(desk);
        auto size = [desk] {
            return desk->configScheme()->findItemByName(u"iconSize"_s)->property().toInt();
        };

        QVERIFY(QMetaObject::invokeMethod(plugin.get(), "applyFolderValues",
                                          Q_ARG(QVariantMap, (QVariantMap{{u"iconSize"_s, MediumIcons}}))));
        QCOMPARE(size(), MediumIcons);

        // The direction is MEASURED, not guessed: in libPlasmaQuick 6.6.6,
        // ContainmentItem::wheelEvent calls performPreviousAction on a
        // scroll up and performNextAction on a scroll down. Swapping these
        // two lines would ship icons that shrink when you scroll up.
        plugin->performPreviousAction(); // wheel up
        QCOMPARE(size(), MediumIcons + 1);
        plugin->performNextAction(); // wheel down
        QCOMPARE(size(), MediumIcons);
        plugin->performNextAction();
        QCOMPARE(size(), MediumIcons - 1);

        // it stops at both ends instead of wrapping round
        for (int i = 0; i < 12; ++i)
            plugin->performNextAction();
        QCOMPARE(size(), SmallestIcons);
        for (int i = 0; i < 12; ++i)
            plugin->performPreviousAction();
        QCOMPARE(size(), BiggestIcons);

        // every stop the View menu offers is reachable with the wheel
        QVERIFY(SmallIcons >= SmallestIcons && LargeIcons <= BiggestIcons);

        // and the View menu reads the size the wheel left behind
        QVERIFY(QMetaObject::invokeMethod(plugin.get(), "applyFolderValues",
                                          Q_ARG(QVariantMap, (QVariantMap{{u"iconSize"_s, SmallIcons}, {u"sortMode"_s, 0}}))));
        plugin->performPreviousAction();
        const QList<QAction *> actions = plugin->contextualActions();
        for (QAction *a : actions.at(0)->menu()->actions()) {
            if (a->objectName() == u"view-medium"_s)
                QVERIFY(a->isChecked());
        }
    }

    void wheelDoesNothingWhereThereAreNoIcons()
    {
        if (m_french)
            QSKIP("English run");
        auto plugin = loadPlugin();
        QVERIFY(plugin);
        TestCorona corona;
        Plasma::Containment *panel = corona.createContainment(u"org.kde.panel"_s);
        QVERIFY(panel);
        panel->setFormFactor(Plasma::Types::Horizontal);
        plugin->setContainment(panel);
        // a panel has no Folder view settings: a no-op, never a crash
        plugin->performNextAction();
        plugin->performPreviousAction();
        QVERIFY(!panel->configScheme() || !panel->configScheme()->findItemByName(u"iconSize"_s));
    }

    void wheelBeforeAnyContainment()
    {
        if (m_french)
            QSKIP("English run");
        auto plugin = loadPlugin();
        QVERIFY(plugin);
        // Plasma may call these before setContainment: it must not crash
        plugin->performNextAction();
        plugin->performPreviousAction();
    }

    void pluginOnPanel()
    {
        if (m_french)
            QSKIP("English run");
        auto plugin = loadPlugin();
        QVERIFY(plugin);
        TestCorona corona;
        Plasma::Containment *panel = corona.createContainment(u"org.kde.panel"_s);
        QVERIFY(panel);
        panel->setFormFactor(Plasma::Types::Horizontal);
        plugin->setContainment(panel);
        const QList<QAction *> actions = plugin->contextualActions();
        QCOMPARE(actionShape(actions), (QStringList{u"task-manager"_s, u"-"_s, u"taskbar-settings"_s}));
        QCOMPARE(actions.last()->data().toString(), u"personalization/taskbar"_s);
    }

private:
    bool m_french = false;

    static std::unique_ptr<Plasma::ContainmentActions> loadPlugin()
    {
        const KPluginMetaData md(QStringLiteral(RAYE_DESKTOPMENU_PLUGIN));
        if (!md.isValid() || md.pluginId() != u"org.raye.desktopmenu"_s) {
            qWarning() << "bad plugin metadata" << md.fileName() << md.pluginId();
            return nullptr;
        }
        auto result = KPluginFactory::instantiatePlugin<Plasma::ContainmentActions>(md, nullptr);
        if (!result) {
            qWarning() << result.errorString;
            return nullptr;
        }
        return std::unique_ptr<Plasma::ContainmentActions>(result.plugin);
    }
};

QTEST_MAIN(TestDesktopMenu)
#include "tst_desktopmenu.moc"
