// The tiles of Start in the Windows 10 look, as plain functions so they
// can be tested without Plasma: groups of tiles kept in [Start] Tiles, the
// four tile sizes, how a group packs its tiles, and moving, resizing and
// naming. The pins themselves stay in [Start] Pinned, so both looks show
// the same apps.
//
// [Start] Tiles, one group after another:
//   <name>|<id>=<size>,<id>=<size>;<name>|...
// The name is URI-encoded (it is the person's text); "@productivity" and
// "@explore" are Raye's own groups, named in the person's language.
.pragma library

var sizes = ["small", "medium", "wide", "large"]
// width and height of each size, in cells; a medium tile is 2 x 2
var spans = {
    small: { w: 1, h: 1 },
    medium: { w: 2, h: 2 },
    wide: { w: 4, h: 2 },
    large: { w: 4, h: 4 }
}
var groupCells = 6          // a group is three medium tiles wide
var cell = 50               // a small tile, with 4 px between two tiles
var gap = 4
var groupWidth = groupCells * (cell + gap) - gap
var groupGap = 32           // between two columns of groups

// Raye's first tiles, before the person changes anything
var defaultSizes = {
    "org.kde.dolphin.desktop": "wide",
    "org.raye.store.desktop": "wide"
}
var exploreIds = ["org.raye.store.desktop", "org.kde.gwenview.desktop", "org.kde.haruna.desktop",
                  "org.kde.elisa.desktop"]

function isSize(s) {
    return sizes.indexOf(s) >= 0
}

function tilePixels(size) {
    var sp = spans[isSize(size) ? size : "medium"]
    return { width: sp.w * (cell + gap) - gap, height: sp.h * (cell + gap) - gap }
}

function decodeName(text) {
    try {
        return decodeURIComponent(text)
    } catch (e) {
        return text
    }
}

// The groups the text says, as they are written: no checks against pins.
function parse(text) {
    var out = []
    var parts = String(text || "").split(";")
    for (var i = 0; i < parts.length; ++i) {
        var part = parts[i]
        if (part === "")
            continue
        var bar = part.indexOf("|")
        var name = bar >= 0 ? decodeName(part.substring(0, bar)) : ""
        var list = bar >= 0 ? part.substring(bar + 1) : part
        var tiles = []
        var entries = list.split(",")
        for (var j = 0; j < entries.length; ++j) {
            var e = entries[j]
            if (e === "")
                continue
            var eq = e.lastIndexOf("=")
            var id = eq >= 0 ? e.substring(0, eq) : e
            var size = eq >= 0 ? e.substring(eq + 1) : "medium"
            if (id === "")
                continue
            tiles.push({ id: id, size: isSize(size) ? size : "medium" })
        }
        out.push({ name: name, tiles: tiles })
    }
    return out
}

function serialize(groups) {
    var parts = []
    for (var i = 0; i < groups.length; ++i) {
        var g = groups[i]
        var tiles = []
        for (var j = 0; j < g.tiles.length; ++j)
            tiles.push(g.tiles[j].id + "=" + g.tiles[j].size)
        parts.push(encodeURIComponent(g.name || "") + "|" + tiles.join(","))
    }
    return parts.join(";")
}

// Raye's two first groups: Productivity, then Explore for the Store and
// the apps for pictures, videos and music.
function defaultGroups(pins) {
    var work = []
    var play = []
    for (var i = 0; i < pins.length; ++i) {
        var t = { id: pins[i], size: defaultSizes[pins[i]] || "medium" }
        if (exploreIds.indexOf(pins[i]) >= 0)
            play.push(t)
        else
            work.push(t)
    }
    var out = []
    if (work.length > 0)
        out.push({ name: "@productivity", tiles: work })
    if (play.length > 0)
        out.push({ name: "@explore", tiles: play })
    return out
}

// What Start shows: the groups of the text with only the pins in them,
// each pin once; a new pin goes at the end of the last group. With no
// text yet, Raye's first groups. Empty groups go.
function groups(text, pins) {
    pins = pins || []
    if (String(text || "") === "")
        return defaultGroups(pins)
    var seen = {}
    var out = []
    var parsed = parse(text)
    for (var i = 0; i < parsed.length; ++i) {
        var tiles = []
        for (var j = 0; j < parsed[i].tiles.length; ++j) {
            var t = parsed[i].tiles[j]
            if (pins.indexOf(t.id) < 0 || seen[t.id])
                continue
            seen[t.id] = true
            tiles.push({ id: t.id, size: t.size })
        }
        out.push({ name: parsed[i].name, tiles: tiles })
    }
    var added = []
    for (var k = 0; k < pins.length; ++k) {
        if (!seen[pins[k]]) {
            seen[pins[k]] = true
            added.push({ id: pins[k], size: "medium" })
        }
    }
    out = out.filter(function (g) { return g.tiles.length > 0 })
    if (added.length > 0) {
        if (out.length === 0)
            out.push({ name: "", tiles: [] })
        out[out.length - 1].tiles = out[out.length - 1].tiles.concat(added)
    }
    return out
}

function copy(groups) {
    return groups.map(function (g) {
        return { name: g.name, tiles: g.tiles.map(function (t) { return { id: t.id, size: t.size } }) }
    })
}

function find(groups, id) {
    for (var i = 0; i < groups.length; ++i)
        for (var j = 0; j < groups[i].tiles.length; ++j)
            if (groups[i].tiles[j].id === id)
                return { group: i, index: j }
    return null
}

function setSize(groups, id, size) {
    var out = copy(groups)
    var at = find(out, id)
    if (at && isSize(size))
        out[at.group].tiles[at.index].size = size
    return out
}

function rename(groups, index, name) {
    var out = copy(groups)
    if (index >= 0 && index < out.length)
        out[index].name = String(name || "").trim()
    return out
}

// Moves a tile to place `index` of group `toGroup`; a group past the last
// one is a new group. The group it leaves goes when it is empty.
function moveTile(groups, id, toGroup, index) {
    var out = copy(groups)
    var at = find(out, id)
    if (!at)
        return out
    var tile = out[at.group].tiles.splice(at.index, 1)[0]
    if (toGroup >= out.length) {
        out.push({ name: "", tiles: [tile] })
    } else {
        toGroup = Math.max(0, toGroup)
        // the place is counted without the tile, as it is now
        if (toGroup === at.group && index > at.index)
            index -= 1
        var list = out[toGroup].tiles
        index = Math.max(0, Math.min(index, list.length))
        list.splice(index, 0, tile)
    }
    return out.filter(function (g) { return g.tiles.length > 0 })
}

// Where each tile of a group goes, in cells of a groupCells-wide grid, the
// way Windows 10 packs them: medium, wide and large tiles start on even
// cells, and small tiles fill a 2 x 2 block four at a time.
//   returns { tiles: [{ id, size, x, y, w, h }], rows }
function pack(tiles, columns) {
    columns = columns || groupCells
    var used = []
    function free(x, y, w, h) {
        if (x + w > columns)
            return false
        for (var yy = y; yy < y + h; ++yy)
            for (var xx = x; xx < x + w; ++xx)
                if (used[yy] && used[yy][xx])
                    return false
        return true
    }
    function take(x, y, w, h) {
        for (var yy = y; yy < y + h; ++yy) {
            if (!used[yy])
                used[yy] = []
            for (var xx = x; xx < x + w; ++xx)
                used[yy][xx] = true
        }
    }
    function spot(w, h) {
        for (var y = 0; ; y += 2)
            for (var x = 0; x + w <= columns; x += 2)
                if (free(x, y, w, h))
                    return { x: x, y: y }
    }
    var out = []
    var block = null   // the 2 x 2 block small tiles are filling
    var rows = 0
    for (var i = 0; i < tiles.length; ++i) {
        var t = tiles[i]
        var size = isSize(t.size) ? t.size : "medium"
        var p
        if (size === "small") {
            if (block) {
                p = null
                for (var c = 0; c < 4 && !p; ++c) {
                    var bx = block.x + (c % 2), by = block.y + Math.floor(c / 2)
                    if (free(bx, by, 1, 1))
                        p = { x: bx, y: by }
                }
                if (!p)
                    block = null
            }
            if (!block) {
                block = spot(2, 2)
                p = { x: block.x, y: block.y }
            }
            take(p.x, p.y, 1, 1)
        } else {
            var sp = spans[size]
            p = spot(sp.w, sp.h)
            take(p.x, p.y, sp.w, sp.h)
        }
        var s = spans[size]
        out.push({ id: t.id, size: size, x: p.x, y: p.y, w: s.w, h: s.h })
        rows = Math.max(rows, p.y + s.h)
    }
    return { tiles: out, rows: rows }
}

// [Start] TileColumns: "1", "2" or "3" columns of groups; "auto" is two on
// a screen at least 1600 px wide, one below.
function columnsFor(setting, screenWidth) {
    var n = parseInt(setting, 10)
    if (n >= 1 && n <= 3)
        return n
    return screenWidth >= 1600 ? 2 : 1
}

// The columns that fit a width of tiles area, for the edge of Start the
// person drags.
function columnsForWidth(width) {
    var n = Math.round((width + groupGap) / (groupWidth + groupGap))
    return Math.max(1, Math.min(3, n))
}

// Which column of groups each group goes in: the shortest column so far,
// so the columns end at about the same height. heights: one per group.
function columnOf(heights, columns) {
    var out = []
    var fill = []
    for (var c = 0; c < columns; ++c)
        fill.push(0)
    for (var i = 0; i < heights.length; ++i) {
        var best = 0
        for (var k = 1; k < columns; ++k)
            if (fill[k] < fill[best])
                best = k
        out.push(best)
        fill[best] += heights[i]
    }
    return out
}
