// Plain rules of the Raye sign-in screen, kept out of the QML so they can
// be tested on their own.
.pragma library

// The network status icon of plasma-nm ("network-wireless-60-locked",
// "network-wired-activated", ...) as one of the Raye icons:
// network-wired, network-wireless-0..4, network-none, or "" (hidden).
function networkIcon(name) {
    if (typeof name !== "string" || name.indexOf("network-") !== 0)
        return ""
    var s = name.replace(/-symbolic$/, "")
    if (s === "network-wired" || s === "network-wired-activated" || s === "network-vpn")
        return "network-wired"
    var m = /^network-(wireless|mobile)-(?:connected-)?(\d{1,3})(?:-[a-z0-9]+)*$/.exec(s)
    if (m)
        return "network-wireless-" + bars(parseInt(m[2], 10))
    var none = ["network-disconnect", "network-wireless-disconnected", "network-wired-disconnected",
                "network-unavailable", "network-flightmode-on", "network-connect",
                "network-mobile-available", "network-wireless"]
    if (none.indexOf(s) >= 0)
        return "network-none"
    return ""
}

// signal strength 0..100 to 0..4 bars
function bars(strength) {
    if (strength <= 0)
        return 0
    if (strength <= 20)
        return 1
    if (strength <= 40)
        return 2
    if (strength <= 60)
        return 3
    return 4
}

// battery-000 .. battery-100 by tens, "-charging" when plugged in;
// "" when the machine has no battery of its own
function batteryIcon(hasBattery, percent, pluggedIn) {
    if (!hasBattery || typeof percent !== "number" || isNaN(percent))
        return ""
    var p = Math.max(0, Math.min(100, Math.round(percent / 10) * 10))
    var digits = ("00" + p).slice(-3)
    return "battery-" + digits + (pluggedIn ? "-charging" : "")
}

// "FR", "US": the layout's short name, without its variant
function layoutCode(layouts, index) {
    if (!layouts || index < 0 || index >= layouts.length || !layouts[index])
        return ""
    var s = String(layouts[index].shortName || "")
    s = s.replace(/\(.*$/, "").trim()
    return s.slice(0, 3).toUpperCase()
}

function showLayout(layouts) {
    return !!layouts && layouts.length > 1
}

function nextLayout(count, index) {
    if (count <= 0)
        return -1
    var next = index + 1
    return next > 0 && next < count ? next : 0
}

function indexInRange(index, count) {
    if (count <= 0)
        return -1
    if (typeof index !== "number" || index < 0 || index >= count)
        return 0
    return index
}

// The account shown first: the last one who signed in, found by name
// (an index can be stale when accounts were added), else by index.
function firstUser(users, lastIndex, lastUser) {
    if (!users || users.length === 0)
        return -1
    if (lastUser) {
        for (var i = 0; i < users.length; ++i)
            if (users[i] && users[i].name === lastUser)
                return i
    }
    return indexInRange(lastIndex, users.length)
}

function displayName(user) {
    if (!user)
        return ""
    var real = String(user.realName || "").trim()
    return real !== "" ? real : String(user.name || "")
}

// The lock picture from theme.conf ("background"): a local file only.
// "" means the Raye picture.
function picture(value) {
    if (typeof value !== "string")
        return ""
    var s = value.trim()
    if (s.indexOf("file://") === 0)
        s = s.slice(7)
    if (s.indexOf("/") !== 0)
        return ""
    if (s.indexOf("/usr/share/wallpapers/Next/") === 0)
        return ""
    return "file://" + s
}
