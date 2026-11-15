// Plasma desktop script run by raye-taskbar-sync: makes the taskbars
// match ~/.config/raye-taskbarrc, group [Taskbar]:
//
//   AllDisplays=true|false             (default true)
//   MultiDisplayButtons=all|main-and-window|window-only   (default all)
//
// The main taskbar is the user's own panel with the task manager on the
// main screen. With AllDisplays every other screen gets a copy of it:
// the same items in the same order (Start, search, the apps, the clock...)
// with the same settings, but no notification area: as in Windows, its
// icons are on the main taskbar only (and every tray item would be loaded
// once more for each screen). Only the buttons follow MultiDisplayButtons
// (raye-taskbar-sync also gives every stock Start button the main one's
// order of pinned apps; the Raye taskbar and Start read theirs from
// rayeshellrc). Those panels carry
// [Raye] Role=secondary in their containment settings, so they are the
// only ones this script adds or removes. Running it again changes nothing
// that already matches.

var STOCK_TASKS = ["org.kde.plasma.icontasks", "org.kde.plasma.taskmanager"];
var RAYE_TASKS = "org.raye.taskbar";
var TASKS = STOCK_TASKS.concat([RAYE_TASKS]);
var TRAY = "org.kde.plasma.systemtray";
// items of the main taskbar the copies do without
var NOT_ON_COPIES = [TRAY, "org.raye.quicksettings"];
// the Raye clock draws the toasts and has the bell: the tray's own
// notifications applet leaves the tray, or both would pop up. Without the
// Raye clock it comes back: it is the only one showing them then
var RAYE_CLOCK = "org.raye.clock";
var TRAY_NOTIFICATIONS = "org.kde.plasma.notifications";
var BELL_GROUP = ["Raye"];
var BELL_TAKEN = "BellTaken";
// the Raye USB icon takes the place of Plasma's "Disks & Devices"
var RAYE_DEVICES = "org.raye.devices";
var STOCK_DEVICES = "org.kde.plasma.devicenotifier";
// the Raye pill between the tray and the clock takes the place of these
// tray items: their services (network, sound keys, power) do not need them
var RAYE_QUICK_SETTINGS = "org.raye.quicksettings";
var QUICK_SETTINGS_REPLACES = ["org.kde.plasma.networkmanagement", "org.kde.plasma.volume",
                               "org.kde.plasma.battery", "org.kde.plasma.bluetooth",
                               "org.kde.plasma.brightness", "org.kde.plasma.mediacontroller"];
var ROLE_GROUP = ["Raye"];
var SECONDARY = "secondary";
// task manager settings the script decides itself, never copied
var OWN_TASK_KEYS = ["launchers", "showOnlyCurrentScreen", "rowWidth"];
var PANEL_PROPS = ["location", "height", "hiding", "floating", "alignment", "lengthMode", "opacity"];

function readSettings() {
    var cfg = ConfigFile("raye-taskbarrc", "Taskbar");
    var all = String(cfg.readEntry("AllDisplays") || "true").toLowerCase();
    var buttons = String(cfg.readEntry("MultiDisplayButtons") || "all").toLowerCase();
    if (buttons != "all" && buttons != "main-and-window" && buttons != "window-only")
        buttons = "all";
    return { allDisplays: all != "false" && all != "0" && all != "no", buttons: buttons };
}

function isSecondary(panel) {
    panel.currentConfigGroup = ROLE_GROUP;
    var role = panel.readConfig("Role", "");
    panel.currentConfigGroup = [];
    return String(role) == SECONDARY;
}

function taskWidgets(panel) {
    var out = [];
    for (var i = 0; i < TASKS.length; ++i) {
        var w = panel.widgets(TASKS[i]);
        for (var j = 0; j < w.length; ++j)
            out.push(w[j]);
    }
    return out;
}

// The screen a panel is on, or the one it comes back to when that screen
// is plugged in again.
function screenOf(panel) {
    if (panel.screen >= 0)
        return panel.screen;
    panel.currentConfigGroup = [];
    var last = parseInt(panel.readConfig("lastScreen", -1), 10);
    return isNaN(last) ? -1 : last;
}

// a list setting comes back as "a,b" (or a list): always hand lists over
// as lists, a string would be stored as a list of one item
function listOf(value) {
    if (value === undefined || value === null || value === "")
        return [];
    if (typeof value == "string")
        return value.split(",");
    var out = [];
    for (var i = 0; i < value.length; ++i)
        out.push(String(value[i]));
    return out;
}

// write only what differs, so a second run touches nothing
function setConfig(widget, group, key, value) {
    widget.currentConfigGroup = group;
    var old = widget.readConfig(key, "");
    if (old === undefined || old === null)
        old = "";
    if (String(old) != String(value))
        widget.writeConfig(key, value);
}

function copyGroup(from, to, group, skip) {
    if (!from || !to)
        return;
    from.currentConfigGroup = group;
    var keys = from.configKeys;
    for (var i = 0; i < keys.length; ++i) {
        if (skip && skip.indexOf(keys[i]) >= 0)
            continue;
        from.currentConfigGroup = group;
        setConfig(to, group, keys[i], from.readConfig(keys[i], ""));
    }
}

function firstWidget(panel, plugin) {
    var w = panel.widgets(plugin);
    return w.length > 0 ? w[0] : null;
}

// Plasma has nested the tray in two ways over time: its settings are on
// the widget itself, or on a containment it names.
function trayBox(tray) {
    if (!tray)
        return null;
    tray.currentConfigGroup = [];
    var id = tray.readConfig("SystrayContainmentId", "");
    if (id !== "" && id !== undefined && id !== null) {
        var box = desktopById(parseInt(id, 10));
        if (box)
            return box;
    }
    return tray;
}

function findMain(all) {
    var best = null;
    for (var i = 0; i < all.length; ++i) {
        var p = all[i];
        if (isSecondary(p) || taskWidgets(p).length == 0)
            continue;
        if (!best || (screenOf(p) == 0 && screenOf(best) != 0))
            best = p;
    }
    return best;
}

// The main taskbar's items, left to right (Plasma keeps that order in
// AppletOrder; the list of applets is in the order they were added).
function itemsOf(panel) {
    var w = panel.widgets();
    panel.currentConfigGroup = ["General"];
    var order = String(panel.readConfig("AppletOrder", "") || "").split(";");
    panel.currentConfigGroup = [];
    var out = [];
    for (var i = 0; i < order.length; ++i) {
        for (var j = 0; j < w.length; ++j) {
            if (String(w[j].id) == order[i] && out.indexOf(w[j]) < 0)
                out.push(w[j]);
        }
    }
    for (var k = 0; k < w.length; ++k) {
        if (out.indexOf(w[k]) < 0)
            out.push(w[k]);
    }
    return out;
}

function typesOf(items) {
    var t = [];
    for (var i = 0; i < items.length; ++i)
        t.push(String(items[i].type));
    return t;
}

function copyTypesOf(items) {
    return typesOf(items).filter(function (t) { return NOT_ON_COPIES.indexOf(t) < 0; });
}

// A copy whose items no longer match the main taskbar (an item added or
// removed there, or a copy made by an older version) is made again
// rather than patched: Plasma keeps the order applets were added in.
function sameItems(panel, main) {
    return typesOf(panel.widgets()).sort().join(",") == copyTypesOf(main.widgets()).sort().join(",");
}

function fillSecondary(panel, main) {
    var items = copyTypesOf(itemsOf(main));
    for (var i = 0; i < items.length; ++i)
        panel.addWidget(items[i]);
}

function matchMain(panel, main) {
    for (var i = 0; i < PANEL_PROPS.length; ++i) {
        var k = PANEL_PROPS[i];
        if (main[k] !== undefined && panel[k] != main[k])
            panel[k] = main[k];
    }
}

// An applet keeps a private copy of some files and deletes it with
// itself (the launcher icon's .desktop file): never share those.
var OWN_FILE_KEYS = ["localPath"];

// every setting of an applet, except what the script decides
function copyApplet(from, to, skip) {
    copyGroup(from, to, [], OWN_FILE_KEYS.concat(skip || []));
    from.currentConfigGroup = [];
    var groups = from.configGroups;
    for (var g = 0; g < groups.length; ++g)
        copyGroup(from, to, [groups[g]], skip);
}

function copyTray(from, to) {
    var mainBox = trayBox(from);
    var box = trayBox(to);
    if (!mainBox || !box || mainBox === box)
        return;
    var keys = ["hiddenItems", "shownItems", "extraItems", "showAllItems", "scaleIconsToFit"];
    for (var k = 0; k < keys.length; ++k) {
        mainBox.currentConfigGroup = ["General"];
        var v = mainBox.readConfig(keys[k], "");
        if (v !== undefined && v !== null && v !== "")
            setConfig(box, ["General"], keys[k], v);
    }
}

function configureSecondary(panel, main, settings) {
    matchMain(panel, main);

    // pair the items by kind: the n-th clock with the n-th clock...
    var mine = panel.widgets();
    var theirs = main.widgets();
    var used = [];
    for (var i = 0; i < mine.length; ++i) {
        var from = null;
        for (var j = 0; j < theirs.length && !from; ++j) {
            if (used.indexOf(j) < 0 && theirs[j].type == mine[i].type) {
                from = theirs[j];
                used.push(j);
            }
        }
        if (!from)
            continue;
        var type = String(mine[i].type);
        if (type == TRAY) {
            copyTray(from, mine[i]);
        } else if (type == RAYE_TASKS) {
            // its pins and window filter come from rayeshellrc
            copyApplet(from, mine[i], OWN_TASK_KEYS);
        } else if (STOCK_TASKS.indexOf(type) >= 0) {
            copyApplet(from, mine[i], OWN_TASK_KEYS);
            var launchers = [];
            if (settings.buttons == "all") {
                from.currentConfigGroup = ["General"];
                launchers = listOf(from.readConfig("launchers", ""));
            }
            setConfig(mine[i], ["General"], "launchers", launchers);
            setConfig(mine[i], ["General"], "showOnlyCurrentScreen", settings.buttons != "all");
        } else {
            copyApplet(from, mine[i], null);
        }
    }
}

function newSecondary(screen, main) {
    var p = new Panel;
    p.currentConfigGroup = ROLE_GROUP;
    p.writeConfig("Role", SECONDARY);
    p.currentConfigGroup = [];
    p.screen = screen;
    p.location = main.location;
    fillSecondary(p, main);
    return p;
}

// "preferred://browser" follows the default browser. When Firefox makes
// itself the default it registers a stand-in entry with no icon, and the
// pin turns into a blank page next to a second Firefox button. Pin
// Firefox itself instead: its windows then land on the pin.
var BROWSER_PIN = "preferred://browser";
var FIREFOX = "firefox.desktop";

function fixBrowserPin(widget) {
    widget.currentConfigGroup = ["General"];
    var launchers = String(widget.readConfig("launchers", "") || "");
    if (launchers.split(",").indexOf(BROWSER_PIN) < 0)
        return;
    var browser = String(defaultApplication("browser", true) || "");
    if (!/firefox/i.test(browser) || !applicationExists(FIREFOX))
        return;
    var out = [];
    var list = launchers.split(",");
    for (var i = 0; i < list.length; ++i) {
        var item = list[i] == BROWSER_PIN ? "applications:" + FIREFOX : list[i];
        if (out.indexOf(item) < 0)
            out.push(item);
    }
    setConfig(widget, ["General"], "launchers", out);
}

function removeFrom(box, key) {
    box.currentConfigGroup = ["General"];
    var items = listOf(box.readConfig(key, ""));
    var out = items.filter(function (t) { return t != TRAY_NOTIFICATIONS; });
    if (out.length != items.length)
        setConfig(box, ["General"], key, out);
}

function dropTrayBell(panel) {
    var trays = panel.widgets(TRAY);
    for (var i = 0; i < trays.length; ++i) {
        var box = trayBox(trays[i]);
        if (!box)
            continue;
        box.currentConfigGroup = ["General"];
        if (listOf(box.readConfig("extraItems", "")).indexOf(TRAY_NOTIFICATIONS) >= 0)
            setConfig(box, BELL_GROUP, BELL_TAKEN, "true");
        removeFrom(box, "extraItems");
        removeFrom(box, "hiddenItems");
        removeFrom(box, "shownItems");
    }
}

// only if this script took it: a tray can be without it by choice
function restoreTrayBell(panel) {
    var trays = panel.widgets(TRAY);
    for (var i = 0; i < trays.length; ++i) {
        var box = trayBox(trays[i]);
        if (!box)
            continue;
        box.currentConfigGroup = BELL_GROUP;
        if (String(box.readConfig(BELL_TAKEN, "")) != "true")
            continue;
        setConfig(box, BELL_GROUP, BELL_TAKEN, "");
        box.currentConfigGroup = ["General"];
        var items = listOf(box.readConfig("extraItems", ""));
        if (items.indexOf(TRAY_NOTIFICATIONS) < 0)
            setConfig(box, ["General"], "extraItems", items.concat([TRAY_NOTIFICATIONS]));
    }
}

// the Raye USB icon in the tray in place of the stock one, and the stock
// one back if the Raye one is gone
function useRayeDevices(panel) {
    if (typeof knownWidgetTypes == "undefined")
        return;
    var raye = knownWidgetTypes.indexOf(RAYE_DEVICES) >= 0;
    var want = raye ? RAYE_DEVICES : STOCK_DEVICES;
    var drop = raye ? STOCK_DEVICES : RAYE_DEVICES;
    var trays = panel.widgets(TRAY);
    for (var i = 0; i < trays.length; ++i) {
        var box = trayBox(trays[i]);
        if (!box)
            continue;
        box.currentConfigGroup = ["General"];
        var items = listOf(box.readConfig("extraItems", ""));
        // an empty list is the tray's own defaults: never written here
        if (items.length == 0)
            continue;
        if (!raye && items.indexOf(RAYE_DEVICES) < 0)
            continue;
        var out = items.filter(function (t) { return t != drop; });
        if (out.indexOf(want) < 0)
            out.push(want);
        if (out.join(",") != items.join(","))
            setConfig(box, ["General"], "extraItems", out);
    }
}

// Puts the pill just before the clock in the bar's saved order. Plasma
// lays a bar out from AppletOrder when it loads it, and the script API has
// no way to move a widget in a bar already on screen. So nothing is made
// again: at the first sign-in the bar is loaded after this script and
// comes up in this order; a bar already on screen takes it at the next
// sign-in. Destroying the clock to add it again behind the pill broke the
// first sign-in: Plasma deletes a removed widget later, the new bar loaded
// while the old clock was still listed and gave up half way, and the copy
// for the other screen got both clocks.
function pillBeforeClock(panel, pill, clock) {
    // any other pill is one this run dropped again: Plasma deletes it later
    var ids = itemsOf(panel).filter(function (w) { return String(w.type) != RAYE_QUICK_SETTINGS; })
                            .map(function (w) { return String(w.id); });
    var at = ids.indexOf(String(clock.id));
    if (at < 0)
        return;
    ids.splice(at, 0, String(pill.id));
    panel.currentConfigGroup = ["General"];
    var old = String(panel.readConfig("AppletOrder", "") || "");
    panel.currentConfigGroup = [];
    if (old == ids.join(";"))
        return;
    setConfig(panel, ["General"], "AppletOrder", ids.join(";"));
    panel.currentConfigGroup = [];
    // the bar reads its own copy of the setting, not the file
    if (panel.reloadConfig)
        panel.reloadConfig();
}

// whether a widget on screen comes after the given place on its bar
function shownAfter(widget, place) {
    var g = widget.geometry;
    return !!(g && g.width > 0 && (g.x - place.x) + (g.y - place.y) > 0);
}

// Only on a bar with the Raye clock. A person's first sign-in and an older
// account come through here alike: the default layout has no say in it.
function useRayeQuickSettings(panel) {
    if (typeof knownWidgetTypes == "undefined" || knownWidgetTypes.indexOf(RAYE_QUICK_SETTINGS) < 0)
        return;
    var clock = firstWidget(panel, RAYE_CLOCK);
    if (!clock)
        return;
    // as on Windows, the pill stands before the clock: dropped on the clock
    // when the bar is on screen, it goes in just before it
    var at = clock.geometry;
    var onScreen = at && at.width > 0;
    var pill = firstWidget(panel, RAYE_QUICK_SETTINGS);
    if (!pill) {
        pill = onScreen ? panel.addWidget(RAYE_QUICK_SETTINGS, at.x, at.y)
                        : panel.addWidget(RAYE_QUICK_SETTINGS);
    } else if (onScreen && shownAfter(pill, at)) {
        // A bar not laid out yet when the pill came (the first sign-in) put
        // it last. Once the bar is on screen and loaded, only the pill is
        // dropped again on the clock: it has nothing of its own to lose.
        var again = panel.addWidget(RAYE_QUICK_SETTINGS, at.x, at.y);
        if (again) {
            copyApplet(pill, again, null);
            pill.remove();
            pill = again;
        }
    }
    if (pill)
        pillBeforeClock(panel, pill, clock);
    var trays = panel.widgets(TRAY);
    for (var i = 0; i < trays.length; ++i) {
        var box = trayBox(trays[i]);
        if (!box)
            continue;
        var keys = ["extraItems", "hiddenItems", "shownItems"];
        var changed = false;
        for (var k = 0; k < keys.length; ++k) {
            box.currentConfigGroup = ["General"];
            var items = listOf(box.readConfig(keys[k], ""));
            var out = items.filter(function (t) { return QUICK_SETTINGS_REPLACES.indexOf(t) < 0; });
            // never an empty extraItems: that is the tray's own defaults again
            if (out.length != items.length && (out.length > 0 || keys[k] != "extraItems")) {
                setConfig(box, ["General"], keys[k], out);
                changed = true;
            }
        }
        if (changed && box.reloadConfig)
            box.reloadConfig();
    }
}

function sync() {
    var settings = readSettings();
    var all = panels();
    var main = findMain(all);
    if (!main) {
        print("raye-taskbar-sync: no taskbar with apps, nothing to copy");
        return { added: 0, removed: 0, starts: [] };
    }

    for (var b = 0; b < all.length; ++b) {
        if (!isSecondary(all[b])) {
            var own = taskWidgets(all[b]);
            for (var c = 0; c < own.length; ++c)
                fixBrowserPin(own[c]);
        }
    }

    // as on Windows, the taskbar with the notification area is on the main
    // display, Plasma's screen 0: Plasma puts a new panel where the pointer
    // is, which at the first sign-in was often the other screen
    if (main.screen > 0)
        main.screen = 0;

    // before the copies are matched: it is not one of the items they copy
    useRayeQuickSettings(main);

    var mainScreen = screenOf(main);
    if (mainScreen < 0)
        mainScreen = 0;

    // screens that want a secondary taskbar
    var wanted = {};
    if (settings.allDisplays) {
        for (var s = 0; s < screenCount; ++s)
            wanted[s] = s != mainScreen;
        // a screen where the user put a task manager panel of his own keeps it
        for (var i = 0; i < all.length; ++i) {
            var q = all[i];
            if (q !== main && !isSecondary(q) && taskWidgets(q).length > 0)
                wanted[screenOf(q)] = false;
        }
    }

    // one secondary per screen: the one on screen wins over one waiting
    // for its screen to come back; the rest go
    var kept = {};
    var removed = 0;
    var secondaries = [];
    for (var j = 0; j < all.length; ++j) {
        if (isSecondary(all[j]))
            secondaries.push(all[j]);
    }
    secondaries.sort(function (a, b) { return (b.screen >= 0 ? 1 : 0) - (a.screen >= 0 ? 1 : 0); });
    for (var m = 0; m < secondaries.length; ++m) {
        var p = secondaries[m];
        var sc = screenOf(p);
        var keep;
        if (!settings.allDisplays || sc < 0 || sc == mainScreen || kept[sc] || !sameItems(p, main))
            keep = false;
        else if (sc >= screenCount)
            keep = true; // unplugged: Plasma brings it back with its screen
        else
            keep = wanted[sc] === true;
        if (keep) {
            kept[sc] = p;
        } else {
            p.remove();
            ++removed;
        }
    }

    var added = 0;
    for (var n = 0; n < screenCount; ++n) {
        if (wanted[n] === true && !kept[n]) {
            kept[n] = newSecondary(n, main);
            ++added;
        }
    }

    // the main taskbar shows every window unless only the screen's own count
    for (var st = 0; st < STOCK_TASKS.length; ++st) {
        var mainTasks = main.widgets(STOCK_TASKS[st]);
        for (var t = 0; t < mainTasks.length; ++t)
            setConfig(mainTasks[t], ["General"], "showOnlyCurrentScreen", settings.buttons == "window-only");
    }

    var now = panels();
    var hasRayeClock = false;
    for (var c1 = 0; c1 < now.length; ++c1)
        hasRayeClock = hasRayeClock || now[c1].widgets(RAYE_CLOCK).length > 0;
    for (var a = 0; a < now.length; ++a) {
        if (hasRayeClock)
            dropTrayBell(now[a]);
        else
            restoreTrayBell(now[a]);
        useRayeDevices(now[a]);
    }

    for (var key in kept) {
        if (kept[key])
            configureSecondary(kept[key], main, settings);
    }
    // Start buttons, main first: the wrapper copies the pinned apps over
    var starts = [];
    var mainStart = firstWidget(main, "org.kde.plasma.kickoff");
    if (mainStart) {
        starts.push(mainStart.id);
        for (var k2 in kept) {
            var st = kept[k2] ? firstWidget(kept[k2], "org.kde.plasma.kickoff") : null;
            if (st)
                starts.push(st.id);
        }
    }
    return { added: added, removed: removed, starts: starts };
}

var rayeTaskbarResult = sync();
print("raye-taskbar-sync: added " + rayeTaskbarResult.added + ", removed " + rayeTaskbarResult.removed + "\n");
print("raye-taskbar-starts: " + rayeTaskbarResult.starts.join(" ") + "\n");
