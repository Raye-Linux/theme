// The Start applet's decisions, as plain functions so they can be tested
// without Plasma: which buttons the taskbar shows for each search style,
// how far the Start button moves so Start, Search, Task view and the
// apps sit in the middle of the screen, and where the Start popup opens
// and how big it is.
.pragma library

var buttonSize = 40         // eye, search icon and Task view buttons
var buttonGap = 4           // so the pitch matches the apps' 44
var searchBoxWidth = 232    // the search pill
var searchBoxHeight = 32
var searchLabelWidth = 104  // glass and "Search", the icon-and-label style
var edgeMargin = 8          // the group's margin when left aligned
var flyoutGap = 12          // between the taskbar and the popup
var startWidth = 680
var startWideWidth = 840
var wideScreen = 1600
var startMaxHeight = 760
var screenMargin = 24       // room kept above a popup on a short screen

// The search style of Settings: "box", "iconlabel", "icon" or "hidden";
// the first plan's "iconLabel" is read too. Anything else is the box.
function searchStyle(value) {
    var v = String(value || "")
    if (v === "iconLabel")
        return "iconlabel"
    if (v === "box" || v === "iconlabel" || v === "icon" || v === "hidden")
        return v
    return "box"
}

function searchWidth(style) {
    switch (searchStyle(style)) {
    case "hidden": return 0
    case "icon": return buttonSize
    case "iconlabel": return searchLabelWidth
    default: return searchBoxWidth
    }
}

// The width of the row of buttons: eye, search (per style), Task view,
// each followed by the 4 px gap so the apps carry on at the same pitch.
function rowWidth(style, taskView) {
    var w = buttonSize + buttonGap
    var s = searchWidth(style)
    if (s > 0)
        w += s + buttonGap
    if (taskView)
        w += buttonSize + buttonGap
    return w
}

// How much room the Start applet leaves before its buttons so the group
// (its row plus the taskbar's apps) is centred on the screen, or starts
// 8 px from the left edge. startX: where the applet begins on the screen
// (screen coordinates, without that room). Never negative: when the
// group is wider than what is left, it starts where the applet starts.
function leadingSpace(alignment, screenWidth, startX, rowW, appsW) {
    if (alignment === "left")
        return Math.max(0, edgeMargin - startX)
    var groupLeft = Math.round((screenWidth - (rowW + appsW)) / 2)
    return Math.max(0, groupLeft - startX)
}

// The popup: 680 wide, 840 on a screen at least 1600 wide, as tall as
// 760 or what the screen has above the taskbar less 24 px.
function popupSize(screenWidth, screenHeight, panelHeight) {
    var w = screenWidth >= wideScreen ? startWideWidth : startWidth
    w = Math.min(w, Math.max(0, screenWidth - 2 * edgeMargin))
    var h = Math.min(startMaxHeight, screenHeight - panelHeight - screenMargin)
    return { width: w, height: Math.max(0, h) }
}

// Where it opens (screen coordinates of its top-left corner): 12 px above
// the taskbar, centred on the screen, or at the left edge when the
// taskbar is left aligned; never off the screen.
//   screen: { x, y, width, height }, panelTop: the taskbar's top edge
function popupPosition(alignment, screen, panelTop, width, height) {
    var x = alignment === "left" ? screen.x + flyoutGap
                                 : screen.x + Math.round((screen.width - width) / 2)
    x = Math.max(screen.x, Math.min(x, screen.x + screen.width - width))
    var y = Math.max(screen.y, panelTop - flyoutGap - height)
    return { x: x, y: y }
}

// A key that starts a search when typed in Start: a character a person
// would type, not Tab, Enter, Escape or a lone modifier.
function isTypedText(text, ctrl, alt, meta) {
    if (ctrl || alt || meta)
        return false
    if (typeof text !== "string" || text.length === 0)
        return false
    var c = text.charCodeAt(0)
    return c >= 0x20 && c !== 0x7f
}

// --- the Windows 10 look --------------------------------------------------
// The buttons fill the taskbar's height and sit side by side, from the
// left edge: the eye, a wide square search box, Task view.
var w10 = {
    buttonWidth: 48,
    searchBoxWidth: 320,
    railWidth: 48,          // Start's slim column of icons on the left
    listWidth: 296,         // the list of apps
    tilesLeft: 8,           // room before and after the tiles
    tilesRight: 24,
    groupWidth: 320,        // tileslogic.js groupWidth
    groupGap: 32,
    maxHeight: 720
}

// Windows 10 has no icon-and-label search: that style is the box there
function searchWidth10(style) {
    switch (searchStyle(style)) {
    case "hidden": return 0
    case "icon": return w10.buttonWidth
    default: return w10.searchBoxWidth
    }
}

function rowWidth10(style, taskView) {
    return w10.buttonWidth + searchWidth10(style) + (taskView ? w10.buttonWidth : 0)
}

// Start: the rail, the list and the columns of tile groups, as tall as 720
// or what is above the taskbar; it sits on the taskbar, against the left edge.
function popupSize10(screenWidth, screenHeight, panelHeight, columns) {
    columns = Math.max(1, columns || 1)
    var w = w10.railWidth + w10.listWidth + w10.tilesLeft + w10.tilesRight
            + columns * w10.groupWidth + (columns - 1) * w10.groupGap
    w = Math.min(w, Math.max(0, screenWidth))
    var h = Math.min(w10.maxHeight, screenHeight - panelHeight)
    return { width: w, height: Math.max(0, h) }
}

function popupPosition10(screen, panelTop, width, height) {
    return { x: screen.x, y: Math.max(screen.y, panelTop - height) }
}
