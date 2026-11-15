// The taskbar's decisions, as plain functions so they can be tested
// without Plasma: pinned apps as launcher URLs, how the buttons fit
// (labels, small buttons, the "..." overflow), what a click does, the
// indicator under a button and the badge text.
.pragma library

var pitch = 44              // one 40 px button to the next
var smallPitch = 36         // "Show smaller taskbar buttons": 32 px buttons
var labelMin = 160          // a button with its window title
var labelMax = 200
var labelGap = 4            // between two labelled buttons
var overflowPitch = 44      // the "..." button

// --- pinned apps -------------------------------------------------------

// Same check as ShellSettings::isDesktopId: a file name, never a path.
function isDesktopId(id) {
    return typeof id === "string" && /^[A-Za-z0-9][A-Za-z0-9._+-]*\.desktop$/.test(id) && id.length <= 255
}

// "org.kde.dolphin.desktop" -> "applications:org.kde.dolphin.desktop",
// the form Plasma's tasks model keeps launchers in. "" when not an id.
function launcherForDesktopId(id) {
    return isDesktopId(id) ? "applications:" + id : ""
}

// The desktop id back from a launcher URL: "applications:x.desktop",
// "file:///usr/share/applications/x.desktop" or a plain path. "" when
// the launcher is not a desktop file (an executable, a web URL).
function desktopIdForLauncher(url) {
    var s = String(url || "")
    var q = s.indexOf("?")
    if (q >= 0)
        s = s.substring(0, q)   // Plasma adds ?iconData=... to some
    if (s.indexOf("applications:") === 0)
        s = s.substring("applications:".length)
    else if (s.indexOf("file://") === 0 || s.indexOf("/") === 0)
        s = s.substring(s.lastIndexOf("/") + 1)
    else if (s.indexOf(":") >= 0)
        return ""
    s = decodeURIComponent(s)
    return isDesktopId(s) ? s : ""
}

function launchersForPinned(ids) {
    var out = []
    for (var i = 0; i < ids.length; ++i) {
        var url = launcherForDesktopId(ids[i])
        if (url !== "" && out.indexOf(url) < 0)
            out.push(url)
    }
    return out
}

function pinnedForLaunchers(urls) {
    var out = []
    for (var i = 0; i < urls.length; ++i) {
        var id = desktopIdForLauncher(urls[i])
        if (id !== "" && out.indexOf(id) < 0)
            out.push(id)
    }
    return out
}

function sameList(a, b) {
    if (a.length !== b.length)
        return false
    for (var i = 0; i < a.length; ++i) {
        if (a[i] !== b[i])
            return false
    }
    return true
}

// --- fitting the buttons -----------------------------------------------

// How the buttons are drawn in the room the taskbar has.
//   combine:      "always", "full" (only when the taskbar is full), "never"
//   smallButtons: "never", "always", "full"
//   grouped:      buttons when windows of an app share one button
//   ungrouped:    buttons when each window has its own
//   available:    width in px
// Returns { labels, small, count, buttonWidth, pitch, visible, overflow,
//           width } where count is the number of buttons drawn (grouped
// or not), visible how many fit before the "..." and width what the
// row takes.
function fit(combine, smallButtons, grouped, ungrouped, available) {
    available = Math.max(0, Math.floor(available))
    var labels = false
    if (combine === "never")
        labels = true
    else if (combine === "full")
        labels = ungrouped * (labelMin + labelGap) <= available
    var count = labels ? ungrouped : grouped

    var small = smallButtons === "always"
    if (!small && smallButtons === "full" && !labels)
        small = count * pitch > available

    var buttonWidth, step
    if (labels) {
        // as wide as the room allows, between the two sizes
        var each = count > 0 ? Math.floor(available / count) - labelGap : labelMax
        buttonWidth = Math.max(labelMin, Math.min(labelMax, each))
        step = buttonWidth + labelGap
    } else {
        buttonWidth = small ? smallPitch - 4 : pitch - 4
        step = small ? smallPitch : pitch
    }

    var visible = count
    var overflow = false
    if (count * step > available) {
        overflow = true
        visible = Math.max(0, Math.floor((available - overflowPitch) / step))
    }
    var width = visible * step + (overflow ? overflowPitch : 0)
    return {
        labels: labels, small: small, count: count, buttonWidth: buttonWidth,
        pitch: step, visible: visible, overflow: overflow, width: width
    }
}

// Where the row starts in the taskbar's own width: centred, or at the
// left edge. Used when the Start button is not next to the taskbar (the
// Start applet centres the whole group when it is).
function rowStart(alignment, rowWidth, containerWidth) {
    if (alignment !== "center")
        return 0
    return Math.max(0, Math.round((containerWidth - rowWidth) / 2))
}

// --- clicks ------------------------------------------------------------

// What a click on a button does. task: { isLauncher, isStartup, isActive,
// isMinimized, childCount }; button: "left" or "middle"; shift, ctrl.
// One of: "launch" (start the pinned app), "newInstance", "activate",
// "minimize", "cycle" (next window of the group), "none".
function clickAction(task, button, shift, ctrl) {
    if (task.isStartup)
        return "none"
    // Windows' Ctrl+Shift+click runs the app elevated; Raye never runs
    // a desktop app as root, so it opens a new window like Shift+click
    if (button === "middle" || shift)
        return task.isLauncher ? "launch" : "newInstance"
    if (button !== "left")
        return "none"
    if (task.isLauncher)
        return "launch"
    if (task.childCount > 1)
        return "cycle"
    if (task.isActive && !task.isMinimized)
        return "minimize"
    return "activate"
}

// The next window of a group after the active one (child rows), or the
// first when none of them is active. activeRow: -1 when none.
function nextInGroup(childCount, activeRow) {
    if (childCount <= 0)
        return -1
    if (activeRow < 0)
        return 0
    return (activeRow + 1) % childCount
}

// --- what the button shows ---------------------------------------------

// The pill under the icon: "none" (pinned, not running), "starting"
// (the app was launched and its window is not there yet: a short pill
// that breathes), "running" (short and grey) or "focused" (long and
// accent).
function indicator(task) {
    if (task.isStartup)
        return "starting"
    if (task.isLauncher)
        return "none"
    if (task.isActive && !task.isMinimized)
        return "focused"
    return "running"
}

function badgeText(count) {
    count = Math.floor(Number(count) || 0)
    if (count <= 0)
        return ""
    return count > 99 ? "99+" : String(count)
}

// --- badges: com.canonical.Unity.LauncherEntry -----------------------------
//
// An application publishes its own unread count on the session bus:
//   Update("application://org.kde.kmail2.desktop",
//          { "count": 5, "count-visible": true })
// Telegram, Discord, Thunderbird, Nextcloud and the GNOME/Elementary
// mail clients all speak it. The badge is then the app's own number,
// not "how many of its notifications are still in the centre".
//
// Everything here returns a NEW object. badgeCounts sits behind a QML
// property binding: a function that edits the map in place changes
// nothing on screen.

// "application://org.kde.kmail2.desktop" -> "org.kde.kmail2", the key
// appKey() gives for the same app. "" for anything else, so rubbish on
// a public bus can never put a badge on an icon.
function launcherEntryKey(appUri) {
    var s = String(appUri || "")
    if (s.indexOf("application://") !== 0)
        return ""
    s = s.substring("application://".length)
    var q = s.indexOf("?")
    if (q >= 0)
        s = s.substring(0, q)
    if (s.indexOf("/") >= 0)
        s = s.substring(s.lastIndexOf("/") + 1)   // a full path is allowed
    try {
        s = decodeURIComponent(s)
    } catch (e) {
        return ""
    }
    if (!isDesktopId(s))
        return ""
    return s.substring(0, s.length - ".desktop".length).toLowerCase()
}

function copyCounts(counts) {
    var next = {}
    for (var k in counts)
        next[k] = counts[k]
    return next
}

// One Update applied to the badges.
//
// An update carries only what changed, so a message about `progress`
// alone must leave the count alone -- it is a different feature of the
// same interface and it is not a badge.
//
// `count` with no `count-visible` is taken as visible. The interface
// says count-visible governs it, and libunity does send both together;
// but reading a lone count as hidden means an app that splits the two
// across two messages never gets a badge at all, which is the defect
// this replaces. A count of 0 still means no badge, so an app that
// wants it gone has a plain way to say so.
function applyLauncherEntry(counts, appUri, properties) {
    var next = copyCounts(counts)
    var key = launcherEntryKey(appUri)
    if (key === "")
        return next
    var p = properties || {}
    var hasCount = "count" in p
    var hasVisible = "count-visible" in p
    if (!hasCount && !hasVisible)
        return next
    var visible = hasVisible ? p["count-visible"] === true : true
    var n = hasCount ? Math.floor(Number(p["count"])) : next[key]
    if (!visible || !isFinite(n) || n <= 0)
        delete next[key]
    else
        next[key] = n
    return next
}

// The application is gone -- it quit, it crashed, it was killed -- and
// will say nothing more. Its badge goes with it. A number nobody can
// clear sends someone into an app to look for a message that is not
// there, which is worse than no badge at all.
function dropLauncherEntry(counts, appUri) {
    var next = copyCounts(counts)
    var key = launcherEntryKey(appUri)
    if (key !== "")
        delete next[key]
    return next
}

// The app's own id for badges: "org.kde.dolphin" from a launcher URL or
// an app id (what notifications carry as desktop-entry).
function appKey(launcherUrl, appId) {
    var id = desktopIdForLauncher(launcherUrl)
    if (id === "")
        id = String(appId || "")
    if (id.length > 8 && id.substring(id.length - 8) === ".desktop")
        id = id.substring(0, id.length - 8)
    return id.toLowerCase()
}

// --- moving a button by dragging it -------------------------------------

// Where a dragged button goes, from where it is now (current, -1 at the
// start): the pointer past the middle of the next button takes that
// button's place, and coming back needs it past the middle of the one that
// moved over. Going back and forth needs a whole button of travel, so a
// shaky hand never makes two buttons swap and swap back.
function dragSlot(from, current, pointerX, pitch, width, count) {
    var t = current < 0 ? from : current
    while (t + 1 < count && pointerX > (t + 1) * pitch + width / 2)
        ++t
    while (t > 0 && pointerX < (t - 1) * pitch + width / 2)
        --t
    return t
}

// The place a button is shown at while another one is dragged from `from`
// to `to`: the ones in between step over by one to make room.
function shiftedSlot(i, from, to) {
    if (from < 0 || to < 0 || i === from)
        return i
    if (from < to && i > from && i <= to)
        return i - 1
    if (from > to && i >= to && i < from)
        return i + 1
    return i
}

// Meta+1...9: which button a number picks, or -1. Buttons hidden behind
// the "..." still count, as in Windows.
function indexForNumber(number, count) {
    var i = Math.floor(number) - 1
    return i >= 0 && i < count ? i : -1
}

// --- pins on several screens -------------------------------------------

// Whether a taskbar shows the pinned apps. "Show my taskbar apps on":
// all taskbars, or (the two other choices) the main taskbar only, the
// copies on the other screens holding just their open windows, as in
// Windows. multiDisplayApps: "all", "main-and-open", "open".
function showPins(multiDisplayApps, mainScreen) {
    return mainScreen || multiDisplayApps === "all" || multiDisplayApps === undefined
}

// The pins of the stock task manager ("applications:x.desktop,...") as
// desktop ids, for moving a taskbar over to this one. "Default browser"
// becomes the browser's own entry when it is known.
function pinnedFromStockLaunchers(launchers, browserId) {
    var list = typeof launchers === "string" ? launchers.split(",") : (launchers || [])
    var out = []
    for (var i = 0; i < list.length; ++i) {
        var s = String(list[i]).trim()
        var id = s === "preferred://browser" ? (isDesktopId(browserId) ? browserId : "")
                                             : desktopIdForLauncher(s)
        if (id !== "" && out.indexOf(id) < 0)
            out.push(id)
    }
    return out
}

// --- the right-click menu ----------------------------------------------

// The rows of an app's right-click menu, top to bottom, as Windows has
// them: recent files, the app's tasks, then the app, pin or unpin, end
// task, close. info: { recent, actions (counts), valid (the app is
// installed), pinned, isLauncher, childCount, endTask }.
// Each row: { kind } with kind one of "header" (+ section "recent" or
// "tasks"), "recent" / "action" (+ at, the index in its list),
// "separator", "app", "pin", "unpin", "endTask", "close".
function jumpListEntries(info) {
    var out = []
    var i
    if (info.recent > 0) {
        out.push({ kind: "header", section: "recent" })
        for (i = 0; i < info.recent; ++i)
            out.push({ kind: "recent", at: i })
    }
    if (info.actions > 0) {
        out.push({ kind: "header", section: "tasks" })
        for (i = 0; i < info.actions; ++i)
            out.push({ kind: "action", at: i })
    }
    if (out.length > 0)
        out.push({ kind: "separator" })
    if (info.valid)
        out.push({ kind: "app" })
    if (info.pinned)
        out.push({ kind: "unpin" })
    else if (info.valid)
        out.push({ kind: "pin" })
    if (!info.isLauncher && info.endTask)
        out.push({ kind: "endTask" })
    if (!info.isLauncher)
        out.push({ kind: "close" })
    return out
}
