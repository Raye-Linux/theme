// Where a dragged window will land. The snap bar's preview and KWin's own
// quick tile outline (Outline.qml) both draw it, so dragging to an edge
// and dragging onto the snap bar show the same thing.
//
// With the window being moved known, the place shows that window itself:
// its live picture flies from the window to the place, on a sheet that is
// exactly the frame it will get. The picture is never scaled: KWin's
// picture of a window has the window's own pixels, and stretched to a
// bigger zone it goes soft. It sits at the top left of the place, one
// pixel for one, and what does not fit is cut. The window left under the
// pointer is covered while its picture shows, so it never shows twice. Without a picture (a minimised window, or no window known) the
// place is the frosted sheet alone, and it is not drawn inside `hole`,
// the moved window's frame, which KWin keeps under it.
//
// One shape that travels: pointing at another zone moves it there instead
// of fading one out and another in.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Window
import org.kde.kwin
import org.raye.ui

Item {
    id: root
    objectName: "landing"

    property bool shown: false
    // the place it stands for, in this item's coordinates
    property rect place
    // the frame of the window being moved, in this item's coordinates
    property rect hole
    // the window being moved
    property var window: null
    // the screen's pixels per point: the picture lands on whole pixels
    property real pixelRatio: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1

    // KWin can draw its picture: a minimised window has none to give
    readonly property bool live: !!window && !window.minimized
        && window.frameGeometry.width > 0 && window.frameGeometry.height > 0
    // a place shows inside its zone's edges; the window's own picture shows
    // on the exact frame it will get
    readonly property int inset: live ? 0 : 6
    // something is still on screen (the sheet may be on its way back)
    readonly property bool busy: sheet.visible
    readonly property bool hasHole: hole.width > 0 && hole.height > 0

    readonly property alias sheet: sheet

    // Puts the sheet on rect (null: nothing promised). With from, it starts
    // there; without, it leaves from the window the first time, or grows out
    // of the middle of the place when there is no window. The window's
    // picture never goes back into the window: it is gone at once, so it
    // is never seen twice (KWin's outline and the snap bar each have one).
    function land(rect, from) {
        if (!rect) {
            if (live) {
                clear();
            } else {
                shown = false;
            }
            return;
        }
        const r = grown(rect);
        if (!from && !shown && live && hasHole && !sheet.visible) {
            from = hole;
        }
        // from the window to its place is a longer way than zone to zone
        sheet.duration = shown ? Tokens.durationFast : Tokens.durationNormal;
        if (!shown || from) {
            sheet.moving = false;
            if (from) {
                sheet.x = from.x;
                sheet.y = from.y;
                sheet.width = from.width;
                sheet.height = from.height;
            } else if (!sheet.visible) {
                sheet.x = r.x + r.width * 0.06;
                sheet.y = r.y + r.height * 0.06;
                sheet.width = r.width * 0.88;
                sheet.height = r.height * 0.88;
            }
            sheet.moving = true;
        }
        place = r;
        shown = true;
        sheet.x = r.x;
        sheet.y = r.y;
        sheet.width = r.width;
        sheet.height = r.height;
    }

    // gone at once: the window has just been put where the sheet was
    function clear() {
        sheet.moving = false;
        sheet.quick = true;
        shown = false;
        sheet.quick = false;
    }

    // rect with the inset taken off, for a jump straight to it
    function grown(rect) {
        return Qt.rect(rect.x + inset, rect.y + inset, rect.width - 2 * inset, rect.height - 2 * inset);
    }

    // Where the sheet is: what is drawn below follows it.
    Item {
        id: sheet
        objectName: "landingSheet"
        property bool moving: true
        property int duration: Tokens.durationFast
        property bool quick: false
        opacity: root.shown ? 1 : 0
        visible: opacity > 0

        // the picture shows at once: it starts on the window, pixel for pixel,
        // and a fade would show both of them half way
        Behavior on opacity { enabled: !sheet.quick && !root.live; NumberAnimation { duration: root.shown ? Tokens.durationFaster : sheet.duration } }
        Behavior on x { enabled: sheet.moving; NumberAnimation { duration: sheet.duration; easing.type: Easing.OutCubic } }
        Behavior on y { enabled: sheet.moving; NumberAnimation { duration: sheet.duration; easing.type: Easing.OutCubic } }
        Behavior on width { enabled: sheet.moving; NumberAnimation { duration: sheet.duration; easing.type: Easing.OutCubic } }
        Behavior on height { enabled: sheet.moving; NumberAnimation { duration: sheet.duration; easing.type: Easing.OutCubic } }
    }

    // on the nearest whole pixel of the screen
    function onPixel(v) {
        return Math.round(v * pixelRatio) / pixelRatio;
    }

    // The window's picture on the sheet, one pixel for one: its top left on
    // the sheet's, cut where the sheet ends. frameSize is the whole picture.
    readonly property size frameSize: live ? Qt.size(window.frameGeometry.width, window.frameGeometry.height) : Qt.size(0, 0)
    readonly property rect picture: {
        if (!live) {
            return Qt.rect(0, 0, 0, 0);
        }
        const x = onPixel(sheet.x), y = onPixel(sheet.y);
        return Qt.rect(x, y, Math.max(0, Math.min(frameSize.width, sheet.x + sheet.width - x)),
                       Math.max(0, Math.min(frameSize.height, sheet.y + sheet.height - y)));
    }

    // the accent edge that says "here", with light catching it like on the
    // windows it stands for
    component Edge: Rectangle {
        radius: Tokens.radiusWindow
        color: "transparent"
        border.width: 2
        border.color: Qt.alpha(Tokens.accent, 0.85)

        Rectangle {
            anchors.fill: parent
            anchors.margins: parent.border.width
            radius: parent.radius - parent.border.width
            color: "transparent"
            border.width: 1
            border.color: Tokens.edgeLit
        }
    }

    // glass over whatever is behind, barely tinted
    component Glass: Rectangle {
        radius: Tokens.radiusWindow
        color: Qt.tint(Qt.alpha(Tokens.bgTertiary, 0.5), Qt.alpha(Tokens.accent, 0.12))
        Edge {
            anchors.fill: parent
        }
    }

    // --- with the window's picture ---------------------------------------------

    Item {
        anchors.fill: parent
        visible: root.live && sheet.visible

        // the window left under the pointer: it is the picture now
        Rectangle {
            objectName: "landingVeil"
            // its outline is drawn just outside the frame
            x: root.hole.x - 2
            y: root.hole.y - 2
            width: root.hole.width + 4
            height: root.hole.height + 4
            visible: root.hasHole
            radius: Tokens.radiusWindow + 2
            // solid: a window seen through it would be a second copy
            color: Tokens.bgTertiary
            opacity: sheet.opacity
        }

        Item {
            x: sheet.x
            y: sheet.y
            width: sheet.width
            height: sheet.height
            opacity: sheet.opacity

            // a soft shadow under the frame: a few rings, nothing to blur
            Repeater {
                model: 4
                Rectangle {
                    required property int index
                    x: -(index + 1) * 3
                    y: -(index + 1) * 3 + 6
                    width: parent.width + 2 * (index + 1) * 3
                    height: parent.height + 2 * (index + 1) * 3
                    radius: Tokens.radiusWindow + (index + 1) * 3
                    color: "transparent"
                    border.width: 3
                    border.color: Qt.alpha(Tokens.shadow, 0.16 - index * 0.04)
                }
            }

            // the rest of the frame, solid: the veiled window must not show
            // through next to its own picture
            Rectangle {
                objectName: "landingGlass"
                anchors.fill: parent
                radius: Tokens.radiusWindow
                color: Qt.tint(Tokens.bgTertiary, Qt.alpha(Tokens.accent, 0.1))
            }
        }

        // Only while shown: KWin draws the picture of a window for as long
        // as an item shows it, and nothing once it is gone.
        Loader {
            active: root.live && sheet.visible
            x: root.picture.x
            y: root.picture.y
            width: root.picture.width
            height: root.picture.height
            clip: true
            opacity: sheet.opacity
            sourceComponent: Item {
                objectName: "landingPicture"
                readonly property var client: thumbnail.client
                readonly property alias thumbnail: thumbnail

                WindowThumbnail {
                    id: thumbnail
                    width: root.frameSize.width
                    height: root.frameSize.height
                    client: root.window
                }
            }
        }

        Edge {
            x: sheet.x
            y: sheet.y
            width: sheet.width
            height: sheet.height
            opacity: sheet.opacity
        }
    }

    // --- without: the sheet alone, around the window ------------------------------

    // The area less the hole, as four bands around it (one band, the whole
    // area, with no hole). Each shows the sheet through a rectangular clip:
    // plain clipping, no layer and no shader, so it costs next to nothing.
    readonly property var bands: {
        const w = width, h = height, c = hole;
        if (c.width <= 0 || c.height <= 0) {
            return [Qt.rect(0, 0, w, h), Qt.rect(0, 0, 0, 0), Qt.rect(0, 0, 0, 0), Qt.rect(0, 0, 0, 0)];
        }
        const top = Math.max(0, c.y), bottom = Math.min(h, c.y + c.height);
        const left = Math.max(0, c.x), right = Math.min(w, c.x + c.width);
        return [Qt.rect(0, 0, w, top),
                Qt.rect(0, bottom, w, Math.max(0, h - bottom)),
                Qt.rect(0, top, left, Math.max(0, bottom - top)),
                Qt.rect(right, top, Math.max(0, w - right), Math.max(0, bottom - top))];
    }

    // always four, so a moving window only moves them: a model that changed
    // on every step of the drag would build them anew each time
    Repeater {
        model: 4
        delegate: Item {
            id: band
            required property int index
            readonly property rect area: root.bands[index]
            objectName: "landingBand"
            x: area.x
            y: area.y
            width: area.width
            height: area.height
            clip: true
            visible: !root.live && sheet.visible && width > 0 && height > 0

            Glass {
                x: sheet.x - band.x
                y: sheet.y - band.y
                width: sheet.width
                height: sheet.height
                opacity: sheet.opacity
            }
        }
    }
}
