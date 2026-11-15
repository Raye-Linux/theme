// The snap layouts as small screens: each layout a 64 x 44 miniature of
// its zones. Pointing at a zone lights it in the accent, a click picks it.
// With `numbered` (opened from the keyboard) each layout shows its number;
// after a layout's number, its zones show theirs.
//
// Used by the flyout (rows of 3 or 2) and by the snap bar (one row).
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.raye.ui

FocusScope {
    id: root

    // the layouts of snap.js
    property var layouts: []
    property int columns: layouts.length > 4 ? 3 : 2
    property bool numbered: false
    // a zone lit from outside: the snap bar while a window is dragged over it
    property int litLayout: -1
    property int litZone: -1
    // with numbers: the layout picked by its number, whose zones get numbers
    property int pickedLayout: -1

    readonly property int cellWidth: 64
    readonly property int cellHeight: 44
    readonly property int zoneGap: 4
    readonly property int spacing: 12
    readonly property int padding: 12
    readonly property int rows: Math.ceil(layouts.length / Math.max(1, columns))

    signal picked(int layout, int zone)
    signal cancelled()

    implicitWidth: 2 * padding + columns * cellWidth + (columns - 1) * spacing
    implicitHeight: 2 * padding + rows * cellHeight + (rows - 1) * spacing
    width: implicitWidth
    height: implicitHeight

    Accessible.role: Accessible.List
    Accessible.name: i18nd("raye_shell_snap", "Snap layouts")

    // the zone under a point of this item, or null
    function zoneAt(px, py) {
        for (let l = 0; l < grid.count; ++l) {
            const cell = grid.itemAt(l);
            if (!cell) {
                continue;
            }
            const p = cell.mapFromItem(root, px, py);
            for (let z = 0; z < cell.zoneCount; ++z) {
                const r = cell.zoneRect(z);
                if (p.x >= r.x - zoneGap / 2 && p.x < r.x + r.width + zoneGap / 2
                        && p.y >= r.y - zoneGap / 2 && p.y < r.y + r.height + zoneGap / 2) {
                    return { layout: l, zone: z };
                }
            }
        }
        return null;
    }

    // Inside the bar, the zone nearest to a point, or null outside it: the
    // gaps between zones and layouts are no dead spots, so the preview does
    // not blink out while the pointer crosses them.
    function nearestZoneAt(px, py) {
        if (px < 0 || py < 0 || px >= width || py >= height) {
            return null;
        }
        let best = null, bestDistance = Infinity;
        for (let l = 0; l < grid.count; ++l) {
            const cell = grid.itemAt(l);
            if (!cell) {
                continue;
            }
            const p = cell.mapFromItem(root, px, py);
            for (let z = 0; z < cell.zoneCount; ++z) {
                const r = cell.zoneRect(z);
                const dx = Math.max(r.x - p.x, 0, p.x - (r.x + r.width));
                const dy = Math.max(r.y - p.y, 0, p.y - (r.y + r.height));
                const d = dx * dx + dy * dy;
                if (d < bestDistance) {
                    bestDistance = d;
                    best = { layout: l, zone: z };
                }
            }
        }
        return best;
    }

    Keys.onPressed: event => {
        const n = event.key - Qt.Key_1;
        if (event.key === Qt.Key_Escape) {
            if (pickedLayout >= 0) {
                pickedLayout = -1;
            } else {
                cancelled();
            }
            event.accepted = true;
        } else if (numbered && n >= 0 && n < 9) {
            if (pickedLayout < 0 && n < layouts.length) {
                pickedLayout = n;
            } else if (pickedLayout >= 0 && n < layouts[pickedLayout].zones.length) {
                picked(pickedLayout, n);
            }
            event.accepted = true;
        }
    }

    Repeater {
        id: grid
        model: root.layouts

        delegate: Item {
            id: cell
            required property var modelData
            required property int index
            readonly property int zoneCount: modelData.zones.length
            x: root.padding + (index % root.columns) * (root.cellWidth + root.spacing)
            y: root.padding + Math.floor(index / root.columns) * (root.cellHeight + root.spacing)
            width: root.cellWidth
            height: root.cellHeight
            opacity: root.pickedLayout >= 0 && root.pickedLayout !== index ? 0.4 : 1
            Behavior on opacity { NumberAnimation { duration: Tokens.durationFaster } }

            // zones touch the miniature's edge; inside, they keep zoneGap apart
            function zoneRect(i) {
                const z = modelData.zones[i];
                const g = root.zoneGap / 2;
                const x0 = Math.round(z.x * width) + (z.x > 0 ? g : 0);
                const x1 = Math.round((z.x + z.w) * width) - (z.x + z.w < 0.999 ? g : 0);
                const y0 = Math.round(z.y * height) + (z.y > 0 ? g : 0);
                const y1 = Math.round((z.y + z.h) * height) - (z.y + z.h < 0.999 ? g : 0);
                return Qt.rect(x0, y0, x1 - x0, y1 - y0);
            }

            Repeater {
                model: cell.modelData.zones
                delegate: Rectangle {
                    id: zoneItem
                    required property int index
                    readonly property rect r: cell.zoneRect(index)
                    readonly property bool lit: mouse.containsMouse
                        || (root.litLayout === cell.index && root.litZone === index)
                    objectName: "zone-" + cell.index + "-" + index
                    x: r.x
                    y: r.y
                    width: r.width
                    height: r.height
                    radius: Tokens.radiusControl
                    color: lit ? Tokens.accent : Qt.alpha(Tokens.textPrimary, mouse.pressed ? 0.2 : 0.12)
                    border.width: lit ? 0 : 1
                    border.color: Qt.alpha(Tokens.textPrimary, 0.08)
                    Behavior on color { ColorAnimation { duration: Tokens.durationFaster } }

                    Text {
                        anchors.centerIn: parent
                        visible: root.numbered && root.pickedLayout === cell.index
                        text: zoneItem.index + 1
                        font: Tokens.caption
                        color: zoneItem.lit ? Tokens.textOnAccent : Tokens.textPrimary
                    }

                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        anchors.margins: -root.zoneGap / 2
                        hoverEnabled: true
                        onClicked: root.picked(cell.index, zoneItem.index)
                    }
                }
            }

            // the layout's number, at its corner
            Rectangle {
                visible: root.numbered && root.pickedLayout < 0
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: -4
                width: 16
                height: 16
                radius: 8
                color: Tokens.accent
                Text {
                    anchors.centerIn: parent
                    text: cell.index + 1
                    font: Tokens.caption
                    color: Tokens.textOnAccent
                }
            }
        }
    }
}
