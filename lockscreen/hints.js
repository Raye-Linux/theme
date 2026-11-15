// Small decisions of the Raye lock screen, kept here so they can be tested
// without a greeter: which "other way in" line shows under the picture,
// the lock screen's own words, and the status icons and pictures it feeds
// to the sign-in module.
//
// Method lines: the greeter lists a reader as available as soon as it
// starts asking, before PAM has said anything, and on a machine without
// a reader it only learns later that there is none. So a line shows only
// after that kind of reader has actually spoken (an info or error message)
// and while it is still listed. Smartcards are never advertised.
//
// State objects are never changed in place: each call returns a new one.
.pragma library

// the greeter's flags for its non-interactive authenticators
var FINGERPRINT = 1
var SMARTCARD = 2

// how long "try again" stays after a finger was not recognised (ms)
var RETRY_SHOWN = 3000

function initial() {
    return { spoken: 0, retryUntil: 0 }
}

function _copy(s) {
    return { spoken: s.spoken, retryUntil: s.retryUntil }
}

// a non-interactive authenticator sent an info message ("Place your
// finger on the reader")
function spoke(state, kind) {
    var s = _copy(state)
    s.spoken |= (kind & FINGERPRINT)
    return s
}

// it sent an error message (finger not recognised): it is there and
// talking, and "try again" shows for a while
function failed(state, kind, now) {
    var s = spoke(state, kind)
    if (kind & FINGERPRINT)
        s.retryUntil = now + RETRY_SHOWN
    return s
}

// the list of available authenticators changed: a kind that left has to
// speak again before its line comes back
function available(state, types) {
    var s = _copy(state)
    s.spoken &= types
    if (!(types & FINGERPRINT))
        s.retryUntil = 0
    return s
}

// "" (no line), "fingerprint" or "fingerprintRetry"
function hint(state, types, now) {
    if ((types & FINGERPRINT) && (state.spoken & FINGERPRINT))
        return now < state.retryUntil ? "fingerprintRetry" : "fingerprint"
    return ""
}

// --- words -----------------------------------------------------------
// The sign-in module has the shared words; these are the lock screen's
// own (the media card and the fingerprint retry line).
var WORDS = {
    en: {
        fingerprint: "Scan your finger on the fingerprint reader",
        fingerprintRetry: "That finger wasn't recognized. Try again.",
        previousTrack: "Previous track",
        nextTrack: "Next track",
        play: "Play",
        pause: "Pause",
        noTitle: "No title"
    },
    fr: {
        fingerprint: "Posez votre doigt sur le lecteur d'empreintes",
        fingerprintRetry: "Empreinte non reconnue. Réessayez.",
        previousTrack: "Piste précédente",
        nextTrack: "Piste suivante",
        play: "Lecture",
        pause: "Pause",
        noTitle: "Sans titre"
    }
}

function text(key, lang) {
    var table = WORDS[lang] || WORDS.en
    return table[key] !== undefined ? table[key] : WORDS.en[key]
}

function hintText(kind, lang) {
    return kind === "" ? "" : text(kind, lang)
}

// no fingerprint icon in the sign-in set yet: the line goes without one
function hintIcon(kind) {
    return ""
}

// --- status icons ------------------------------------------------------

// "battery-050-charging"; "" without an internal battery
function batteryIcon(hasBattery, percent, pluggedIn) {
    if (!hasBattery || typeof percent !== "number" || isNaN(percent))
        return ""
    var level = Math.round(Math.max(0, Math.min(100, percent)) / 10) * 10
    var name = "battery-" + ("00" + level).slice(-3)
    return pluggedIn ? name + "-charging" : name
}

// The network applet's icon name -> one of the sign-in icons:
// network-wired, network-wireless-0..4, network-none; "" when unknown.
function networkIcon(name) {
    if (typeof name !== "string" || name === "")
        return ""
    if (/disconnect|offline|unavailable|limited|error/.test(name))
        return "network-none"
    if (/wireless|wifi|mobile/.test(name)) {
        var m = name.match(/-(\d{1,3})(?:-|$)/)
        var p = m ? parseInt(m[1], 10) : 0
        var bars = p <= 0 ? 0 : p <= 20 ? 1 : p <= 40 ? 2 : p <= 60 ? 3 : 4
        return "network-wireless-" + bars
    }
    if (/wired|vpn|ethernet/.test(name))
        return "network-wired"
    return ""
}

// "fr" -> "FR", "ca(fr)" -> "CA": at most three letters, like Windows'
function layoutCode(shortName) {
    if (typeof shortName !== "string")
        return ""
    var m = shortName.match(/^[A-Za-z]+/)
    return m ? m[0].slice(0, 3).toUpperCase() : ""
}

// --- pictures --------------------------------------------------------

function _fileUrl(path) {
    return "file://" + path.split("/").map(encodeURIComponent).join("/")
}

// the greeter gives the account picture as a plain path
function userPictureUrl(path) {
    if (typeof path !== "string" || path.charAt(0) !== "/")
        return ""
    return _fileUrl(path)
}

var RAYE_WALLPAPER = "/usr/share/wallpapers/Raye"

// The lock picture from the lock screen's wallpaper setting (a picture
// file or a wallpaper package folder, as a path or a file URL). "" means
// the sign-in module's default Raye picture, which also has a pre-blurred
// copy for machines without 3D.
function lockPicture(image) {
    if (typeof image !== "string" || image === "")
        return ""
    var url = image
    if (url.charAt(0) === "/")
        url = _fileUrl(url)
    if (url.indexOf("file:///") !== 0)
        return ""
    var path = decodeURIComponent(url.slice(7))
    if (path === RAYE_WALLPAPER || path.indexOf(RAYE_WALLPAPER + "/") === 0)
        return ""
    if (/\.(png|jpe?g|webp|svgz?|avif|jxl|bmp)$/i.test(path))
        return url
    // a wallpaper package: its pictures are named by size
    return url.replace(/\/+$/, "") + "/contents/images/1920x1080.png"
}
