// The Start menu's decisions, as plain functions so they can be tested
// without Plasma: the pins (one list for every Start of the session),
// pages of the Pinned grid and where tiles go while one is dragged, the
// letters of All apps, which search results go together, what Recommended
// shows, and moving the pins of the stock launcher over once.
.pragma library

var columns = 6             // Pinned: 6 tiles per row
var cellWidth = 96
var cellHeight = 84


// Raye's default pins: the same list as [Start] Pinned in
// /etc/xdg/rayeshellrc (a test compares them).
var defaultPins = ["org.kde.dolphin.desktop", "firefox.desktop", "org.raye.store.desktop",
                   "org.raye.settings.desktop", "org.kde.konsole.desktop", "org.kde.kate.desktop",
                   "org.kde.kcalc.desktop", "org.kde.gwenview.desktop", "org.kde.haruna.desktop",
                   "org.kde.elisa.desktop", "org.raye.taskmanager.desktop"]

// An entry of a pin list as the Start menu keeps it: a desktop file name
// ("org.kde.dolphin.desktop") or a preferred:// link. The stock launcher
// writes "applications:" in front; a path to a desktop file keeps its name.
function pinId(value) {
    var v = String(value || "").trim()
    if (v.indexOf("applications:") === 0)
        v = v.substring("applications:".length)
    if (v.indexOf("file://") === 0)
        v = v.substring("file://".length)
    if (v.charAt(0) === "/" && /\.desktop$/.test(v))
        v = v.substring(v.lastIndexOf("/") + 1)
    return v
}

// The same list, every entry once, empty entries dropped.
function cleanPins(list) {
    var out = []
    var seen = {}
    for (var i = 0; list && i < list.length; ++i) {
        var id = pinId(list[i])
        if (id === "" || seen[id])
            continue
        seen[id] = true
        out.push(id)
    }
    return out
}

function isPinned(list, id) {
    return cleanPins(list).indexOf(pinId(id)) >= 0
}

// Pin at the end (Windows adds new pins last).
function pin(list, id) {
    var out = cleanPins(list)
    var v = pinId(id)
    if (v !== "" && out.indexOf(v) < 0)
        out.push(v)
    return out
}

function unpin(list, id) {
    var v = pinId(id)
    return cleanPins(list).filter(function (x) { return x !== v })
}

// "Move to front"
function moveToFront(list, id) {
    var v = pinId(id)
    var out = unpin(list, v)
    out.unshift(v)
    return out
}

// The list after dragging the pin at `from` to the place `to`.
function movePin(list, from, to) {
    var out = cleanPins(list)
    if (from < 0 || from >= out.length)
        return out
    to = Math.max(0, Math.min(to, out.length - 1))
    var item = out.splice(from, 1)[0]
    out.splice(to, 0, item)
    return out
}

// While the tile at `from` is dragged over the place `to`, where tile `i`
// shows: the dragged one at `to`, the ones in between shifted by one.
function visualIndex(i, from, to) {
    if (from < 0 || to < 0 || from === to)
        return i
    if (i === from)
        return to
    if (from < to && i > from && i <= to)
        return i - 1
    if (from > to && i >= to && i < from)
        return i + 1
    return i
}

// The place under a point of the grid (x, y inside the page), on `page`;
// cells of cellW x cellH (96 x 84 when not given).
function slotAt(x, y, page, perPage, count, cellW, cellH) {
    var cw = cellW || cellWidth
    var ch = cellH || cellHeight
    var col = Math.max(0, Math.min(columns - 1, Math.floor(x / cw)))
    var row = Math.max(0, Math.min(Math.floor(perPage / columns) - 1, Math.floor(y / ch)))
    var slot = page * perPage + row * columns + col
    return Math.max(0, Math.min(slot, count - 1))
}

function pageCount(count, perPage) {
    return Math.max(1, Math.ceil(count / Math.max(1, perPage)))
}

// Rows of the Pinned grid: 3, or 4 when Start is tall enough (a big
// screen) and the layout gives pins more room.
function pinnedRows(popupHeight, layout) {
    if (layout === "pins")
        return popupHeight >= 640 ? 4 : 3
    if (layout === "recommendations")
        return 2
    return popupHeight >= 740 ? 4 : 3
}

// Recommended: 2 columns, 2 rows (3 when Start is tall), none when it is
// turned off or the layout favours pins on a short screen.
function recommendedCount(popupHeight, layout, shown) {
    if (!shown)
        return 0
    if (layout === "pins")
        return popupHeight >= 740 ? 2 : 0
    if (layout === "recommendations")
        return popupHeight >= 640 ? 8 : 6
    return popupHeight >= 740 ? 6 : 4
}

// The pins of the stock launcher moved over once: only when the Start
// pins are still Raye's defaults (the person never changed them) and the
// stock launcher had some. Its list is in its own order; links it kept to
// apps that are not installed are dropped by the caller (`installed`).
// Returns the new list, or null when nothing should change.
function migratedPins(stockOrdered, currentPins, defaults, installed) {
    var stock = cleanPins(stockOrdered)
    if (stock.length === 0)
        return null
    var current = cleanPins(currentPins)
    var dflt = cleanPins(defaults)
    if (current.join(",") !== dflt.join(","))
        return null
    var out = []
    for (var i = 0; i < stock.length; ++i) {
        if (installed && !installed(stock[i]))
            continue
        out.push(stock[i])
    }
    if (out.length === 0 || out.join(",") === current.join(","))
        return null
    return out
}

// The stock launcher's links in its saved order (kactivitymanagerd-statsrc
// "ordering"), then the links it has that the order does not list.
function orderedFavorites(links, ordering) {
    var order = cleanPins(ordering)
    var all = cleanPins(links)
    var out = order.filter(function (x) { return all.indexOf(x) >= 0 || all.length === 0 })
    for (var i = 0; i < all.length; ++i) {
        if (out.indexOf(all[i]) < 0)
            out.push(all[i])
    }
    return out
}


var alphabet = ["#", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"]

// The letter heading an app in All apps: its first letter without accent,
// "#" for a digit or anything else.
function letterFor(name) {
    var s = String(name || "").trim()
    if (s === "")
        return "#"
    var c = s.charAt(0)
    if (typeof c.normalize === "function")
        c = c.normalize("NFD").charAt(0)
    c = c.toUpperCase()
    return c >= "A" && c <= "Z" ? c : "#"
}

// Apps sorted the way the list shows them: by letter ("#" first), then by
// name without case or accents.
function sortKey(name) {
    var s = String(name || "").trim()
    if (typeof s.normalize === "function")
        s = s.normalize("NFD").replace(/[̀-ͯ]/g, "")
    var l = letterFor(name)
    return (l === "#" ? "0" : "1" + l) + s.toLowerCase()
}

function compareApps(a, b) {
    var ka = sortKey(a.name)
    var kb = sortKey(b.name)
    return ka < kb ? -1 : ka > kb ? 1 : 0
}

// Which letters have apps: { "A": true, ... }
function lettersPresent(names) {
    var out = {}
    for (var i = 0; names && i < names.length; ++i)
        out[letterFor(names[i])] = true
    return out
}

// The row of the first app under `letter` in a sorted list of names, or of
// the next letter that has apps; -1 when none.
function firstRowFor(names, letter) {
    var want = alphabet.indexOf(letter)
    var best = -1
    var bestLetter = 99
    for (var i = 0; names && i < names.length; ++i) {
        var l = alphabet.indexOf(letterFor(names[i]))
        if (l >= want && l < bestLetter) {
            best = i
            bestLetter = l
            if (l === want)
                break
        }
    }
    return best
}

// The rows of All apps: a letter heading before the first app of each
// letter. apps: sorted [{ name, ... }] -> [{ header: "A" } | { app }].
function withHeaders(apps) {
    var out = []
    var current = null
    for (var i = 0; apps && i < apps.length; ++i) {
        var l = letterFor(apps[i].name)
        if (l !== current) {
            out.push({ header: l })
            current = l
        }
        out.push({ app: apps[i] })
    }
    return out
}

// The row of a letter's heading in those rows, or of the next letter
// that has one; -1 when none.
function headerRow(rows, letter) {
    var want = alphabet.indexOf(letter)
    for (var i = 0; rows && i < rows.length; ++i) {
        if (rows[i].header !== undefined && alphabet.indexOf(rows[i].header) >= want)
            return i
    }
    return -1
}


// Results go together by kind, in this order.
var searchGroups = ["best", "apps", "settings", "files", "store", "answers", "other"]

function searchGroup(runnerId) {
    var id = String(runnerId || "")
    if (id === "krunner_services" || id === "services")
        return "apps"
    if (id === "org.raye.settings.runner" || id === "krunner_systemsettings")
        return "settings"
    if (id === "baloosearch" || id === "krunner_recentdocuments" || id === "krunner_placesrunner"
        || id === "locations" || id === "recentdocuments")
        return "files"
    if (id === "org.raye.store.runner")
        return "store"
    if (id === "calculator" || id === "unitconverter" || id === "org.kde.datetime")
        return "answers"
    return "other"
}

// The runners Start asks, apps first; the Store runner gives "Get <app>"
// for what is not installed.
var searchRunners = ["krunner_services", "org.raye.settings.runner", "calculator",
                     "unitconverter", "baloosearch", "krunner_recentdocuments",
                     "krunner_placesrunner", "org.raye.store.runner"]

// Start's results from groups of matches: [{ runner, items: [...] }] ->
// [{ group, items }] in searchGroups order, the best match (first item of
// the first non-empty group in runner order, apps first) taken out on top.
// At most `perGroup` items in each group.
function groupResults(groups, perGroup) {
    var by = {}
    var best = null
    var order = ["apps", "settings", "answers", "files", "store", "other"]
    for (var g = 0; groups && g < groups.length; ++g) {
        var key = searchGroup(groups[g].runner)
        if (!by[key])
            by[key] = []
        by[key] = by[key].concat(groups[g].items || [])
    }
    for (var o = 0; o < order.length && !best; ++o) {
        var list = by[order[o]]
        if (list && list.length > 0 && order[o] !== "store") {
            best = list[0]
            by[order[o]] = list.slice(1)
        }
    }
    var out = []
    if (best)
        out.push({ group: "best", items: [best] })
    for (var s = 1; s < searchGroups.length; ++s) {
        var items = by[searchGroups[s]]
        if (items && items.length > 0)
            out.push({ group: searchGroups[s], items: items.slice(0, perGroup) })
    }
    return out
}


// New apps first (at most half the places), then recent files, then more
// new apps if files did not fill it. Items: { kind: "app" | "file", ... }.
// Most used apps, when given, come after the new apps, within the same half.
function recommended(newApps, recentFiles, count, mostUsed) {
    var apps = (newApps || []).slice(0, Math.ceil(count / 2))
    var used = mostUsed || []
    var files = recentFiles || []
    var out = apps.slice()
    var room = Math.ceil(count / 2)
    var u = 0
    for (; u < used.length && out.length < room; ++u)
        out.push(used[u])
    for (var j = 0; j < files.length && out.length < count; ++j)
        out.push(files[j])
    for (; u < used.length && out.length < count; ++u)
        out.push(used[u])
    for (var k = apps.length; newApps && k < newApps.length && out.length < count; ++k)
        out.push(newApps[k])
    return out
}

// The buttons next to power, from [Start] Folders: names Start knows, each
// once, in the order set. Keep the names with Folders in shellsettings.cpp.
var folderIcons = {
    settings: "settings-configure",
    files: "folder-symbolic",
    documents: "folder-documents-symbolic",
    downloads: "folder-download-symbolic",
    music: "folder-music-symbolic",
    pictures: "folder-pictures-symbolic",
    videos: "folder-videos-symbolic",
    network: "folder-network-symbolic",
    personal: "user-home-symbolic"
}

function folderButtons(keys) {
    var out = []
    for (var i = 0; keys && i < keys.length; ++i) {
        var k = String(keys[i])
        if (folderIcons.hasOwnProperty(k) && out.indexOf(k) < 0)
            out.push(k)
    }
    return out
}

function folderIcon(key) {
    return folderIcons.hasOwnProperty(key) ? folderIcons[key] : ""
}

// What FolderGlyph draws for a folder button. Settings keeps its gear
// icon; each folder gets its own outline, so Files and Downloads do not
// look the same.
var folderGlyphs = {
    files: "folder",
    documents: "document",
    downloads: "download",
    music: "music",
    pictures: "picture",
    videos: "video",
    network: "network",
    personal: "person"
}

function folderGlyph(key) {
    return folderGlyphs.hasOwnProperty(key) ? folderGlyphs[key] : ""
}

// The icons of a category tile, as on Windows: up to four apps in full;
// with more, three in full and the next four small in the fourth place.
// Pinned apps come first, in pin order, then the rest as sorted.
function categoryPreview(apps, pins) {
    var list = apps ? Array.prototype.slice.call(apps) : []
    var order = cleanPins(pins)
    var rank = function (a) {
        var i = order.indexOf(pinId(a.favoriteId))
        return i < 0 ? order.length : i
    }
    var ranked = list.map(function (a, i) { return { app: a, rank: rank(a), at: i } })
    ranked.sort(function (x, y) { return x.rank - y.rank || x.at - y.at })
    var sorted = ranked.map(function (x) { return x.app })
    if (sorted.length <= 4)
        return { big: sorted, small: [] }
    return { big: sorted.slice(0, 3), small: sorted.slice(3, 7) }
}


// Left edge of the popup when the taskbar is left aligned: lined up with
// the Start button, never closer than 12 px to the screen edge, never off
// the screen. Centred: in the middle of the screen.
function popupX(alignment, screenX, screenWidth, startButtonX, width) {
    var x = alignment === "left" ? Math.max(screenX + 12, startButtonX)
                                 : screenX + Math.round((screenWidth - width) / 2)
    return Math.max(screenX, Math.min(x, screenX + screenWidth - width))
}
