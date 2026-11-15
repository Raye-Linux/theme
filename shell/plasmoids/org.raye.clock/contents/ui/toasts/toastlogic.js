// The toasts' decisions, as plain functions so they can be tested without
// Plasma: how long a toast stays, what happens when it runs out, where the
// stack goes, and which name and icon a notification shows. System senders
// carry names of the desktop's parts; a person should read what they do
// ("Network"), so those are translated here, for the centre too.
.pragma library

var MAX_VISIBLE = 3
var WIDTH = 364
// from the card to the taskbar and to the edge of the screen
var EDGE = 12
// from one card to the next
var GAP = 12
var SLIDE_IN = 250
var SLIDE_OUT = 167
// the others moving up or down when one comes or goes
var RESTACK = 250
// a file offered with the notification gets longer to be looked at
var EXTRA_FOR_FILES = 5000
var BODY_LINES = 3
var BODY_LINES_OPEN = 12

// senders never shown, in the toasts and in the centre: the desktop's own
// fund-raising reminder is no business of a Raye user
var NEVER_SHOWN = ["donationmessage"]

// --- how long it stays --------------------------------------------------

// Milliseconds, 0 for "until closed". `timeout` is what the sender asked:
// -1 the default, 0 never, or its own number.
function timeoutMs(o) {
    if (o.critical)
        return 0
    if (o.job) {
        if (o.jobRunning)
            return o.permanentJobs ? 0 : Math.max(1, o.seconds) * 1000
        return Math.max(1, o.seconds) * 1000
    }
    if (o.timeout === 0)
        return 0
    if (o.timeout > 0)
        return o.timeout
    return Math.max(1, o.seconds) * 1000 + (o.urlCount > 0 ? EXTRA_FOR_FILES : 0)
}

// What the end of the time does: "dismiss" keeps a running job going
// out of sight; "hide" keeps a notification whose buttons still work in
// the centre; "expire" tells the sender it is over.
function whenTimeIsUp(o) {
    if (o.job && o.jobRunning)
        return "dismiss"
    if (o.resident || (o.hasActions && !o.transient))
        return "hide"
    return "expire"
}

// --- do not disturb ------------------------------------------------------

function inhibited(o) {
    if (!o.serverValid)
        return false
    if (o.until instanceof Date && !isNaN(o.until.getTime()) && o.now < o.until)
        return true
    return !!(o.byApplication
              || (o.whenFullscreen && o.fullscreenFocused)
              || (o.whenMirrored && o.screensMirrored))
}

function urgencies(o) {
    var mask = 0
    if (!o.inhibited || o.criticalInDnd)
        mask |= o.critical
    if (!o.inhibited) {
        mask |= o.normal
        if (o.lowPopups)
            mask |= o.low
    }
    return mask
}

// --- where it goes ---------------------------------------------------------

// heights: the windows, newest first. margin: what each window holds
// around its card (top and bottom). The first card sits `bottom` above
// nothing: the next one above it. A stack never fills more than `room`
// but the newest always shows. Each entry: { y, visible }.
function stack(heights, bottom, margin, gap, room) {
    var out = []
    var edge = bottom
    var used = 0
    for (var i = 0; i < heights.length; ++i) {
        var card = Math.max(0, heights[i] - 2 * margin)
        var y = edge - card - margin
        used += card + (i > 0 ? gap : 0)
        out.push({ y: y, visible: i === 0 || used <= room })
        edge = y + margin - gap
    }
    return out
}

// --- what it shows ---------------------------------------------------------

// Plain text only: what an app sends is never read as markup, so it
// cannot load anything.
function plain(text) {
    return String(text === undefined || text === null ? "" : text)
        .replace(/<br\s*\/?>/gi, "\n")
        .replace(/<[^>]*>/g, "")
        .replace(/&lt;/g, "<").replace(/&gt;/g, ">").replace(/&quot;/g, "\"")
        .replace(/&#39;/g, "'").replace(/&nbsp;/g, " ").replace(/&amp;/g, "&")
}

var RAYE_ICON = "raye-eye"

// senders of the desktop: notifyrc name or desktop entry -> [key, icon]
var SENDERS = {
    "plasma_workspace": ["system", RAYE_ICON],
    "org.kde.plasmashell": ["system", RAYE_ICON],
    "plasmashell": ["system", RAYE_ICON],
    "ksmserver": ["system", RAYE_ICON],
    "kwin": ["system", RAYE_ICON],
    "kded": ["system", RAYE_ICON],
    "kded6": ["system", RAYE_ICON],
    "oom-notifier": ["system", RAYE_ICON],
    "networkmanagement": ["network", "network-wired"],
    "org.kde.plasma-nm": ["network", "network-wired"],
    "devicenotifications": ["devices", "drive-removable-media"],
    "freespacenotifier": ["storage", "drive-harddisk"],
    "powerdevil": ["power", "battery"],
    "bluedevil": ["bluetooth", "preferences-system-bluetooth"],
    "kaccess": ["accessibility", "preferences-desktop-accessibility"],
    "libnotificationmanager": ["notifications", "preferences-desktop-notification"],
    "ksecretd": ["passwords", "dialog-password"],
    "polkit-kde-authentication-agent-1": ["security", "dialog-password"]
}

// words that name the desktop's parts: a sender with one of them in its
// name, or a picture of the desktop's own logo, shows as the system
var BRAND = /(^|[^a-z])(kde|plasma|kwin|kded|klipper)([^a-z]|$)/i
var BRAND_ICON = /(^|[-_.])(kde|plasma|plasmashell)([-_.]|$)|^start-here-kde/i

// The name and icon a notification shows. `key` is a word ToastSource.qml
// translates, or "" when `name` is the app's own name, shown as it is.
function source(applicationName, iconName, desktopEntry, notifyRcName, isJob) {
    var name = String(applicationName || "")
    var icon = String(iconName || "")
    var sender = SENDERS[String(notifyRcName || "")] || SENDERS[String(desktopEntry || "")]
    // the default sender is not who it is: Plasma files every plain
    // notification of an app under it
    if (String(notifyRcName || "") === "plasma_workspace" && desktopEntry && !SENDERS[desktopEntry]) {
        sender = null
        if (BRAND_ICON.test(icon))
            icon = ""
    }
    if (!sender && BRAND.test(name))
        sender = ["system", RAYE_ICON]
    if (sender) {
        // a copy started from the desktop is the Files app's work
        if (isJob && sender[0] === "system")
            return { key: "files", name: "", icon: "system-file-manager" }
        return { key: sender[0], name: "", icon: sender[1] }
    }
    if (BRAND_ICON.test(icon))
        icon = RAYE_ICON
    return { key: "", name: name, icon: icon || "preferences-desktop-notification" }
}

// a sender with no title of its own gets the desktop's generic words
// or the shell's name as one: nothing is better
var EMPTY_TITLES = [
    /^system notifications?$/i,
    /^notifications du syst[eè]me$/i,
    /^plasma( desktop)?( workspace| shell)?$/i,
    /^espace de travail (du bureau )?plasma$/i,
    /^kde( plasma)?$/i
]

function title(text) {
    var t = plain(text).trim()
    for (var i = 0; i < EMPTY_TITLES.length; ++i) {
        if (EMPTY_TITLES[i].test(t))
            return ""
    }
    return t
}

// "Connection 'Wired connection 1' activated." in plain words. Returns
// { kind: "connected" | "disconnected", name } or null.
function networkEvent(notifyRcName, body) {
    if (String(notifyRcName || "") !== "networkmanagement")
        return null
    var text = plain(body).trim()
    var on = /^(?:VPN )?[Cc]onnection '(.+)' activated\.?$/.exec(text)
             || /^Connexion (?:VPN )?« (.+) » activée\.?$/.exec(text)
    var off = /^(?:VPN )?[Cc]onnection '(.+)' deactivated\.?$/.exec(text)
              || /^Connexion (?:VPN )?« (.+) » désactivée\.?$/.exec(text)
              || /^Désactivation de la connexion (?:VPN )?« (.+) »\.?$/.exec(text)
    var hit = on || off
    if (!hit)
        return null
    var name = /^(wired connection|connexion filaire)( \d+)?$/i.test(hit[1]) ? "Ethernet" : hit[1]
    return { kind: on ? "connected" : "disconnected", name: name }
}

// A sender's icon goes in the header when it is an icon of the theme; a
// picture of its own (a contact's photo) beside the words.
function themeIcon(icon) {
    return typeof icon === "string" && icon !== "" && !/^(\/|file:)/.test(icon) ? icon : ""
}

function pictureFile(icon) {
    return typeof icon === "string" && /^(\/|file:)/.test(icon) ? icon : ""
}

// the one picture worth showing for a file offered with the notification
function thumbnail(urls) {
    if (!urls || urls.length !== 1)
        return ""
    var url = String(urls[0])
    return /^file:\/\/.+\.(png|jpe?g|webp|gif|bmp)$/i.test(url) ? url : ""
}
