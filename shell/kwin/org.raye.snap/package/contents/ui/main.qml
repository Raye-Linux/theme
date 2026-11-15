// Raye Snap: Windows' snapping on top of KWin.
//
// - Win+Left/Right/Up/Down move a window the way Windows does (halves,
//   quarters, maximise, restore, minimise, the next screen);
//   Win+Shift+Up stretches it to the full height; Win+Home minimises the
//   other windows, and brings them back.
// - Win+Z opens the snap layouts at the window's Maximize button, with
//   numbers; resting on Maximize does the same under the pointer (the
//   decoration asks org.raye.Snap, whose relay calls "Raye Snap Layouts
//   Under Pointer").
// - Dragging a window to the top middle of the screen slides down the
//   snap bar with the same layouts (SnapBar). The zone under the pointer
//   shows full size on the screen, with the window itself in it.
// - After a snap, snap assist offers the other windows for the free zones
//   (SnapAssist), and two windows snapped together resize together
//   (ResizePartner).
// Edge snapping while dragging stays KWin's own.
//
// The geometry is in code/snap.js. Settings: [Script-org.raye.snap] in kwinrc.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Window
import org.kde.kwin
import org.kde.plasma.core as PlasmaCore
import org.raye.ui
import "../code/snap.js" as Snap

Item {
    id: root

    property bool snapBarOn: true
    property bool snapAssistOn: true
    property bool resizePartnerOn: true
    property bool onHoverOn: true

    // read again when Settings changes them (KWin reloads its config)
    function readSettings() {
        snapBarOn = KWin.readConfig("SnapBar", true);
        snapAssistOn = KWin.readConfig("SnapAssist", true);
        resizePartnerOn = KWin.readConfig("ResizePartner", true);
        onHoverOn = KWin.readConfig("SnapLayoutsOnHover", true);
    }

    // frames to go back to, by window id: windows this script snapped
    property var restoreFrames: ({})
    // windows snapped together through a layout: id -> { group, window }
    property var groups: ({})
    property int groupSerial: 0
    // what Win+Home minimised, to bring back on the next press
    property var minimizedByUs: []

    // --- helpers -------------------------------------------------------------

    function key(w) {
        return w ? w.internalId.toString() : "";
    }

    function plain(r) {
        return { x: Math.round(r.x), y: Math.round(r.y), width: Math.round(r.width), height: Math.round(r.height) };
    }

    function workArea(output) {
        return plain(Workspace.clientArea(KWin.MaximizeArea, output, Workspace.currentDesktop));
    }

    function snappable(w) {
        return w && w.normalWindow && !w.fullScreen && !w.minimized && w.moveable && w.resizeable;
    }

    function stateOf(w) {
        if (w.maximizeMode === 3) {
            return "maximized";
        }
        return Snap.classify(plain(w.frameGeometry), workArea(w.output));
    }

    // the screens left to right, and the one beside a window's
    function neighbour(w, step) {
        const screens = Workspace.screens.slice().sort((a, b) => a.geometry.x - b.geometry.x);
        const i = screens.indexOf(w.output);
        const j = i + step;
        return i >= 0 && j >= 0 && j < screens.length ? screens[j] : null;
    }

    function rememberFrame(w) {
        const id = key(w);
        if (restoreFrames[id] === undefined && stateOf(w) === "none") {
            const frames = restoreFrames;
            frames[id] = plain(w.frameGeometry);
            restoreFrames = frames;
        }
    }

    function forget(w) {
        const id = key(w);
        const frames = restoreFrames;
        delete frames[id];
        restoreFrames = frames;
        const g = groups;
        delete g[id];
        groups = g;
    }

    // a window moved on its own leaves its group: it no longer resizes with the others
    function leaveGroup(w) {
        const g = groups;
        delete g[key(w)];
        groups = g;
    }

    function place(w, rect) {
        rememberFrame(w);
        if (w.maximizeMode !== 0) {
            w.setMaximize(false, false);
        }
        if (w.tile) {
            w.tile = null;
        }
        w.frameGeometry = Qt.rect(rect.x, rect.y, rect.width, rect.height);
    }

    function restore(w) {
        const id = key(w);
        const frame = restoreFrames[id];
        if (w.maximizeMode !== 0) {
            w.setMaximize(false, false);
        }
        if (w.tile) {
            w.tile = null;
        }
        if (frame !== undefined) {
            w.frameGeometry = Qt.rect(frame.x, frame.y, frame.width, frame.height);
        }
        forget(w);
    }

    // --- Windows keys -----------------------------------------------------------

    function arrowKey(direction) {
        const w = Workspace.activeWindow;
        if (!snappable(w)) {
            return;
        }
        const state = stateOf(w);
        leaveGroup(w);
        const step = Snap.arrow(state, direction, neighbour(w, -1) !== null, neighbour(w, 1) !== null);
        switch (step.action) {
        case "snap": {
            const output = step.screen === "prev" ? neighbour(w, -1) : step.screen === "next" ? neighbour(w, 1) : w.output;
            const area = workArea(output);
            place(w, Snap.zoneRect(area, Snap.named[step.zone]));
            const of = Snap.layoutOfNamed(step.zone);
            // from the keyboard: offer the other half, not the other quarters
            if (of && of.layout === "halves" && state === "none") {
                afterSnap(w, Snap.layoutById(of.layout), of.zone, area, output);
            }
            break;
        }
        case "maximize":
            rememberFrame(w);
            if (w.tile) {
                w.tile = null;
            }
            w.setMaximize(true, true);
            break;
        case "restore":
            restore(w);
            break;
        case "minimize":
            w.minimized = true;
            break;
        }
    }

    function fullHeight() {
        const w = Workspace.activeWindow;
        if (!snappable(w)) {
            return;
        }
        place(w, Snap.fullHeight(plain(w.frameGeometry), workArea(w.output)));
    }

    function minimizeOthers() {
        const active = Workspace.activeWindow;
        if (minimizedByUs.length > 0) {
            for (const w of minimizedByUs) {
                if (Workspace.windows.indexOf(w) >= 0) {
                    w.minimized = false;
                }
            }
            minimizedByUs = [];
            if (active) {
                Workspace.activeWindow = active;
            }
            return;
        }
        const list = [];
        for (const w of Workspace.windows) {
            if (w !== active && w.normalWindow && w.minimizable && !w.minimized
                    && (w.onAllDesktops || w.desktops.indexOf(Workspace.currentDesktop) >= 0)
                    && (!active || w.output === active.output)) {
                w.minimized = true;
                list.push(w);
            }
        }
        minimizedByUs = list;
    }

    // --- layouts: flyout, bar, assist ----------------------------------------------

    function placeInLayout(w, layout, zoneIndex, output) {
        const area = workArea(output);
        place(w, Snap.zoneRect(area, layout.zones[zoneIndex]));
        afterSnap(w, layout, zoneIndex, area, output);
    }

    // a window snapped into a layout: join it to a group, offer the rest
    function afterSnap(w, layout, zoneIndex, area, output) {
        groupSerial += 1;
        const g = groups;
        g[key(w)] = { group: groupSerial, window: w };
        groups = g;
        if (!snapAssistOn) {
            return;
        }
        const free = Snap.freeZones(layout, [zoneIndex]);
        if (free.length > 0) {
            assist.start(w, layout, [zoneIndex], area, output, groupSerial);
        }
    }

    function openLayouts(underPointer) {
        let w = Workspace.activeWindow;
        let anchor;
        if (underPointer) {
            if (!onHoverOn) {
                return;
            }
            const at = Workspace.windowAt(Workspace.cursorPos, 1);
            if (at && at.length > 0) {
                w = at[0];
            }
            anchor = Workspace.cursorPos;
        }
        if (!snappable(w)) {
            return;
        }
        if (!underPointer) {
            // where the Maximize button is: right of the title bar, left of Close
            const f = w.frameGeometry;
            anchor = Qt.point(f.x + f.width - 46 - 23, f.y + 32);
        }
        flyout.open(w, anchor, !underPointer);
    }

    ShortcutHandler {
        name: "Raye Snap Layouts"
        text: i18nd("raye_shell_snap", "Open snap layouts")
        sequence: "Meta+Z"
        onActivated: root.openLayouts(false)
    }
    ShortcutHandler {
        name: "Raye Snap Layouts Under Pointer"
        text: i18nd("raye_shell_snap", "Open snap layouts under the pointer")
        sequence: ""
        onActivated: root.openLayouts(true)
    }
    ShortcutHandler {
        name: "Raye Snap Left"
        text: i18nd("raye_shell_snap", "Snap the window to the left")
        sequence: "Meta+Left"
        onActivated: root.arrowKey("left")
    }
    ShortcutHandler {
        name: "Raye Snap Right"
        text: i18nd("raye_shell_snap", "Snap the window to the right")
        sequence: "Meta+Right"
        onActivated: root.arrowKey("right")
    }
    ShortcutHandler {
        name: "Raye Snap Up"
        text: i18nd("raye_shell_snap", "Maximize or snap the window up")
        sequence: "Meta+Up"
        onActivated: root.arrowKey("up")
    }
    ShortcutHandler {
        name: "Raye Snap Down"
        text: i18nd("raye_shell_snap", "Restore, minimize or snap the window down")
        sequence: "Meta+Down"
        onActivated: root.arrowKey("down")
    }
    ShortcutHandler {
        name: "Raye Snap Full Height"
        text: i18nd("raye_shell_snap", "Stretch the window to the full height")
        sequence: "Meta+Shift+Up"
        onActivated: root.fullHeight()
    }
    ShortcutHandler {
        name: "Raye Minimize Others"
        text: i18nd("raye_shell_snap", "Minimize all but the active window")
        sequence: "Meta+Home"
        onActivated: root.minimizeOthers()
    }

    // --- following windows: drags for the bar, resizes for partners --------------

    property var dragged: null
    // how the drag began: put back if it is cancelled
    property var dragStart: null
    // KWin moved the window after its last step. At the end of a drag that
    // is Escape putting it back, not a drop.
    property bool movedSinceStep: false
    property var resizeFrom: null
    property var watched: ({})

    function watch(w) {
        if (!w || !w.normalWindow || watched[key(w)]) {
            return;
        }
        const list = watched;
        list[key(w)] = true;
        watched = list;
        w.interactiveMoveResizeStarted.connect(() => root.moveResizeStarted(w));
        w.interactiveMoveResizeStepped.connect(() => root.moveResizeStepped(w));
        w.interactiveMoveResizeFinished.connect(() => root.moveResizeFinished(w));
        if (w.frameGeometryAboutToChange) {
            w.frameGeometryAboutToChange.connect(() => {
                if (root.dragged === w) {
                    root.movedSinceStep = true;
                }
            });
        }
    }

    function moveResizeStarted(w) {
        if (w.move) {
            dragged = w;
            const id = key(w);
            const frame = restoreFrames[id];
            dragStart = { restore: frame, group: groups[id] };
            // a snapped window dragged away gets its size back, under the pointer
            if (frame !== undefined && stateOf(w) !== "none") {
                const r = Snap.restoreUnderPointer(plain(w.frameGeometry), frame, Workspace.cursorPos);
                forget(w);
                w.frameGeometry = Qt.rect(r.x, r.y, r.width, r.height);
            } else {
                forget(w);
            }
            movedSinceStep = false;
        } else if (w.resize) {
            resizeFrom = plain(w.frameGeometry);
        }
    }

    function moveResizeStepped(w) {
        if (w.move && dragged === w) {
            movedSinceStep = false;
            trackBar();
        } else if (w.resize && resizeFrom && resizePartnerOn) {
            followPartner(w);
        }
    }

    // Followed through the pointer, not only the window: near the top KWin
    // stops moving the window, so it sends no more steps.
    function trackBar() {
        if (!snapBarOn || !dragged || !dragged.move) {
            return;
        }
        const cursor = Workspace.cursorPos;
        const screen = Workspace.screenAt(cursor);
        if (!bar.shown && screen && Snap.inBarTrigger(cursor, plain(screen.geometry))) {
            bar.showOn(screen);
        }
        if (bar.shown) {
            bar.track(cursor);
        }
    }

    function moveResizeFinished(w) {
        if (dragged === w) {
            const cancelled = movedSinceStep;
            const target = bar.shown && !cancelled ? bar.target : null;
            const output = bar.output;
            dragged = null;
            if (cancelled) {
                // KWin put the window back where it was: so are its snap and group
                const id = key(w);
                if (dragStart && dragStart.restore !== undefined) {
                    const frames = restoreFrames;
                    frames[id] = dragStart.restore;
                    restoreFrames = frames;
                }
                if (dragStart && dragStart.group) {
                    const g = groups;
                    g[id] = dragStart.group;
                    groups = g;
                }
            }
            dragStart = null;
            if (target) {
                const layout = bar.layouts[target.layout];
                bar.close();
                // the picture stays until the window is there
                preview.settle(w, Snap.zoneRect(workArea(output), layout.zones[target.zone]));
                // after KWin is done with its own end of the drag
                Qt.callLater(() => root.placeInLayout(w, layout, target.zone, output));
            } else {
                bar.hide(!cancelled);
            }
        }
        resizeFrom = null;
    }

    // KWin maximises a window dropped this close to the top, and shows its
    // own preview for it (Outline.qml): the bar steps aside meanwhile.
    function atMaximizeEdge(cursor, output) {
        return Options.electricBorderMaximize && dragged !== null && dragged.maximizable !== false
            && cursor.y <= workArea(output).y + 5;
    }

    function followPartner(w) {
        const mine = groups[key(w)];
        if (!mine) {
            return;
        }
        const after = plain(w.frameGeometry);
        for (const id in groups) {
            const other = groups[id];
            if (other.group !== mine.group || other.window === w || other.window.output !== w.output) {
                continue;
            }
            const r = Snap.partnerFrame(resizeFrom, after, plain(other.window.frameGeometry));
            if (r) {
                other.window.frameGeometry = Qt.rect(r.x, r.y, r.width, r.height);
            }
        }
        resizeFrom = after;
    }

    Connections {
        target: Workspace
        function onWindowAdded(w) {
            root.watch(w);
        }
        function onWindowRemoved(w) {
            root.forget(w);
            const list = root.watched;
            delete list[root.key(w)];
            root.watched = list;
            if (assist.window === w) {
                assist.stop();
            }
        }
    }

    Connections {
        target: Workspace
        enabled: root.dragged !== null && root.snapBarOn
        function onCursorPosChanged() {
            root.trackBar();
        }
    }

    // starts the relay of the decoration's org.raye.Snap call (D-Bus activation)
    DBusCall {
        id: wakeRelay
        service: "org.raye.Snap"
        path: "/Snap"
        dbusInterface: "org.freedesktop.DBus.Peer"
        method: "Ping"
    }

    Connections {
        target: Options
        function onConfigChanged() {
            root.readSettings();
        }
    }

    Component.onCompleted: {
        readSettings();
        for (const w of Workspace.windows) {
            watch(w);
        }
        wakeRelay.call();
    }

    // --- the layouts flyout -------------------------------------------------------------

    PlasmaCore.Dialog {
        id: flyout
        objectName: "snapFlyout"
        property var window: null
        location: PlasmaCore.Types.Floating
        flags: Qt.Popup | Qt.X11BypassWindowManagerHint
        hideOnWindowDeactivate: true
        visible: false

        function open(w, anchor, numbered) {
            window = w;
            const screen = plain(w.output.geometry);
            layouts.layouts = Snap.layoutsFor(screen.width);
            layouts.numbered = numbered;
            layouts.pickedLayout = -1;
            const p = Snap.flyoutPosition(anchor, { width: layouts.width, height: layouts.height }, screen);
            x = p.x;
            y = p.y;
            visible = true;
            requestActivate();
            layouts.forceActiveFocus();
        }

        mainItem: Layouts {
            id: layouts
            objectName: "flyoutLayouts"
            focus: true
            onPicked: (layout, zone) => {
                const w = flyout.window;
                flyout.visible = false;
                if (w) {
                    Workspace.activeWindow = w;
                    root.placeInLayout(w, layouts.layouts[layout], zone, w.output);
                }
            }
            onCancelled: flyout.visible = false
        }

        onSceneGraphError: () => {
            // nothing: without this a graphics reset may end KWin
        }
    }

    // --- the snap bar ---------------------------------------------------------------------

    PlasmaCore.Dialog {
        id: bar
        objectName: "snapBar"
        property bool shown: false
        // up out of the way, for KWin's maximise at the top edge
        property bool tucked: false
        property bool placed: false
        property var output: null
        property var target: null
        readonly property alias layouts: barLayouts.layouts
        // where it rests, whatever its slide is doing
        readonly property int restY: output ? root.plain(output.geometry).y + 8 : 0
        location: PlasmaCore.Types.Floating
        flags: Qt.X11BypassWindowManagerHint | Qt.WindowDoesNotAcceptFocus
        outputOnly: true
        visible: false

        function showOn(screen) {
            output = screen;
            const g = root.plain(screen.geometry);
            barLayouts.layouts = Snap.layoutsFor(g.width);
            barLayouts.columns = barLayouts.layouts.length;
            x = g.x + Math.round((g.width - barLayouts.width) / 2);
            y = hiddenY();
            shown = true;
            tucked = false;
            placed = false;
            // first: a window shown later stays above it, and the bar must
            preview.cover(screen);
            visible = true;
        }
        // above the screen: it slides down whole, frame and layouts together
        function hiddenY() {
            return root.plain(output.geometry).y - Math.max(height, barLayouts.height + 40);
        }
        function slideTo(tuck) {
            tucked = tuck;
            placed = true;
            barSlide.to = tuck ? hiddenY() : restY;
            barSlide.restart();
        }
        function track(cursor) {
            const g = root.plain(output.geometry);
            // the pointer went back down: the bar goes away
            if (cursor.y > restY + barLayouts.height + 120 || cursor.x < g.x || cursor.x > g.x + g.width) {
                hide(false);
                return;
            }
            // back out only a little below the edge, so it does not flap
            const tuck = root.atMaximizeEdge(cursor, output) || (placed && tucked && cursor.y < restY + 4);
            if (!placed || tuck !== tucked) {
                slideTo(tuck);
            }
            target = tucked ? null : barLayouts.nearestZoneAt(cursor.x - x, cursor.y - restY);
            barLayouts.litLayout = target ? target.layout : -1;
            barLayouts.litZone = target ? target.zone : -1;
            preview.land(target ? Snap.zoneRect(root.workArea(output), barLayouts.layouts[target.layout].zones[target.zone]) : null);
        }
        // the bar alone; the preview may stay a moment longer
        function close() {
            barSlide.stop();
            shown = false;
            visible = false;
            target = null;
            barLayouts.litLayout = -1;
        }
        // dropped: the window goes where the preview is, so it just goes;
        // otherwise the preview leaves (a plain sheet fades first)
        function hide(dropped) {
            close();
            preview.hide(dropped);
        }

        mainItem: Layouts {
            id: barLayouts
            objectName: "barLayouts"
        }

        onSceneGraphError: () => {}
    }

    // the bar slides down from the top edge, and up when it steps aside
    NumberAnimation {
        id: barSlide
        target: bar
        property: "y"
        duration: Tokens.durationFast
        easing.type: Easing.OutCubic
    }

    // --- where the window will land -----------------------------------------------------

    // Covers the screen's work area for the length of the drag and lets the
    // pointer through. Only the sheet inside it moves, so going from one zone
    // to the next is one shape travelling, not two windows.
    Window {
        id: preview
        objectName: "snapPreview"
        // the script's root item has no window: without this Qt waits for one
        // and the preview never shows
        transientParent: null
        flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput
        color: "transparent"
        visible: false

        property var area: null
        property rect landing
        readonly property bool hasZone: landingSheet.shown
        // the window it shows, kept until the preview is gone (the drag may
        // be over by then)
        property var window: null
        // dropped: the window on its way to this frame
        property var settling: null
        property rect settleFrame

        function cover(output) {
            closing = false;
            hideSoon.stop();
            settleTimer.stop();
            settling = null;
            window = root.dragged;
            area = root.workArea(output);
            x = area.x;
            y = area.y;
            width = area.width;
            height = area.height;
            landingSheet.land(null);
            visible = true;
        }

        // rect: the place, or null
        function land(rect) {
            if (!rect) {
                landingSheet.land(null);
                return;
            }
            landing = Qt.rect(rect.x, rect.y, rect.width, rect.height);
            landingSheet.land(Qt.rect(rect.x - area.x, rect.y - area.y, rect.width, rect.height));
        }

        property bool closing: false

        function hide(dropped) {
            settleTimer.stop();
            settling = null;
            if (dropped) {
                landingSheet.clear();
            } else {
                landingSheet.land(null);
            }
            closing = !dropped && landingSheet.busy;
            if (!closing) {
                hideSoon.restart();
            }
        }

        // A moment empty before it goes: KWin fades a window out from its
        // last picture, which would be the window's picture fading on its own.
        Timer {
            id: hideSoon
            interval: 80
            onTriggered: preview.visible = false
        }

        // Stays on the place until the window has its frame there, so the
        // window never shows where it was dropped in between.
        function settle(w, frame) {
            settleFrame = Qt.rect(frame.x, frame.y, frame.width, frame.height);
            settling = w;
            settleTimer.restart();
        }

        onVisibleChanged: {
            if (!visible) {
                window = null;
            }
        }

        Connections {
            target: preview.settling
            function onFrameGeometryChanged() {
                const f = root.plain(preview.settling.frameGeometry);
                const r = preview.settleFrame;
                if (f.x === r.x && f.y === r.y && f.width === r.width && f.height === r.height) {
                    preview.hide(true);
                }
            }
        }

        // a window that will not take the frame (a minimum size) gets it anyway
        Timer {
            id: settleTimer
            interval: 400
            onTriggered: preview.hide(true)
        }

        Landing {
            id: landingSheet
            window: preview.window
            onBusyChanged: {
                if (!busy && preview.closing) {
                    preview.closing = false;
                    hideSoon.restart();
                }
            }
            // the window's own size, which the content item only takes once
            // the window has been resized for real
            width: preview.width
            height: preview.height
            hole: preview.window && preview.area
                ? Qt.rect(preview.window.frameGeometry.x - preview.area.x, preview.window.frameGeometry.y - preview.area.y,
                          preview.window.frameGeometry.width, preview.window.frameGeometry.height)
                : Qt.rect(0, 0, 0, 0)
        }
    }

    // --- snap assist ------------------------------------------------------------------------

    PlasmaCore.Dialog {
        id: assist
        objectName: "snapAssist"
        property var window: null
        property var layout: null
        property var filled: []
        property var area: null
        property var output: null
        property int group: 0
        property var zone: null
        property var candidates: []
        location: PlasmaCore.Types.Floating
        flags: Qt.Popup | Qt.X11BypassWindowManagerHint
        hideOnWindowDeactivate: true
        visible: false

        function start(w, l, filledZones, a, o, g) {
            window = w;
            layout = l;
            filled = filledZones;
            area = a;
            output = o;
            group = g;
            next();
        }

        // the next free zone, with the windows that could fill it
        function next() {
            const free = Snap.freeZones(layout, filled);
            const list = [];
            for (const c of Workspace.stackingOrder.slice().reverse()) {
                const joined = root.groups[root.key(c)];
                if (c === window || !c.normalWindow || !c.moveable || !c.resizeable || (joined && joined.group === group)
                        || !(c.onAllDesktops || c.desktops.indexOf(Workspace.currentDesktop) >= 0)) {
                    continue;
                }
                list.push(c);
            }
            if (free.length === 0 || list.length === 0) {
                stop();
                return;
            }
            candidates = list;
            zone = { index: free[0], rect: Snap.zoneRect(area, layout.zones[free[0]]) };
            x = zone.rect.x;
            y = zone.rect.y;
            // the dialog's frame is outside its item: the whole window covers the zone
            cards.width = zone.rect.width - margins.left - margins.right;
            cards.height = zone.rect.height - margins.top - margins.bottom;
            visible = true;
            requestActivate();
            cards.forceActiveFocus();
            cardsIn.restart();
        }

        function fill(c) {
            filling = true;
            visible = false;
            filling = false;
            const g = root.groups;
            g[root.key(c)] = { group: group, window: c };
            root.groups = g;
            if (c.minimized) {
                c.minimized = false;
            }
            root.place(c, zone.rect);
            filled = filled.concat([zone.index]);
            Workspace.activeWindow = c;
            next();
        }

        function stop() {
            visible = false;
            window = null;
            candidates = [];
        }

        // closed by a click elsewhere or Escape: snap assist ends
        property bool filling: false
        onVisibleChanged: if (!visible && window && !filling) stop()

        mainItem: FocusScope {
            id: cards
            focus: true
            Keys.onEscapePressed: assist.stop()

            // the cards sit in the middle of the zone, as many to a row as fit
            Flow {
                id: cardFlow
                objectName: "snapAssistCards"
                readonly property int cardWidth: Math.min(240, Math.max(160, (cards.width - 32 - 12) / 2))
                readonly property int perRow: Math.max(1, Math.floor((cards.width - 32 + spacing) / (cardWidth + spacing)))
                anchors.centerIn: parent
                width: Math.min(assist.candidates.length, perRow) * (cardWidth + spacing) - spacing
                spacing: 12

                Repeater {
                    model: assist.candidates
                    delegate: Item {
                        id: card
                        required property var modelData
                        width: cardFlow.cardWidth
                        height: width * 0.62 + 28

                        Rectangle {
                            anchors.fill: parent
                            radius: Tokens.radiusOverlay
                            color: hover.containsMouse ? Tokens.fillControlHover : "transparent"
                            border.width: hover.containsMouse ? 2 : 0
                            border.color: Tokens.focusOuter
                        }
                        Text {
                            id: title
                            x: 8
                            y: 6
                            width: parent.width - 16
                            text: card.modelData.caption
                            font: Tokens.caption
                            color: Tokens.textPrimary
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }
                        WindowThumbnail {
                            x: 8
                            y: 28
                            width: parent.width - 16
                            height: parent.height - 36
                            wId: card.modelData.internalId
                        }
                        MouseArea {
                            id: hover
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: assist.fill(card.modelData)
                        }
                    }
                }

                NumberAnimation on opacity {
                    id: cardsIn
                    running: false
                    from: 0
                    to: 1
                    duration: Tokens.durationFast
                    easing.type: Easing.OutCubic
                }
            }
        }

        onSceneGraphError: () => {}
    }
}
