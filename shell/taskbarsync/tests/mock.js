// A small stand-in for the Plasma desktop scripting API, as much of it as
// sync.js uses. Values are kept as strings, the way KConfig hands them back.

var __files = {};
var __log = [];
var __nextId = 1;
var __panels = [];
var __printed = [];
var __defaultBrowser = "firefox.desktop";
var __apps = ["firefox.desktop", "org.kde.dolphin.desktop"];
var screenCount = 1;
var knownWidgetTypes = ["org.kde.plasma.kickoff", "org.kde.plasma.icontasks", "org.kde.plasma.systemtray",
                        "org.kde.plasma.digitalclock", "org.raye.start", "org.raye.taskbar", "org.raye.clock"];

function print(text) { __printed.push(String(text)); }

function ConfigFile(file, group) {
    return {
        readEntry: function (key) {
            var g = (__files[file] || {})[group] || {};
            return g[key];
        }
    };
}

function defaultApplication(name, storageId) { return name == "browser" ? __defaultBrowser : ""; }
function applicationExists(name) { return __apps.indexOf(name) >= 0; }

function __widget(plugin) {
    return {
        id: __nextId++,
        plugin: plugin,
        type: plugin,
        cfg: {},
        currentConfigGroup: [],
        readConfig: function (key, def) {
            var g = this.cfg[this.currentConfigGroup.join("/")] || {};
            return key in g ? g[key] : def;
        },
        writeConfig: function (key, value) {
            var path = this.currentConfigGroup.join("/");
            if (!this.cfg[path])
                this.cfg[path] = {};
            this.cfg[path][key] = String(value);
            __log.push([this.id, path, key, String(value)].join("|"));
        },
        get configGroups() {
            var prefix = this.currentConfigGroup.join("/");
            var out = [];
            for (var path in this.cfg) {
                var rest = prefix === "" ? path : (path.indexOf(prefix + "/") == 0 ? path.substr(prefix.length + 1) : "");
                var top = rest.split("/")[0];
                if (top !== "" && out.indexOf(top) < 0)
                    out.push(top);
            }
            return out;
        },
        get configKeys() {
            return Object.keys(this.cfg[this.currentConfigGroup.join("/")] || {});
        },
        // test helper: write without logging
        set: function (path, key, value) {
            if (!this.cfg[path])
                this.cfg[path] = {};
            this.cfg[path][key] = String(value);
        },
        get: function (path, key) { return (this.cfg[path] || {})[key]; }
    };
}

function __panel(screen) {
    var p = __widget("org.kde.panel");
    // a widget's place in the bar, as its index: enough to drop one before it
    p.geometryOf = function (w) { return { x: this.applets.indexOf(w), y: 0, width: 1, height: 1 }; };
    p.applets = [];
    p.screen = screen === undefined ? 0 : screen;
    p.location = "bottom";
    p.height = 48;
    p.hiding = "none";
    p.floating = false;
    p.alignment = "center";
    p.lengthMode = "fill";
    p.opacity = "adaptive";
    p.removed = false;
    p.reloadConfig = function () { __log.push(this.id + "|reload"); };
    p.widgets = function (plugin) {
        return this.applets.filter(function (a) { return !plugin || a.plugin == plugin; });
    };
    // with x: where it is dropped, as Plasma does, before what stands there
    // a bar not laid out yet, where Plasma cannot tell what stands there:
    // what is dropped goes at the end
    p.dropIgnored = false;
    p.addWidget = function (plugin, x) {
        var w = __widget(plugin);
        var panel = this;
        // as in Plasma, a removed widget is only deleted once the script
        // has returned: until then the bar still lists it
        w.remove = function () {
            w.destroyed = true;
            __doomed.push([panel, w]);
            __log.push(panel.id + "|remove|" + plugin);
        };
        if (!this.dropIgnored && x !== undefined && x >= 0 && x < this.applets.length)
            this.applets.splice(x, 0, w);
        else
            this.applets.push(w);
        __log.push(this.id + "|add|" + plugin);
        return w;
    };
    p.remove = function () {
        this.removed = true;
        var i = __panels.indexOf(this);
        if (i >= 0)
            __panels.splice(i, 1);
        __log.push(this.id + "|remove");
    };
    p.set("", "lastScreen", p.screen);
    __panels.push(p);
    return p;
}

function Panel() {
    var p = __panel(0);
    __log.push(p.id + "|new");
    return p;
}

function panels() { return __panels.slice(); }

// the widgets removed by the script are deleted, as when Plasma gets back
// to its event loop
var __doomed = [];
function __settle() {
    for (var i = 0; i < __doomed.length; ++i) {
        var apps = __doomed[i][0].applets;
        var at = apps.indexOf(__doomed[i][1]);
        if (at >= 0)
            apps.splice(at, 1);
    }
    __doomed = [];
}

// What a bar shows once Plasma loads it: the widgets named in AppletOrder
// in that order, then the others in the order they were added. A widget
// removed but not deleted yet is what made a new bar give up loading.
function __shown(p) {
    var order = String(p.get("General", "AppletOrder") || "").split(";");
    var out = [];
    for (var i = 0; i < order.length; ++i) {
        for (var j = 0; j < p.applets.length; ++j) {
            if (String(p.applets[j].id) == order[i] && out.indexOf(p.applets[j]) < 0)
                out.push(p.applets[j]);
        }
    }
    for (var k = 0; k < p.applets.length; ++k) {
        if (out.indexOf(p.applets[k]) < 0)
            out.push(p.applets[k]);
    }
    for (var d = 0; d < out.length; ++d) {
        if (out[d].destroyed)
            return "a removed widget is still there: " + out[d].plugin;
    }
    return out.map(function (w) { return w.plugin; }).join(",");
}

function desktopById(id) {
    for (var i = 0; i < __panels.length; ++i) {
        var apps = __panels[i].applets;
        for (var j = 0; j < apps.length; ++j) {
            if (apps[j].id == id)
                return apps[j];
        }
    }
    return null;
}

// the taskbar the Raye layout makes
function __mainTaskbar(screen) {
    var p = __panel(screen === undefined ? 0 : screen);
    p.addWidget("org.kde.plasma.panelspacer");
    var start = p.addWidget("org.kde.plasma.kickoff");
    start.set("General", "icon", "raye-eye");
    start.set("General", "favoritesPortedToKAstats", "true");
    var t = p.addWidget("org.kde.plasma.icontasks");
    t.set("General", "launchers", "applications:org.kde.dolphin.desktop,preferred://browser,applications:org.raye.store.desktop");
    t.set("General", "iconSpacing", "1");
    t.set("General", "maxStripes", "1");
    p.addWidget("org.kde.plasma.panelspacer");
    var tray = p.addWidget("org.kde.plasma.systemtray");
    tray.set("General", "hiddenItems", "org.kde.kscreen,org.kde.plasma.clipboard");
    tray.set("General", "extraItems", "org.kde.plasma.volume,org.kde.plasma.battery");
    var clock = p.addWidget("org.kde.plasma.digitalclock");
    clock.set("Appearance", "showDate", "true");
    clock.set("Appearance", "dateFormat", "shortDate");
    p.addWidget("org.kde.plasma.showdesktop");
    __log = [];
    return p;
}

function __secondaries() {
    return __panels.filter(function (p) { return p.get("Raye", "Role") == "secondary"; });
}

// a secondary taskbar as raye-taskbar-sync makes it
function __secondary(screen) {
    var p = __panel(screen);
    p.set("Raye", "Role", "secondary");
    var plugins = ["org.kde.plasma.panelspacer", "org.kde.plasma.kickoff", "org.kde.plasma.icontasks",
                   "org.kde.plasma.panelspacer", "org.kde.plasma.digitalclock", "org.kde.plasma.showdesktop"];
    for (var i = 0; i < plugins.length; ++i)
        p.addWidget(plugins[i]);
    return p;
}

function __tasks(p) { return p.widgets("org.kde.plasma.icontasks")[0]; }

// the Raye taskbar: Raye Start, the Raye task list, the tray, the Raye clock
function __rayeTaskbar(screen) {
    var p = __panel(screen === undefined ? 0 : screen);
    p.addWidget("org.raye.start");
    var t = p.addWidget("org.raye.taskbar");
    t.set("General", "rowWidth", "176");
    var tray = p.addWidget("org.kde.plasma.systemtray");
    tray.set("General", "extraItems", "org.kde.plasma.volume,org.kde.plasma.notifications,org.kde.plasma.battery");
    p.addWidget("org.raye.clock");
    p.addWidget("org.kde.plasma.showdesktop");
    __log = [];
    return p;
}
