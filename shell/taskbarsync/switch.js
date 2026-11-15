// Plasma desktop script run by raye-taskbar-switch: moves the user's
// taskbar from Plasma's stock applets (Application Launcher, Icons-only
// Task Manager, Digital Clock) to Raye's own (org.raye.start,
// org.raye.taskbar, org.raye.clock).
//
// Run with RAYE_SWITCH_MODE = "read" first: it prints the pinned apps,
// the alignment and the Start button's id of the main taskbar. With
// "start" (and RAYE_START_ORDER, the stock Start menu's order of
// favourites) it prints those favourites as desktop ids. The wrapper
// writes all of it to rayeshellrc. Then with "apply": each of the user's own taskbars (never
// a copy raye-taskbar-sync made, it makes those again) is replaced by a
// new one with the Raye items, keeping its screen, size, auto-hide and
// the tray's settings.

var STOCK_TASKS = ["org.kde.plasma.icontasks", "org.kde.plasma.taskmanager"];
var TRAY = "org.kde.plasma.systemtray";
var KEEP_PANEL = ["location", "height", "hiding", "floating", "alignment", "lengthMode", "opacity"];
var TRAY_KEYS = ["hiddenItems", "shownItems", "extraItems", "showAllItems", "scaleIconsToFit"];

function isCopy(panel) {
    panel.currentConfigGroup = ["Raye"];
    var role = panel.readConfig("Role", "");
    panel.currentConfigGroup = [];
    return String(role) == "secondary";
}

function stockTasks(panel) {
    for (var i = 0; i < STOCK_TASKS.length; ++i) {
        var w = panel.widgets(STOCK_TASKS[i]);
        if (w.length > 0)
            return w[0];
    }
    return null;
}

function screenOf(panel) {
    if (panel.screen >= 0)
        return panel.screen;
    panel.currentConfigGroup = [];
    var last = parseInt(panel.readConfig("lastScreen", -1), 10);
    return isNaN(last) ? -1 : last;
}

function userTaskbars() {
    var out = [];
    var all = panels();
    for (var i = 0; i < all.length; ++i) {
        if (!isCopy(all[i]) && stockTasks(all[i]))
            out.push(all[i]);
    }
    return out;
}

function mainOf(list) {
    var best = null;
    for (var i = 0; i < list.length; ++i) {
        if (!best || (screenOf(list[i]) == 0 && screenOf(best) != 0))
            best = list[i];
    }
    return best;
}

function isDesktopId(id) {
    return /^[A-Za-z0-9][A-Za-z0-9._+-]*\.desktop$/.test(id) && id.length <= 255;
}

function launchersOf(tasks) {
    tasks.currentConfigGroup = ["General"];
    var value = tasks.readConfig("launchers", "");
    return typeof value == "string" ? value.split(",") : value;
}

// "applications:x.desktop,..." -> ["x.desktop", ...]; "default browser"
// becomes Firefox when Firefox is the browser and is installed. With
// installedOnly, entries of apps that are gone are dropped.
function idsOf(list, installedOnly) {
    var browser = String(defaultApplication("browser", true) || "");
    var browserId = /firefox/i.test(browser) && applicationExists("firefox.desktop") ? "firefox.desktop" : "";
    var out = [];
    for (var i = 0; list && i < list.length; ++i) {
        var s = String(list[i]).replace(/^\s+|\s+$/g, "");
        var q = s.indexOf("?");
        if (q >= 0)
            s = s.substring(0, q);
        var id;
        if (s == "preferred://browser")
            id = browserId;
        else if (s.indexOf("applications:") == 0)
            id = s.substring("applications:".length);
        else if (s.indexOf("file://") == 0 || s.indexOf("/") == 0)
            id = s.substring(s.lastIndexOf("/") + 1);
        else if (s.indexOf(":") < 0 && s !== "")
            id = /\.desktop$/.test(s) ? s : s + ".desktop";
        else
            id = "";
        if (!isDesktopId(id) || out.indexOf(id) >= 0)
            continue;
        if (installedOnly && !applicationExists(id))
            continue;
        out.push(id);
    }
    return out;
}

// centred when an expanding spacer comes before the Start button
function alignmentOf(panel) {
    var w = panel.widgets();
    panel.currentConfigGroup = ["General"];
    var order = String(panel.readConfig("AppletOrder", "") || "").split(";");
    panel.currentConfigGroup = [];
    for (var i = 0; i < order.length; ++i) {
        for (var j = 0; j < w.length; ++j) {
            if (String(w[j].id) != order[i])
                continue;
            if (w[j].type == "org.kde.plasma.panelspacer")
                return "center";
            if (w[j].type == "org.kde.plasma.kickoff")
                return "left";
        }
    }
    return "center";
}

function trayBox(tray) {
    tray.currentConfigGroup = [];
    var id = tray.readConfig("SystrayContainmentId", "");
    if (id !== "" && id !== undefined && id !== null) {
        var box = desktopById(parseInt(id, 10));
        if (box)
            return box;
    }
    return tray;
}

function replace(old) {
    var p = new Panel;
    p.screen = screenOf(old);
    for (var i = 0; i < KEEP_PANEL.length; ++i) {
        var k = KEEP_PANEL[i];
        if (old[k] !== undefined)
            p[k] = old[k];
    }
    p.addWidget("org.raye.start");
    p.addWidget("org.raye.taskbar");
    var oldTrays = old.widgets(TRAY);
    if (oldTrays.length > 0) {
        var tray = p.addWidget(TRAY);
        var from = trayBox(oldTrays[0]);
        var to = trayBox(tray);
        for (var t = 0; t < TRAY_KEYS.length; ++t) {
            from.currentConfigGroup = ["General"];
            var v = from.readConfig(TRAY_KEYS[t], "");
            if (v !== undefined && v !== null && v !== "") {
                to.currentConfigGroup = ["General"];
                to.writeConfig(TRAY_KEYS[t], v);
            }
        }
    }
    p.addWidget("org.raye.clock");
    old.remove();
    return p;
}

function rayeSwitch(mode) {
    if (mode == "start") {
        var order = typeof RAYE_START_ORDER == "string" ? RAYE_START_ORDER.split(",") : [];
        return "raye-start-pins: " + idsOf(order, true).join(",");
    }
    var bars = userTaskbars();
    var main = mainOf(bars);
    if (!main)
        return "raye-taskbar-switch: nothing to switch";
    if (mode == "read") {
        var starts = main.widgets("org.kde.plasma.kickoff");
        return "raye-taskbar-pins: " + idsOf(launchersOf(stockTasks(main)), false).join(",")
            + "\nraye-taskbar-alignment: " + alignmentOf(main)
            + "\nraye-taskbar-start: " + (starts.length > 0 ? starts[0].id : "");
    }
    var missing = ["org.raye.start", "org.raye.taskbar", "org.raye.clock"].filter(function (t) {
        return knownWidgetTypes.indexOf(t) < 0;
    });
    if (missing.length > 0)
        return "raye-taskbar-switch: not installed: " + missing.join(" ");
    // the copies on other screens go too: raye-taskbar-sync makes them again
    var all = panels();
    for (var c = 0; c < all.length; ++c) {
        if (isCopy(all[c]))
            all[c].remove();
    }
    for (var i = 0; i < bars.length; ++i)
        replace(bars[i]);
    return "raye-taskbar-switch: switched " + bars.length;
}

print(rayeSwitch(typeof RAYE_SWITCH_MODE == "string" ? RAYE_SWITCH_MODE : "read") + "\n");
