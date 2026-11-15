// Sizes of the Raye window switcher: how many cards fit in a row, and
// how big a window's thumbnail is. Plain functions, so they can be tested
// without KWin.
.pragma library

var cardPadding = 8      // inside a card, around its title and thumbnail
var thumbWidth = 220
var thumbMaxHeight = 150
var titleHeight = 20     // icon 16 and a Caption line
var titleGap = 8
var gap = 12             // between cards
var platePadding = 12    // inside the plate, with the frame's 4 px: 16
var sideRoom = 160       // the plate keeps this much of the screen width free
var outlineRoom = 6         // room around the cards for the selected one's outline

function cardWidth() {
    return thumbWidth + 2 * cardPadding
}

function cardHeight() {
    return cardPadding + titleHeight + titleGap + thumbMaxHeight + cardPadding
}

// cards per row: as many as fit in the screen width less sideRoom, at
// least one, never more than there are windows
function columns(count, screenWidth) {
    if (count <= 0)
        return 0
    var room = screenWidth - sideRoom - 2 * platePadding
    var fit = Math.floor((room + gap) / (cardWidth() + gap))
    return Math.max(1, Math.min(count, fit))
}

function rows(count, screenWidth) {
    var c = columns(count, screenWidth)
    return c === 0 ? 0 : Math.ceil(count / c)
}

// a window's thumbnail: 220 wide, as tall as its shape asks but no more
// than 150 (a tall window gets narrower instead)
function thumbSize(windowWidth, windowHeight) {
    if (!(windowWidth > 0) || !(windowHeight > 0))
        return { width: thumbWidth, height: Math.round(thumbWidth * 9 / 16) }
    var h = thumbWidth * windowHeight / windowWidth
    if (h <= thumbMaxHeight)
        return { width: thumbWidth, height: Math.round(h) }
    return { width: Math.round(thumbMaxHeight * windowWidth / windowHeight), height: thumbMaxHeight }
}

// the size of the plate's content for count windows, rows above maxRows
// scroll
function contentSize(count, screenWidth, screenHeight) {
    var c = columns(count, screenWidth)
    var r = rows(count, screenWidth)
    var maxRows = Math.max(1, Math.floor((screenHeight * 0.8 - 2 * platePadding + gap) / (cardHeight() + gap)))
    var shownRows = Math.min(r, maxRows)
    return {
        width: c * cardWidth() + Math.max(0, c - 1) * gap,
        height: shownRows * cardHeight() + Math.max(0, shownRows - 1) * gap
    }
}
