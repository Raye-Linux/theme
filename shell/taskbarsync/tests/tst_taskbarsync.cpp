// sync.js against a stand-in of the Plasma scripting API (mock.js): which
// taskbars it adds, keeps and removes, and what it writes in them.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QJSEngine>
#include <QTest>

using namespace Qt::StringLiterals;

static const QString QS_SCENE = u"knownWidgetTypes.push('org.raye.quicksettings'); var m = __rayeTaskbar();"
                                "m.applets.forEach(function (w) { w.geometry = m.geometryOf(w); });"_s;

class TaskbarSyncTest : public QObject
{
    Q_OBJECT

private:
    static QString read(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            qFatal("cannot read %s", qPrintable(path));
        return QString::fromUtf8(f.readAll());
    }

    // mock, then the scene, then sync.js `runs` times; returns the value of `check`
    static QString run(const QString &scene, const QString &check, int runs = 1)
    {
        QJSEngine engine;
        const QString mockPath = QStringLiteral(RAYE_TASKBARSYNC_DIR "/tests/mock.js");
        const QString syncPath = QStringLiteral(RAYE_TASKBARSYNC_DIR "/sync.js");
        QJSValue r = engine.evaluate(read(mockPath), mockPath);
        if (r.isError())
            return QStringLiteral("mock: ") + r.toString();
        r = engine.evaluate(scene);
        if (r.isError())
            return QStringLiteral("scene: ") + r.toString();
        const QString sync = read(syncPath);
        for (int i = 0; i < runs; ++i) {
            // Plasma deletes what a run removed before the next run comes
            if (i > 0)
                engine.evaluate(QStringLiteral("__settle();"));
            if (i == runs - 1)
                engine.evaluate(QStringLiteral("__log = [];"));
            r = engine.evaluate(sync, syncPath);
            if (r.isError())
                return QStringLiteral("sync: ") + r.toString() + QStringLiteral(" line ") + r.property(QStringLiteral("lineNumber")).toString();
        }
        r = engine.evaluate(check);
        if (r.isError())
            return QStringLiteral("check: ") + r.toString();
        return r.toString();
    }

    // the same for switch.js, in a mode ("read" or "apply")
    static QString runSwitch(const QString &scene, const QString &mode, const QString &check)
    {
        QJSEngine engine;
        const QString mockPath = QStringLiteral(RAYE_TASKBARSYNC_DIR "/tests/mock.js");
        const QString switchPath = QStringLiteral(RAYE_TASKBARSYNC_DIR "/switch.js");
        QJSValue r = engine.evaluate(read(mockPath), mockPath);
        if (r.isError())
            return QStringLiteral("mock: ") + r.toString();
        r = engine.evaluate(scene + QStringLiteral("; var RAYE_SWITCH_MODE = '") + mode + QStringLiteral("';"));
        if (r.isError())
            return QStringLiteral("scene: ") + r.toString();
        r = engine.evaluate(read(switchPath), switchPath);
        if (r.isError())
            return QStringLiteral("switch: ") + r.toString() + QStringLiteral(" line ") + r.property(QStringLiteral("lineNumber")).toString();
        r = engine.evaluate(check);
        if (r.isError())
            return QStringLiteral("check: ") + r.toString();
        return r.toString();
    }

private Q_SLOTS:
    void oneScreenAddsNothing()
    {
        QCOMPARE(run(u"var m = __mainTaskbar();"_s,
                     u"__panels.length + ' ' + __secondaries().length + ' ' + __tasks(m).get('General','showOnlyCurrentScreen')"_s),
                 u"1 0 false"_s);
    }

    void secondScreenGetsATaskbar()
    {
        const QString scene = u"screenCount = 2; var m = __mainTaskbar(); m.height = 52; m.hiding = 'autohide';"_s;
        QCOMPARE(run(scene, u"var s = __secondaries(); s.length + ' ' + s[0].screen + ' ' + s[0].location"_s), u"1 1 bottom"_s);
        // Start, the apps and the clock, in that order; no tray (Windows
        // shows its icons on the main taskbar only)
        QCOMPARE(run(scene, u"__secondaries()[0].applets.map(function (a) { return a.plugin.split('.').pop(); }).join(',')"_s),
                 u"panelspacer,kickoff,icontasks,panelspacer,digitalclock,showdesktop"_s);
        // the main taskbar's look
        QCOMPARE(run(scene, u"var s = __secondaries()[0]; s.height + ' ' + s.hiding"_s), u"52 autohide"_s);
        // pins and task manager settings copied, every window shown
        QCOMPARE(run(scene, u"var t = __tasks(__secondaries()[0]); t.get('General','launchers') == __tasks(m).get('General','launchers')"_s),
                 u"true"_s);
        QCOMPARE(run(scene, u"var t = __tasks(__secondaries()[0]); t.get('General','iconSpacing') + t.get('General','maxStripes') + ' ' + t.get('General','showOnlyCurrentScreen')"_s),
                 u"11 false"_s);
        // the clock like the main one
        QCOMPARE(run(scene, u"__secondaries()[0].widgets('org.kde.plasma.digitalclock')[0].get('Appearance','dateFormat')"_s), u"shortDate"_s);
    }

    void threeScreensGetTwo()
    {
        QCOMPARE(run(u"screenCount = 3; __mainTaskbar();"_s,
                     u"__secondaries().map(function (p) { return p.screen; }).sort().join(',')"_s),
                 u"1,2"_s);
    }

    void secondRunChangesNothing()
    {
        QCOMPARE(run(u"screenCount = 2; __mainTaskbar();"_s, u"__log.length + ' ' + __panels.length"_s, 2), u"0 2"_s);
        QCOMPARE(run(u"screenCount = 3; __files['raye-taskbarrc'] = {Taskbar: {MultiDisplayButtons: 'window-only'}}; __mainTaskbar();"_s,
                     u"__log.join(';')"_s, 3),
                 QString());
    }

    void allDisplaysOffRemovesThem()
    {
        QCOMPARE(run(u"screenCount = 2; __mainTaskbar(); var s = __secondary(1);"
                     "__files['raye-taskbarrc'] = {Taskbar: {AllDisplays: 'false'}};"_s,
                     u"__panels.length + ' ' + s.removed"_s),
                 u"1 true"_s);
    }

    void buttonsOnWindowScreenOnly()
    {
        const QString scene = u"screenCount = 2; var m = __mainTaskbar(); __files['raye-taskbarrc'] = {Taskbar: {MultiDisplayButtons: 'window-only'}};"_s;
        QCOMPARE(run(scene, u"var t = __tasks(__secondaries()[0]); __tasks(m).get('General','showOnlyCurrentScreen') + ' ' + t.get('General','showOnlyCurrentScreen') + ' [' + (t.get('General','launchers') || '') + ']'"_s),
                 u"true true []"_s);
    }

    void buttonsOnMainAndWindowScreen()
    {
        const QString scene = u"screenCount = 2; var m = __mainTaskbar(); __files['raye-taskbarrc'] = {Taskbar: {MultiDisplayButtons: 'main-and-window'}};"_s;
        QCOMPARE(run(scene, u"var t = __tasks(__secondaries()[0]); __tasks(m).get('General','showOnlyCurrentScreen') + ' ' + t.get('General','showOnlyCurrentScreen') + ' [' + (t.get('General','launchers') || '') + ']'"_s),
                 u"false true []"_s);
    }

    void switchingBackToAllRestoresPins()
    {
        QCOMPARE(run(u"screenCount = 2; var m = __mainTaskbar(); var s = __secondary(1);"
                     "var t = __tasks(s); t.set('General','launchers',''); t.set('General','showOnlyCurrentScreen','true');"_s,
                     u"__panels.length + ' ' + t.get('General','showOnlyCurrentScreen') + ' ' + (t.get('General','launchers') == __tasks(m).get('General','launchers'))"_s),
                 u"2 false true"_s);
    }

    void unknownValuesFallBackToDefaults()
    {
        QCOMPARE(run(u"screenCount = 2; var m = __mainTaskbar(); __files['raye-taskbarrc'] = {Taskbar: {AllDisplays: 'maybe', MultiDisplayButtons: 'odd'}};"_s,
                     u"__secondaries().length + ' ' + __tasks(__secondaries()[0]).get('General','showOnlyCurrentScreen')"_s),
                 u"1 false"_s);
    }

    void unpluggedScreenKeepsItsTaskbar()
    {
        // screen 1 gone: Plasma brings its panel back when it returns
        QCOMPARE(run(u"screenCount = 1; __mainTaskbar(); var s = __secondary(-1); s.set('', 'lastScreen', 1);"_s,
                     u"__panels.length + ' ' + s.removed"_s),
                 u"2 false"_s);
    }

    void onlyOneTaskbarPerScreen()
    {
        QCOMPARE(run(u"screenCount = 2; __mainTaskbar(); var a = __secondary(-1); a.set('', 'lastScreen', 1);"
                     "var b = __secondary(1);"_s,
                     u"__panels.length + ' ' + a.removed + ' ' + b.removed"_s),
                 u"2 true false"_s);
    }

    void mainTaskbarGoesToTheMainDisplay()
    {
        // made on screen 1 (where the pointer was), screen 0 had only a copy
        QCOMPARE(run(u"screenCount = 2; var m = __mainTaskbar(1); var a = __secondary(0);"_s,
                     u"m.screen + ' ' + a.removed + ' ' + __secondaries().length + ' ' + __secondaries()[0].screen"_s),
                 u"0 true 1 1"_s);
        // a main taskbar waiting for its unplugged screen stays as it is
        QCOMPARE(run(u"screenCount = 1; var m = __mainTaskbar(-1); m.set('', 'lastScreen', 1);"_s, u"m.screen"_s), u"-1"_s);
    }

    void secondaryOnMainScreenGoes()
    {
        QCOMPARE(run(u"screenCount = 2; __mainTaskbar(); var a = __secondary(0);"_s,
                     u"a.removed + ' ' + __secondaries().length + ' ' + __secondaries()[0].screen"_s),
                 u"true 1 1"_s);
    }

    void ownPanelOnAScreenIsLeftAlone()
    {
        QCOMPARE(run(u"screenCount = 2; __mainTaskbar(); var own = __panel(1); own.addWidget('org.kde.plasma.taskmanager');"_s,
                     u"__panels.length + ' ' + __secondaries().length + ' ' + own.removed"_s),
                 u"2 0 false"_s);
    }

    void panelsWithoutAppsAreNotTouched()
    {
        // a top bar of the user's own, and no task manager anywhere
        QCOMPARE(run(u"screenCount = 2; var top = __panel(0); top.location = 'top'; top.addWidget('org.kde.plasma.digitalclock');"_s,
                     u"__panels.length + ' ' + top.removed"_s),
                 u"1 false"_s);
    }

    void secondaryStartIsTheMainOne()
    {
        const QString scene = u"screenCount = 3; var m = __mainTaskbar();"_s;
        QCOMPARE(run(scene, u"var k = __secondaries()[0].widgets('org.kde.plasma.kickoff')[0]; k.get('General','icon') + ' ' + k.get('General','favoritesPortedToKAstats')"_s),
                 u"raye-eye true"_s);
        // the wrapper is told every Start button, the main one first
        QCOMPARE(run(scene, u"var ids = [m.widgets('org.kde.plasma.kickoff')[0].id].concat(__secondaries().map(function (p) { return p.widgets('org.kde.plasma.kickoff')[0].id; }));"
                            "__printed.join('').indexOf('raye-taskbar-starts: ' + ids.join(' ') + '\\n') >= 0"_s),
                 u"true"_s);
    }

    void olderSecondaryIsMadeAgain()
    {
        // made before Start was on every taskbar: apps, tray and clock only
        QCOMPARE(run(u"screenCount = 2; __mainTaskbar(); var old = __panel(1); old.set('Raye','Role','secondary');"
                     "['org.kde.plasma.panelspacer','org.kde.plasma.icontasks','org.kde.plasma.panelspacer','org.kde.plasma.systemtray',"
                     "'org.kde.plasma.digitalclock','org.kde.plasma.showdesktop'].forEach(function (w) { old.addWidget(w); });"_s,
                     u"old.removed + ' ' + __secondaries().length + ' ' + __secondaries()[0].widgets('org.kde.plasma.kickoff').length + ' ' + __secondaries()[0].screen"_s),
                 u"true 1 1 1"_s);
    }

    void copyFollowsTheMainTaskbar()
    {
        // search and task view added to the main taskbar, the task list
        // with labels: the copy is made again with the same items, in order
        const QString scene = u"screenCount = 2; var m = __mainTaskbar(); var old = __secondary(1);"
                              "var k = m.widgets('org.kde.plasma.icontasks')[0]; m.applets.splice(m.applets.indexOf(k), 1);"
                              "var t = m.addWidget('org.kde.plasma.taskmanager'); t.set('General','launchers','applications:firefox.desktop');"
                              "var search = m.addWidget('org.kde.milou'); search.set('General','mode','box');"
                              "m.set('General','AppletOrder', [m.applets[0], m.applets[1], search, t].concat(m.applets.slice(2, 6)).map(function (a) { return a.id; }).join(';'));"_s;
        QCOMPARE(run(scene, u"old.removed + ' ' + __secondaries()[0].applets.map(function (a) { return a.plugin.split('.').pop(); }).join(',')"_s),
                 u"true panelspacer,kickoff,milou,taskmanager,panelspacer,digitalclock,showdesktop"_s);
        QCOMPARE(run(scene, u"__secondaries()[0].widgets('org.kde.milou')[0].get('General','mode') + ' ' + __secondaries()[0].widgets('org.kde.plasma.taskmanager')[0].get('General','launchers')"_s),
                 u"box applications:firefox.desktop"_s);
        QCOMPARE(run(scene, u"__log.join(';')"_s, 2), QString());
    }

    void copyWithATrayIsMadeAgain()
    {
        // made when copies still had a tray: made again without it
        QCOMPARE(run(u"screenCount = 2; __mainTaskbar(); var old = __panel(1); old.set('Raye','Role','secondary');"
                     "['org.kde.plasma.panelspacer','org.kde.plasma.kickoff','org.kde.plasma.icontasks','org.kde.plasma.panelspacer',"
                     "'org.kde.plasma.systemtray','org.kde.plasma.digitalclock','org.kde.plasma.showdesktop'].forEach(function (w) { old.addWidget(w); });"_s,
                     u"old.removed + ' ' + __secondaries().length + ' ' + __secondaries()[0].widgets('org.kde.plasma.systemtray').length"_s),
                 u"true 1 0"_s);
    }

    void rayeTaskbarIsCopied()
    {
        const QString scene = u"screenCount = 2; var m = __rayeTaskbar();"_s;
        QCOMPARE(run(scene, u"__secondaries()[0].applets.map(function (a) { return a.plugin; }).join(',')"_s),
                 u"org.raye.start,org.raye.taskbar,org.raye.clock,org.kde.plasma.showdesktop"_s);
        // the Raye task list keeps its own pins and filter (rayeshellrc):
        // the script writes neither, nor the width it publishes
        QCOMPARE(run(scene, u"var t = __secondaries()[0].widgets('org.raye.taskbar')[0]; [t.get('General','launchers'), t.get('General','showOnlyCurrentScreen'), t.get('General','rowWidth'), m.widgets('org.raye.taskbar')[0].get('General','showOnlyCurrentScreen')].map(String).join(' ')"_s),
                 u"undefined undefined undefined undefined"_s);
        QCOMPARE(run(scene, u"__log.join(';')"_s, 2), QString());
    }

    void rayeClockTakesTheBellOutOfTheTray()
    {
        // the clock draws the toasts: the stock applet leaves the tray,
        // or every notification pops up twice
        QCOMPARE(run(u"var m = __rayeTaskbar();"_s,
                     u"var t = m.widgets('org.kde.plasma.systemtray')[0]; '[' + t.get('General','hiddenItems') + '] ' + t.get('General','extraItems')"_s),
                 u"[undefined] org.kde.plasma.volume,org.kde.plasma.battery"_s);
        QCOMPARE(run(u"var m = __rayeTaskbar(); var t = m.widgets('org.kde.plasma.systemtray')[0]; t.set('General','hiddenItems','org.kde.kscreen,org.kde.plasma.notifications'); t.set('General','shownItems','org.kde.plasma.notifications,a');"_s,
                     u"var t = m.widgets('org.kde.plasma.systemtray')[0]; t.get('General','hiddenItems') + ' ' + t.get('General','shownItems')"_s),
                 u"org.kde.kscreen a"_s);
        // a second run changes nothing
        QCOMPARE(run(u"__rayeTaskbar();"_s, u"__log.join(';')"_s, 2), QString());
        // the stock clock keeps the tray as it is
        QCOMPARE(run(u"var m = __mainTaskbar();"_s, u"m.widgets('org.kde.plasma.systemtray')[0].get('General','hiddenItems')"_s),
                 u"org.kde.kscreen,org.kde.plasma.clipboard"_s);
        // and it comes back when the Raye clock is gone, if it was taken here
        const QString taken = u"var m = __mainTaskbar(); var t = m.widgets('org.kde.plasma.systemtray')[0]; t.set('General','extraItems','org.kde.plasma.volume'); t.set('Raye','BellTaken','true');"_s;
        QCOMPARE(run(taken, u"m.widgets('org.kde.plasma.systemtray')[0].get('General','extraItems')"_s),
                 u"org.kde.plasma.volume,org.kde.plasma.notifications"_s);
        const QString byChoice = u"var m = __mainTaskbar(); var t = m.widgets('org.kde.plasma.systemtray')[0]; t.set('General','extraItems','org.kde.plasma.volume');"_s;
        QCOMPARE(run(byChoice, u"m.widgets('org.kde.plasma.systemtray')[0].get('General','extraItems')"_s),
                 u"org.kde.plasma.volume"_s);
    }

    void switchReadsPinsAndAlignment()
    {
        // the owner's taskbar: centred, Firefox pinned as "default browser"
        QCOMPARE(runSwitch(u"var m = __mainTaskbar();"_s, u"read"_s, u"__printed.join('')"_s),
                 u"raye-taskbar-pins: org.kde.dolphin.desktop,firefox.desktop,org.raye.store.desktop\nraye-taskbar-alignment: center\n"
                 "raye-taskbar-start: " + QString::number(3) + u"\n"_s);
        // Start first: left aligned
        QCOMPARE(runSwitch(u"var m = __mainTaskbar(); m.set('General','AppletOrder', [m.applets[1].id, m.applets[2].id, m.applets[0].id].join(';'));"_s,
                           u"read"_s, u"__printed.join('').split('\\n')[1]"_s),
                 u"raye-taskbar-alignment: left"_s);
        // another browser: the browser pin is dropped, never a blank one
        QCOMPARE(runSwitch(u"var m = __mainTaskbar(); __defaultBrowser = 'google-chrome.desktop';"_s, u"read"_s,
                           u"__printed.join('').split('\\n')[0]"_s),
                 u"raye-taskbar-pins: org.kde.dolphin.desktop,org.raye.store.desktop"_s);
        // nothing changed by reading
        QCOMPARE(runSwitch(u"var m = __mainTaskbar();"_s, u"read"_s, u"__log.join(';')"_s), QString());
    }

    void switchReadsStartFavourites()
    {
        // the stock Start's order of favourites, apps that are gone dropped
        QCOMPARE(runSwitch(u"var RAYE_START_ORDER = 'preferred://browser,applications:org.kde.dolphin.desktop,"
                           "applications:org.kde.discover.desktop,org.kde.konsole,applications:org.kde.dolphin.desktop';"
                           "__apps.push('org.kde.konsole.desktop');"_s,
                           u"start"_s, u"__printed.join('')"_s),
                 u"raye-start-pins: firefox.desktop,org.kde.dolphin.desktop,org.kde.konsole.desktop\n"_s);
    }

    void switchMakesTheRayeTaskbar()
    {
        const QString scene = u"screenCount = 2; var m = __mainTaskbar(); m.height = 52; m.hiding = 'autohide'; var s = __secondary(1);"_s;
        QCOMPARE(runSwitch(scene, u"apply"_s,
                           u"[m.removed, s.removed, __panels.length].join(' ') + ' ' + __panels[0].applets.map(function (a) { return a.plugin; }).join(',')"_s),
                 u"true true 1 org.raye.start,org.raye.taskbar,org.kde.plasma.systemtray,org.raye.clock"_s);
        // same screen, size, auto-hide; the tray keeps its settings
        QCOMPARE(runSwitch(scene, u"apply"_s,
                           u"var p = __panels[0]; [p.screen, p.height, p.hiding, p.widgets('org.kde.plasma.systemtray')[0].get('General','hiddenItems')].join(' ')"_s),
                 u"0 52 autohide org.kde.kscreen,org.kde.plasma.clipboard"_s);
        QCOMPARE(runSwitch(scene, u"apply"_s, u"__printed.join('')"_s), u"raye-taskbar-switch: switched 1\n"_s);
    }

    void switchNeedsTheRayeApplets()
    {
        QCOMPARE(runSwitch(u"var m = __mainTaskbar(); knownWidgetTypes = ['org.raye.taskbar'];"_s, u"apply"_s,
                           u"__printed.join('') + m.removed + ' ' + __log.length"_s),
                 u"raye-taskbar-switch: not installed: org.raye.start org.raye.clock\nfalse 0"_s);
        // a Raye taskbar already: nothing to do
        QCOMPARE(runSwitch(u"__rayeTaskbar();"_s, u"apply"_s, u"__printed.join('') + __log.length"_s),
                 u"raye-taskbar-switch: nothing to switch\n0"_s);
    }

    void rayeDevicesTakeThePlaceOfTheStockOne()
    {
        const QString scene = u"knownWidgetTypes.push('org.raye.devices'); var m = __mainTaskbar();"
                              "m.widgets('org.kde.plasma.systemtray')[0].set('General','extraItems','org.kde.plasma.volume,org.kde.plasma.devicenotifier');"_s;
        QCOMPARE(run(scene, u"m.widgets('org.kde.plasma.systemtray')[0].get('General','extraItems')"_s),
                 u"org.kde.plasma.volume,org.raye.devices"_s);
        QCOMPARE(run(scene, u"__log.join(';')"_s, 2), QString());
        // not installed: the stock one stays
        QCOMPARE(run(u"var m = __mainTaskbar(); m.widgets('org.kde.plasma.systemtray')[0].set('General','extraItems','org.kde.plasma.devicenotifier');"_s,
                     u"m.widgets('org.kde.plasma.systemtray')[0].get('General','extraItems')"_s),
                 u"org.kde.plasma.devicenotifier"_s);
        // the Raye one removed again: the stock one comes back
        QCOMPARE(run(u"var m = __mainTaskbar(); m.widgets('org.kde.plasma.systemtray')[0].set('General','extraItems','org.kde.plasma.volume,org.raye.devices');"_s,
                     u"m.widgets('org.kde.plasma.systemtray')[0].get('General','extraItems')"_s),
                 u"org.kde.plasma.volume,org.kde.plasma.devicenotifier"_s);
    }

    // Quick settings takes network, sound, battery... out of the tray and
    // sits between the tray and the clock
    void quickSettingsGoesBetweenTheTrayAndTheClock()
    {
        QCOMPARE(run(QS_SCENE, u"__shown(m)"_s),
                 u"org.raye.start,org.raye.taskbar,org.kde.plasma.systemtray,org.raye.quicksettings,org.raye.clock,org.kde.plasma.showdesktop"_s);
    }

    void quickSettingsTakesItsItemsOutOfTheTray()
    {
        const QString scene = QS_SCENE
            + u"m.widgets('org.kde.plasma.systemtray')[0].set('General','extraItems','org.kde.plasma.networkmanagement,org.kde.plasma.volume,org.kde.plasma.battery,org.kde.plasma.bluetooth,org.kde.plasma.brightness,org.kde.plasma.mediacontroller,org.kde.plasma.clipboard');"
              "m.widgets('org.kde.plasma.systemtray')[0].set('General','hiddenItems','org.kde.plasma.brightness,org.kde.plasma.clipboard');"_s;
        QCOMPARE(run(scene, u"m.widgets('org.kde.plasma.systemtray')[0].get('General','extraItems')"_s),
                 u"org.kde.plasma.clipboard"_s);
        QCOMPARE(run(scene, u"m.widgets('org.kde.plasma.systemtray')[0].get('General','hiddenItems')"_s),
                 u"org.kde.plasma.clipboard"_s);
    }

    // A bar already on screen takes the pill where it is dropped, on the
    // clock: it shows before the clock at once, not only at the next sign-in
    void quickSettingsIsDroppedBeforeTheClockOnABarOnScreen()
    {
        QCOMPARE(run(QS_SCENE, u"m.widgets().map(function (w) { return w.plugin; }).join(',')"_s),
                 u"org.raye.start,org.raye.taskbar,org.kde.plasma.systemtray,org.raye.quicksettings,org.raye.clock,org.kde.plasma.showdesktop"_s);
    }

    // The first sign-in: the bar was not laid out when the pill came, so it
    // shows last. On the next run, the bar on screen, only the pill is
    // dropped again on the clock; the clock and the rest stay as they are.
    void quickSettingsShownAfterTheClockIsDroppedAgainBeforeIt()
    {
        const QString scene = QS_SCENE
            + u"var c = m.widgets('org.raye.clock')[0];"
              "var q = m.addWidget('org.raye.quicksettings'); q.set('General','x','1');"
              "m.applets.forEach(function (w) { w.geometry = m.geometryOf(w); });"_s;
        const QString live = u"m.widgets().filter(function (w) { return !w.destroyed; }).map(function (w) { return w.plugin; }).join(',')"_s;
        QCOMPARE(run(scene, live),
                 u"org.raye.start,org.raye.taskbar,org.kde.plasma.systemtray,org.raye.quicksettings,org.raye.clock,org.kde.plasma.showdesktop"_s);
        QCOMPARE(run(scene, u"var p = m.widgets('org.raye.quicksettings').filter(function (w) { return !w.destroyed; });"
                            "p.length + ' ' + (p[0] !== q) + ' ' + p[0].get('General','x') + ' ' + (m.widgets('org.raye.clock')[0] === c)"_s),
                 u"1 true 1 true"_s);
        QCOMPARE(run(scene, u"__log.filter(function (l) { return l.split('|')[1] == 'remove'; }).join(';')"_s),
                 QString::number(1) + u"|remove|org.raye.quicksettings"_s);
        QCOMPARE(run(scene, live, 2),
                 u"org.raye.start,org.raye.taskbar,org.kde.plasma.systemtray,org.raye.quicksettings,org.raye.clock,org.kde.plasma.showdesktop"_s);
        QCOMPARE(run(scene, u"__log.join(';')"_s, 2), QString());
    }

    // At the first sign-in the bar is new: it is loaded after the script,
    // from AppletOrder. Windows has the pill before the clock.
    void quickSettingsGoesBeforeTheClockOnANewBar()
    {
        const QString scene = QS_SCENE + u"m.dropIgnored = true;"_s;
        QCOMPARE(run(scene, u"__shown(m)"_s),
                 u"org.raye.start,org.raye.taskbar,org.kde.plasma.systemtray,org.raye.quicksettings,org.raye.clock,org.kde.plasma.showdesktop"_s);
        QCOMPARE(run(scene, u"__shown(m)"_s, 2),
                 u"org.raye.start,org.raye.taskbar,org.kde.plasma.systemtray,org.raye.quicksettings,org.raye.clock,org.kde.plasma.showdesktop"_s);
    }

    // The first sign-in with two screens. Making the clock again left the
    // old one listed until Plasma deleted it: the new bar loaded with it and
    // stopped half way (8 px long, LayoutManager.js "Value is null"), and the
    // copy on the other screen got two clocks. Nothing is made again now.
    void firstSignInWithTwoScreensMakesNothingAgain()
    {
        const QString scene = QS_SCENE + u"screenCount = 2;"_s;
        QCOMPARE(run(scene, u"__log.filter(function (l) { return / remove/.test(l.replace('|', ' ')); }).join(';')"_s), QString());
        QCOMPARE(run(scene, u"__shown(m)"_s),
                 u"org.raye.start,org.raye.taskbar,org.kde.plasma.systemtray,org.raye.quicksettings,org.raye.clock,org.kde.plasma.showdesktop"_s);
        QCOMPARE(run(scene, u"__shown(__secondaries()[0])"_s),
                 u"org.raye.start,org.raye.taskbar,org.raye.clock,org.kde.plasma.showdesktop"_s);
        QCOMPARE(run(scene, u"__shown(__secondaries()[0])"_s, 2),
                 u"org.raye.start,org.raye.taskbar,org.raye.clock,org.kde.plasma.showdesktop"_s);
    }

    // An account made by the image of 2026-11-08 has the pill after the
    // clock: it moves in front of it in the saved order. The clock is the
    // same widget, so it keeps its id and every setting.
    void quickSettingsAfterTheClockMovesInFrontOfIt()
    {
        const QString scene = QS_SCENE
            + u"var c = m.widgets('org.raye.clock')[0]; c.set('General','focusMinutes','45'); c.set('General/Popup','x','1');"
              "m.addWidget('org.raye.quicksettings');"
              "m.set('General','AppletOrder', m.applets.map(function (a) { return a.id; }).join(';'));"_s;
        QCOMPARE(run(scene, u"__shown(m)"_s),
                 u"org.raye.start,org.raye.taskbar,org.kde.plasma.systemtray,org.raye.quicksettings,org.raye.clock,org.kde.plasma.showdesktop"_s);
        QCOMPARE(run(scene, u"var k = m.widgets('org.raye.clock'); (k[0] === c) + ' ' + k[0].get('General','focusMinutes') + ' ' + k[0].get('General/Popup','x')"_s),
                 u"true 45 1"_s);
        QCOMPARE(run(scene, u"m.widgets('org.raye.clock').length + ' ' + m.widgets('org.raye.quicksettings').length"_s), u"1 1"_s);
        // the bar re-reads the setting it lays itself out from
        QCOMPARE(run(scene, u"__log.filter(function (l) { return l == m.id + '|reload'; }).length"_s), u"1"_s);
    }

    // A launcher icon behind the clock stays the same widget: Plasma
    // deletes a launcher's private file with the widget, so it must not be
    // made again.
    void quickSettingsLeavesALauncherAlone()
    {
        const QString scene = QS_SCENE
            + u"var tv = m.addWidget('org.kde.plasma.icon');"
              "tv.set('', 'url', 'file:///usr/share/applications/org.raye.taskview.desktop');"
              "tv.set('', 'localPath', '/home/u/.local/share/plasma_icons/org.raye.taskview.desktop');"_s;
        QCOMPARE(run(scene, u"var c = m.widgets('org.kde.plasma.icon'); c.length + ' ' + (c[0] === tv) + ' ' + c[0].get('', 'localPath')"_s),
                 u"1 true /home/u/.local/share/plasma_icons/org.raye.taskview.desktop"_s);
        QCOMPARE(run(scene, u"__shown(m)"_s),
                 u"org.raye.start,org.raye.taskbar,org.kde.plasma.systemtray,org.raye.quicksettings,org.raye.clock,org.kde.plasma.showdesktop,org.kde.plasma.icon"_s);
    }

    void quickSettingsIsAddedOnceAndASecondRunChangesNothing()
    {
        QCOMPARE(run(QS_SCENE, u"m.widgets('org.raye.quicksettings').length"_s, 2), u"1"_s);
        QCOMPARE(run(QS_SCENE, u"__log.join(';')"_s, 2), QString());
    }

    // one panel for the session: the copies on other screens have the clock
    void quickSettingsIsNotCopiedToTheOtherScreens()
    {
        const QString scene = QS_SCENE + u"screenCount = 2;"_s;
        QCOMPARE(run(scene, u"panels().length"_s), u"2"_s);
        QCOMPARE(run(scene, u"panels().filter(function (p) { return p !== m; })[0].widgets('org.raye.quicksettings').length"_s), u"0"_s);
        QCOMPARE(run(scene, u"panels().filter(function (p) { return p !== m; })[0].widgets('org.raye.clock').length"_s), u"1"_s);
    }

    // the applet not installed, or a bar without the Raye clock: nothing moves
    void withoutTheAppletTheTrayKeepsItsItems()
    {
        const QString scene = u"var m = __rayeTaskbar();"_s;
        QCOMPARE(run(scene, u"m.widgets('org.kde.plasma.systemtray')[0].get('General','extraItems')"_s),
                 u"org.kde.plasma.volume,org.kde.plasma.battery"_s);
        QCOMPARE(run(scene, u"m.widgets('org.raye.quicksettings').length"_s), u"0"_s);
    }

    void aStockBarDoesNotGetQuickSettings()
    {
        const QString scene = u"knownWidgetTypes.push('org.raye.quicksettings'); var m = __mainTaskbar();"_s;
        QCOMPARE(run(scene, u"m.widgets('org.raye.quicksettings').length"_s), u"0"_s);
    }

    void launcherIconKeepsItsOwnFile()
    {
        // Task view is a launcher icon: same target, its own copy of the file
        const QString scene = u"screenCount = 2; var m = __mainTaskbar(); var tv = m.addWidget('org.kde.plasma.icon');"
                              "tv.set('', 'url', 'file:///usr/share/applications/org.raye.taskview.desktop');"
                              "tv.set('', 'localPath', '/home/u/.local/share/plasma_icons/org.raye.taskview.desktop');"_s;
        QCOMPARE(run(scene, u"var c = __secondaries()[0].widgets('org.kde.plasma.icon')[0]; c.get('', 'url') + ' ' + c.get('', 'localPath')"_s),
                 u"file:///usr/share/applications/org.raye.taskview.desktop undefined"_s);
    }

    void firefoxPinReplacesBrowserPin()
    {
        QCOMPARE(run(u"var m = __mainTaskbar(); __defaultBrowser = 'userapp-Firefox-W0ZBY3.desktop';"_s,
                     u"__tasks(m).get('General','launchers')"_s),
                 u"applications:org.kde.dolphin.desktop,applications:firefox.desktop,applications:org.raye.store.desktop"_s);
    }

    void firefoxPinIsNotDoubled()
    {
        QCOMPARE(run(u"var m = __mainTaskbar(); __tasks(m).set('General','launchers','preferred://browser,applications:firefox.desktop');"_s,
                     u"__tasks(m).get('General','launchers')"_s),
                 u"applications:firefox.desktop"_s);
    }

    void otherBrowserKeepsTheBrowserPin()
    {
        QCOMPARE(run(u"var m = __mainTaskbar(); __defaultBrowser = 'google-chrome.desktop';"_s,
                     u"__tasks(m).get('General','launchers').split(',')[1]"_s),
                 u"preferred://browser"_s);
        QCOMPARE(run(u"var m = __mainTaskbar(); __apps = [];"_s, u"__tasks(m).get('General','launchers').split(',')[1]"_s),
                 u"preferred://browser"_s);
    }
};

QTEST_GUILESS_MAIN(TaskbarSyncTest)
#include "tst_taskbarsync.moc"
