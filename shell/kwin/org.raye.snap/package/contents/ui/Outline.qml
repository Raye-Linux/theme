// KWin's quick tile outline (a window dragged to an edge, a corner, or the
// top to maximise), drawn the Raye way: the same sheet as the snap bar's
// preview, so both say "it goes here" alike. kwinrc points KWin at this
// file ([Outline] QmlPath); KWin gives it the context property `outline`
// and stacks it right above the window being moved.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Window
import org.kde.kwin

Window {
    id: outlineWindow

    flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint
    color: "transparent"

    x: outline.unifiedGeometry.x
    y: outline.unifiedGeometry.y
    width: outline.unifiedGeometry.width
    height: outline.unifiedGeometry.height

    // kept up while a plain sheet fades, and a moment empty after: KWin
    // fades a window out from its last picture, which would be the moved
    // window's picture fading on its own. KWin may hide it sooner.
    visible: outline.active || sheet.busy || emptied.running

    Timer {
        id: emptied
        interval: 80
    }

    function local(r) {
        return Qt.rect(r.x - outline.unifiedGeometry.x, r.y - outline.unifiedGeometry.y, r.width, r.height);
    }

    function open() {
        // it leaves from the window's frame when KWin gives one
        const from = outline.visualParentGeometry;
        sheet.land(local(outline.geometry), from.width > 0 && from.height > 0 ? local(from) : null);
    }

    Component.onCompleted: {
        if (outline.active) {
            open();
        }
    }

    Connections {
        target: outline
        function onActiveChanged() {
            if (!outline.active) {
                emptied.restart();
            }
            if (outline.active) {
                outlineWindow.open();
            } else if (sheet.moved) {
                // still dragging: away from the edge
                sheet.land(null);
            } else {
                // dropped: the window is now where the sheet was
                sheet.clear();
            }
        }
        function onGeometryChanged() {
            if (outline.active) {
                sheet.land(outlineWindow.local(outline.geometry));
            }
        }
        // the window under it moved, so the sheet's place in it did: jump,
        // an animation would start from the wrong spot
        function onUnifiedGeometryChanged() {
            if (outline.active) {
                const r = outlineWindow.local(outline.geometry);
                sheet.land(r, sheet.grown(r));
            }
        }
    }

    onSceneGraphError: (error, message) => {
        console.warn("outline: scene graph error:", message);
    }

    Landing {
        id: sheet
        anchors.fill: parent
        readonly property var moved: Workspace.activeWindow && Workspace.activeWindow.move ? Workspace.activeWindow : null
        window: moved
        hole: moved ? outlineWindow.local(moved.frameGeometry) : Qt.rect(0, 0, 0, 0)
    }
}
