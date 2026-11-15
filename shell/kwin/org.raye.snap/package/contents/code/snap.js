// The geometry of Raye Snap, with no KWin in it, so it can be tested on
// its own: the layouts, where each zone falls in a screen's work area,
// which snap a window is in now, what Windows' arrow keys do from there,
// and the snap bar's trigger.
//
// Rectangles are plain objects {x, y, width, height}; zones are fractions
// of the work area.
//
// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// screens narrower than this get 4 layouts, wider ones 6
var wideScreen = 1400;
// how far a window may be from a zone and still count as snapped there
var tolerance = 8;
// the snap bar shows when the pointer drags a window this close to the top
var barTriggerHeight = 8;

function zone(x, y, w, h) {
    return { x: x, y: y, w: w, h: h };
}

// Windows' set, in Windows' order
var allLayouts = [
    { id: "halves", zones: [zone(0, 0, 1 / 2, 1), zone(1 / 2, 0, 1 / 2, 1)] },
    { id: "two-thirds", zones: [zone(0, 0, 2 / 3, 1), zone(2 / 3, 0, 1 / 3, 1)] },
    { id: "thirds", zones: [zone(0, 0, 1 / 3, 1), zone(1 / 3, 0, 1 / 3, 1), zone(2 / 3, 0, 1 / 3, 1)] },
    { id: "half-quarters", zones: [zone(0, 0, 1 / 2, 1), zone(1 / 2, 0, 1 / 2, 1 / 2), zone(1 / 2, 1 / 2, 1 / 2, 1 / 2)] },
    { id: "quarters", zones: [zone(0, 0, 1 / 2, 1 / 2), zone(1 / 2, 0, 1 / 2, 1 / 2), zone(0, 1 / 2, 1 / 2, 1 / 2), zone(1 / 2, 1 / 2, 1 / 2, 1 / 2)] },
    { id: "one-third", zones: [zone(0, 0, 1 / 3, 1), zone(1 / 3, 0, 2 / 3, 1)] }
];

var narrowIds = ["halves", "two-thirds", "half-quarters", "quarters"];

function layoutsFor(screenWidth) {
    if (screenWidth >= wideScreen) {
        return allLayouts;
    }
    return allLayouts.filter(function (l) { return narrowIds.indexOf(l.id) >= 0; });
}

function layoutById(id) {
    for (var i = 0; i < allLayouts.length; ++i) {
        if (allLayouts[i].id === id) {
            return allLayouts[i];
        }
    }
    return null;
}

// whole pixels; neighbours share their edge exactly, nothing is left over
function zoneRect(area, z) {
    var x0 = area.x + Math.round(z.x * area.width);
    var x1 = area.x + Math.round((z.x + z.w) * area.width);
    var y0 = area.y + Math.round(z.y * area.height);
    var y1 = area.y + Math.round((z.y + z.h) * area.height);
    return { x: x0, y: y0, width: x1 - x0, height: y1 - y0 };
}

// the snaps the arrow keys move between
var named = {
    left: zone(0, 0, 1 / 2, 1),
    right: zone(1 / 2, 0, 1 / 2, 1),
    topleft: zone(0, 0, 1 / 2, 1 / 2),
    topright: zone(1 / 2, 0, 1 / 2, 1 / 2),
    bottomleft: zone(0, 1 / 2, 1 / 2, 1 / 2),
    bottomright: zone(1 / 2, 1 / 2, 1 / 2, 1 / 2)
};

function near(a, b) {
    return Math.abs(a.x - b.x) <= tolerance && Math.abs(a.y - b.y) <= tolerance
        && Math.abs(a.width - b.width) <= tolerance && Math.abs(a.height - b.height) <= tolerance;
}

// which named snap a window's frame is in: "left", "topright"... or "none"
function classify(geometry, area) {
    for (var name in named) {
        if (near(geometry, zoneRect(area, named[name]))) {
            return name;
        }
    }
    return "none";
}

// What Windows' Win+arrow does from a state ("maximized", "none" or a
// named snap). key is "left", "right", "up" or "down"; prevScreen and
// nextScreen say whether there is a screen on that side.
// Returns { action: "snap", zone, screen: "same"|"prev"|"next" },
// { action: "maximize" | "restore" | "minimize" | "nothing" }.
function arrow(state, key, prevScreen, nextScreen) {
    function snap(z, screen) {
        return { action: "snap", zone: z, screen: screen || "same" };
    }
    function act(a) {
        return { action: a };
    }
    if (key === "left" || key === "right") {
        var toLeft = key === "left";
        var near_ = toLeft ? "left" : "right";
        var far = toLeft ? "right" : "left";
        var otherScreen = toLeft ? prevScreen : nextScreen;
        var otherName = toLeft ? "prev" : "next";
        switch (state) {
        case near_:
            // on to the next screen, or round to the other half
            return otherScreen ? snap(far, otherName) : snap(far);
        case far:
            return act("restore");
        case "top" + near_:
            return otherScreen ? snap("top" + far, otherName) : act("nothing");
        case "bottom" + near_:
            return otherScreen ? snap("bottom" + far, otherName) : act("nothing");
        case "top" + far:
            return snap("top" + near_);
        case "bottom" + far:
            return snap("bottom" + near_);
        default:
            return snap(near_);
        }
    }
    if (key === "up") {
        switch (state) {
        case "left": return snap("topleft");
        case "right": return snap("topright");
        case "bottomleft": return snap("left");
        case "bottomright": return snap("right");
        case "maximized": return act("nothing");
        default: return act("maximize");
        }
    }
    if (key === "down") {
        switch (state) {
        case "maximized": return act("restore");
        case "left": return snap("bottomleft");
        case "right": return snap("bottomright");
        case "topleft": return snap("left");
        case "topright": return snap("right");
        default: return act("minimize");
        }
    }
    return act("nothing");
}

// Win+Shift+Up: the same width and place, the work area's full height
function fullHeight(geometry, area) {
    return { x: geometry.x, y: area.y, width: geometry.width, height: area.height };
}

// the pointer drags a window into the snap bar's trigger: the top edge,
// middle third of the screen
function inBarTrigger(point, screen) {
    return point.y <= screen.y + barTriggerHeight
        && point.x >= screen.x + screen.width / 3
        && point.x <= screen.x + 2 * screen.width / 3;
}

// The window, restored, under the pointer: its title bar stays where it
// was grabbed, in proportion, as Windows does when a snapped window is
// dragged away.
function restoreUnderPointer(snapped, restored, pointer) {
    var fx = (pointer.x - snapped.x) / Math.max(1, snapped.width);
    var x = Math.round(pointer.x - fx * restored.width);
    var y = Math.round(pointer.y - Math.min(pointer.y - snapped.y, 16));
    return { x: x, y: y, width: restored.width, height: restored.height };
}

// The zones of a layout a snap leaves to fill, in order.
function freeZones(layout, filled) {
    var out = [];
    for (var i = 0; i < layout.zones.length; ++i) {
        if (filled.indexOf(i) < 0) {
            out.push(i);
        }
    }
    return out;
}

// The layout and zone of a named snap, so snap assist can offer the rest.
function layoutOfNamed(name) {
    switch (name) {
    case "left": return { layout: "halves", zone: 0 };
    case "right": return { layout: "halves", zone: 1 };
    case "topleft": return { layout: "quarters", zone: 0 };
    case "topright": return { layout: "quarters", zone: 1 };
    case "bottomleft": return { layout: "quarters", zone: 2 };
    case "bottomright": return { layout: "quarters", zone: 3 };
    }
    return null;
}

// Two snapped windows side by side: when one's shared edge moves (a
// resize), the other follows so they still meet. Returns the partner's
// new frame, or null when the two do not share that edge.
function partnerFrame(before, after, partner) {
    var r;
    var vertical = Math.abs(after.y - partner.y) <= tolerance && Math.abs(after.height - partner.height) <= tolerance;
    var horizontal = Math.abs(after.x - partner.x) <= tolerance && Math.abs(after.width - partner.width) <= tolerance;
    if (vertical && Math.abs(before.x + before.width - partner.x) <= tolerance) {
        // partner on the right; the shared edge is our right edge
        var edge = after.x + after.width;
        r = { x: edge, y: partner.y, width: partner.x + partner.width - edge, height: partner.height };
    } else if (vertical && Math.abs(partner.x + partner.width - before.x) <= tolerance) {
        r = { x: partner.x, y: partner.y, width: after.x - partner.x, height: partner.height };
    } else if (horizontal && Math.abs(before.y + before.height - partner.y) <= tolerance) {
        var bottom = after.y + after.height;
        r = { x: partner.x, y: bottom, width: partner.width, height: partner.y + partner.height - bottom };
    } else if (horizontal && Math.abs(partner.y + partner.height - before.y) <= tolerance) {
        r = { x: partner.x, y: partner.y, width: partner.width, height: after.y - partner.y };
    } else {
        return null;
    }
    if (r.width < 1 || r.height < 1) {
        return null;
    }
    return r;
}

// Where the layouts flyout opens: centred under the anchor (the Maximize
// button, or the pointer), kept inside the screen.
function flyoutPosition(anchor, size, screen) {
    var x = Math.round(anchor.x - size.width / 2);
    var y = Math.round(anchor.y + 8);
    x = Math.max(screen.x + 8, Math.min(x, screen.x + screen.width - size.width - 8));
    if (y + size.height > screen.y + screen.height - 8) {
        y = Math.round(anchor.y - 8 - size.height);
    }
    y = Math.max(screen.y + 8, y);
    return { x: x, y: y };
}
