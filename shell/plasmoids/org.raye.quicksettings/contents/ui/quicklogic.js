// The decisions of quick settings, kept apart from the windows so they can
// be tested on their own: which tiles show, how they page, what Edit does
// to the list, and which glyph stands for what.
.pragma library

// what the panel can show, in the order "Add" lists them
var TILES = ["wifi", "bluetooth", "airplane", "energysaver", "nightlight", "accessibility",
             "dnd", "mic", "project", "vpn", "hotspot"];
// same as [QuickSettings] Tiles in rayeshellrc
var DEFAULTS = ["wifi", "bluetooth", "airplane", "energysaver", "nightlight", "accessibility"];
// A PC with no Wi-Fi or Bluetooth would open on a nearly empty grid. As long
// as the person has not chosen, these take the empty places.
var FILLERS = ["project", "dnd", "mic", "vpn"];
var PER_PAGE = 6;

function known(id) {
    return TILES.indexOf(id) >= 0;
}

function unique(list) {
    var out = [];
    for (var i = 0; i < list.length; ++i) {
        if (known(list[i]) && out.indexOf(list[i]) < 0)
            out.push(list[i]);
    }
    return out;
}

function sameList(a, b) {
    return a.length === b.length && a.every(function (v, i) { return v === b[i]; });
}

// The tiles to draw: the person's list without what this PC cannot do.
// `available` is an object: id -> bool. An id it does not name is available.
function shownTiles(configured, available) {
    var on = function (id) { return available[id] !== false; };
    var list = unique(configured);
    var out = list.filter(on);
    if (!sameList(list, DEFAULTS))
        return out;
    for (var i = 0; i < FILLERS.length && out.length < PER_PAGE; ++i) {
        if (on(FILLERS[i]) && out.indexOf(FILLERS[i]) < 0)
            out.push(FILLERS[i]);
    }
    return out;
}

// Edit starts from what is on screen: saving it never brings back a tile
// the person cannot see.
function addableTiles(shown, available) {
    return TILES.filter(function (id) {
        return shown.indexOf(id) < 0 && available[id] !== false;
    });
}

function pageCount(count, perPage) {
    return Math.max(1, Math.ceil(count / (perPage || PER_PAGE)));
}

function page(list, index, perPage) {
    var size = perPage || PER_PAGE;
    var last = pageCount(list.length, size) - 1;
    var at = Math.max(0, Math.min(index, last));
    return list.slice(at * size, (at + 1) * size);
}

function addTile(list, id) {
    return !known(id) || list.indexOf(id) >= 0 ? list.slice() : list.concat([id]);
}

function removeTile(list, id) {
    return list.filter(function (t) { return t !== id; });
}

// the list with the tile at `from` moved to `to`, both clamped to the list
function moveTile(list, from, to) {
    var out = list.slice();
    if (from < 0 || from >= out.length)
        return out;
    var moved = out.splice(from, 1)[0];
    out.splice(Math.max(0, Math.min(to, out.length)), 0, moved);
    return out;
}

// which cell a tile dropped at x, y lands on
function cellAt(x, y, cellWidth, cellHeight, columns) {
    var col = Math.max(0, Math.min(columns - 1, Math.floor(x / cellWidth)));
    var row = Math.max(0, Math.floor(y / cellHeight));
    return row * columns + col;
}

// A tile that does nothing but open something has no on and off.
function isToggle(id) {
    return id !== "accessibility" && id !== "project";
}

// the right part of a tile, with its "›": it opens a page of the panel
function isSplit(id) {
    return tilePage(id) !== "";
}

// the page of the panel behind a tile's "›"; "" when it has none
function tilePage(id) {
    var pages = { wifi: "network", bluetooth: "bluetooth", vpn: "vpn" };
    return pages[id] || "";
}

// the Raye Settings page behind a tile; "" when there is none
function settingsPage(id) {
    var pages = {
        wifi: "network/wifi",
        bluetooth: "devices/bluetooth",
        airplane: "network/airplane-mode",
        energysaver: "system/power",
        nightlight: "system/night-light",
        accessibility: "accessibility",
        dnd: "system/notifications",
        mic: "system/sound",
        project: "system/display",
        vpn: "network/vpn",
        hotspot: "network/hotspot",
    };
    return pages[id] || "";
}

function tileIcon(id) {
    var icons = {
        wifi: "network-wireless-signal-excellent-symbolic",
        bluetooth: "network-bluetooth-symbolic",
        airplane: "network-flightmode-on-symbolic",
        energysaver: "battery-profile-powersave-symbolic",
        nightlight: "redshift-status-on-symbolic",
        // drawn by AccessibilityGlyph.qml
        accessibility: "applet:accessibility",
        dnd: "notifications-disabled-symbolic",
        mic: "microphone-sensitivity-muted-symbolic",
        project: "monitor-symbolic",
        vpn: "network-vpn-symbolic",
        hotspot: "network-wireless-hotspot-symbolic",
    };
    return icons[id] || "";
}

function volumeIcon(percent, muted) {
    if (muted || percent <= 0)
        return "audio-volume-muted-symbolic";
    if (percent < 34)
        return "audio-volume-low-symbolic";
    if (percent < 67)
        return "audio-volume-medium-symbolic";
    return "audio-volume-high-symbolic";
}

// the battery glyphs come in tens
function batteryIcon(percent, charging) {
    var tens = Math.max(0, Math.min(100, Math.round(percent / 10) * 10));
    var name = ("00" + tens).slice(-3);
    return "battery-" + name + (charging ? "-charging" : "") + "-symbolic";
}

// a wheel notch on the pill is 2 %, like Windows
function wheelVolume(volume, angleDelta) {
    if (angleDelta === 0)
        return volume;
    return Math.max(0, Math.min(100, volume + (angleDelta > 0 ? 2 : -2)));
}

function clampPercent(value, max) {
    if (!(max > 0))
        return 0;
    return Math.max(0, Math.min(100, Math.round(value * 100 / max)));
}

// Windows hands the same text to a screen reader and to the tooltip.
function joinLines(lines) {
    return lines.filter(function (l) { return !!l; }).join("\n");
}

// --- the network tile and its page -----------------------------------
// A connection, as the Backend hands it over:
//   { name, kind: "wired" | "wifi", state: "active" | "connecting" | "",
//     saved, secured, askPassword, signal, path, device, specific }

// What the network tile says. Ethernet wins over Wi-Fi, the way the
// system routes: a PC on a cable is "Ethernet" even with Wi-Fi on.
//   { kind: "wired" | "wifi" | "connecting" | "off" | "none", name }
function networkState(connections, wifiAvailable, wifiOn) {
    var list = connections || [];
    var find = function (kind, state) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i].kind === kind && list[i].state === state)
                return list[i];
        }
        return null;
    };
    var wired = find("wired", "active");
    if (wired)
        return { kind: "wired", name: wired.name };
    var wifi = find("wifi", "active");
    if (wifi)
        return { kind: "wifi", name: wifi.name };
    var pending = find("wired", "connecting") || find("wifi", "connecting");
    if (pending)
        return { kind: "connecting", name: pending.name };
    if (wifiAvailable && !wifiOn)
        return { kind: "off", name: "" };
    return { kind: "none", name: "" };
}

// What the tile and its page are about: "ethernet" while the cable is in
// use or when the PC has no Wi-Fi card, "wifi" otherwise.
function networkMode(connections, wifiAvailable) {
    if (!wifiAvailable)
        return "ethernet";
    var list = connections || [];
    for (var i = 0; i < list.length; ++i) {
        if (list[i].kind === "wired" && (list[i].state === "active" || list[i].state === "connecting"))
            return "ethernet";
    }
    return "wifi";
}

// NetworkManager names a cable "Wired connection 1" in its own language,
// often English: those read as "Ethernet", a name the user gave stays.
function cableName(name) {
    var text = String(name || "").trim();
    return /^(wired connection|connexion filaire)( \d+)?$/i.test(text) ? "" : text;
}

// The IPv4 address out of the values NetworkManager lists for a
// connection: the first one that reads like an address comes before the
// gateway and the DNS servers.
function ipv4Address(values) {
    var list = values || [];
    for (var i = 0; i < list.length; ++i) {
        var m = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})(\/\d+)?$/.exec(String(list[i] || "").trim());
        if (m && m.slice(1, 5).every(function (n) { return Number(n) <= 255; }))
            return m.slice(1, 5).join(".");
    }
    return "";
}

function wifiIcon(signal, secured) {
    if (secured) {
        var step = Math.max(0, Math.min(100, Math.round((signal || 0) / 20) * 20));
        return "network-wireless-" + (step === 0 ? "00" : String(step)) + "-locked-symbolic";
    }
    if (signal >= 80)
        return "network-wireless-signal-excellent-symbolic";
    if (signal >= 55)
        return "network-wireless-signal-good-symbolic";
    if (signal >= 30)
        return "network-wireless-signal-ok-symbolic";
    if (signal > 5)
        return "network-wireless-signal-weak-symbolic";
    return "network-wireless-signal-none-symbolic";
}

function networkIcon(connection) {
    if (connection.kind === "wired")
        return "network-wired-symbolic";
    return wifiIcon(connection.signal, connection.secured);
}

// The page's list: the cable first, then what is connected, then the
// strongest Wi-Fi. A network seen twice (two saved profiles) shows once.
function sortNetworks(connections) {
    var rank = function (c) {
        return (c.kind === "wired" ? 0 : 4) + (c.state === "active" ? 0 : c.state === "connecting" ? 1 : 2);
    };
    var out = [];
    var seen = {};
    var sorted = (connections || []).slice().sort(function (a, b) {
        var d = rank(a) - rank(b);
        if (d !== 0)
            return d;
        d = (b.signal || 0) - (a.signal || 0);
        if (d !== 0)
            return d;
        return a.name < b.name ? -1 : a.name > b.name ? 1 : 0;
    });
    for (var i = 0; i < sorted.length; ++i) {
        var key = sorted[i].kind + ":" + sorted[i].name;
        if (sorted[i].kind === "wifi" && seen[key])
            continue;
        seen[key] = true;
        out.push(sorted[i]);
    }
    return out;
}

// what clicking Connect does with a row: "disconnect", "password" (ask for
// it first, in the row), or "connect"
function networkAction(connection, password) {
    if (connection.state === "active" || connection.state === "connecting")
        return "disconnect";
    if (connection.askPassword && !password)
        return "password";
    return "connect";
}
