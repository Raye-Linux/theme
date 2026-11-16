// The row of app buttons. It knows nothing of Plasma: the tasks come
// from `model` (Plasma's tasks model in the applet, a list in the tests)
// and every action goes to `controller`, which gives:
//   counts()            { grouped, ungrouped } buttons, see Logic.fit()
//   revision            changes when counts() would answer differently
//   launch(i) newInstance(i) activate(i) minimize(i) cycle(i)
//   openUrls(i, urls) move(from, to) isPinned(i) togglePin(i) close(i)
// Options come from `settings` (ShellSettings in the applet).
import QtQuick
import org.raye.ui
import org.raye.shell

import "taskbarlogic.js" as Logic

FocusScope {
    id: taskBar

    property var model
    property var controller
    property var settings: ShellSettings
    // unread counts by app key (Logic.appKey), for the badges
    property var badgeCounts: ({})
    // the Start applet sits right before the taskbar and centres the whole
    // group itself: the row then starts at the taskbar's left edge
    property bool groupedWithStart: false

    readonly property var counts: {
        void (controller ? controller.revision : 0)
        return controller ? controller.counts() : ({ grouped: 0, ungrouped: 0 })
    }
    readonly property var layout: Logic.fit(settings.combine, settings.smallButtons,
                                            counts.grouped, counts.ungrouped, width)
    readonly property bool labels: layout.labels
    readonly property bool small: layout.small
    readonly property bool overflowing: layout.overflow
    readonly property int visibleCount: Math.min(layout.visible, buttonCount)
    readonly property int buttonCount: repeater.count
    // the room the row takes, for the Start applet's centring
    readonly property int rowWidth: Math.min(buttonCount, visibleCount) * layout.pitch + (overflowing ? Logic.overflowPitch : 0)
    readonly property int rowX: groupedWithStart ? 0 : Logic.rowStart(settings.alignment, rowWidth, width)
    property int currentIndex: -1
    // a button being dragged to a new place: from where, to where for now
    property int dragFrom: -1
    property int dragTo: -1
    // the dropped button, eased from where it was let go into its place
    property int landIndex: -1
    property real landOffset: 0
    // the frame the new order is put in: nothing slides then
    property bool settling: false
    readonly property bool menuOpen: jumpListLoader.item ? jumpListLoader.item.opened === true : false

    signal escaped()
    // a right-click on the taskbar outside the buttons, at x, y in this item
    signal emptyMenuRequested(real x, real y)

    // Puts the order seen when the button is let go in the model. The
    // others are already in their places; the dropped one eases from the
    // pointer into its own.
    function drop(from, dx) {
        const to = dragTo < 0 ? from : dragTo
        settling = true
        landing.stop()
        landOffset = from * layout.pitch + dx - to * layout.pitch
        landIndex = to
        dragFrom = -1
        dragTo = -1
        const b = buttonAt(from)
        if (b)
            b.dragOffset = 0
        if (to !== from && controller)
            controller.move(from, to)
        landing.restart()
        Qt.callLater(() => settling = false)
    }

    NumberAnimation {
        id: landing
        target: taskBar
        property: "landOffset"
        to: 0
        duration: ShellTokens.durationFast
        easing: Motion.pointToPoint
        onStopped: taskBar.landIndex = -1
    }

    function buttonAt(index) {
        return repeater.itemAt(index)
    }

    function trigger(index, action) {
        if (!controller || index < 0 || index >= buttonCount)
            return
        switch (action) {
        case "launch": controller.launch(index); break
        case "newInstance": controller.newInstance(index); break
        case "activate": controller.activate(index); break
        case "minimize": controller.minimize(index); break
        case "cycle": controller.cycle(index); break
        }
    }

    function click(index, mouseButton, shift, ctrl) {
        const b = buttonAt(index)
        if (!b)
            return
        trigger(index, Logic.clickAction({ isLauncher: b.isLauncher, isStartup: b.isStartup,
                                           isActive: b.isActive, isMinimized: b.isMinimized,
                                           childCount: b.childCount }, mouseButton, shift, ctrl))
    }

    // Meta+1...9 (plasmashell calls the applet, which calls this)
    function activateTaskAtIndex(index) {
        const i = Logic.indexForNumber(Number(index) + 1, buttonCount)
        if (i >= 0)
            click(i, "left", false, false)
    }

    function openOverflow() {
        if (overflowButton.visible)
            overflow.open()
    }

    // Meta+T: the keyboard goes to the first button
    function takeFocus() {
        currentIndex = buttonCount > 0 ? 0 : -1
        forceActiveFocus(Qt.ShortcutFocusReason)
    }

    function openJumpList(index) {
        const b = buttonAt(index)
        if (!b)
            return
        jumpListLoader.active = true
        const menu = jumpListLoader.item
        if (!menu)
            return
        menu.controller = controller
        menu.index = index
        menu.title = b.title
        menu.isLauncher = b.isLauncher
        menu.childCount = b.childCount
        menu.visualParent = b
        menu.open()
    }

    activeFocusOnTab: true
    implicitHeight: ShellTokens.taskbarHeight
    Accessible.role: Accessible.ToolBar
    Accessible.name: i18nd("raye_shell_taskbar", "Running apps")

    Keys.onPressed: event => {
        const last = Math.max(0, visibleCount - 1)
        switch (event.key) {
        case Qt.Key_Left:
            currentIndex = currentIndex <= 0 ? last : currentIndex - 1
            break
        case Qt.Key_Right:
            currentIndex = currentIndex >= last ? 0 : currentIndex + 1
            break
        case Qt.Key_Home:
            currentIndex = 0
            break
        case Qt.Key_End:
            currentIndex = last
            break
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            click(currentIndex, "left", (event.modifiers & Qt.ShiftModifier) !== 0, false)
            break
        case Qt.Key_Menu:
            openJumpList(currentIndex)
            break
        case Qt.Key_F10:
            if (event.modifiers & Qt.ShiftModifier)
                openJumpList(currentIndex)
            else
                return
            break
        case Qt.Key_Escape:
            currentIndex = -1
            taskBar.escaped()
            break
        default:
            return
        }
        event.accepted = true
    }
    onActiveFocusChanged: if (!activeFocus) currentIndex = -1

    // the taskbar's own menu (Task Manager, Taskbar settings) on the free
    // space, never Plasma's
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: mouse => taskBar.emptyMenuRequested(mouse.x, mouse.y)
    }

    // --- the buttons -------------------------------------------------------
    Item {
        id: row
        x: taskBar.rowX
        width: taskBar.rowWidth
        height: parent.height
        Behavior on x { NumberAnimation { duration: ShellTokens.durationNormal; easing: Motion.pointToPoint } }

        Repeater {
            id: repeater
            model: taskBar.model

            delegate: TaskButton {
                id: taskButton
                required property int index
                required property var model

                property real dragOffset: 0
                // where it shows: its place, or one over while another is dragged past it
                readonly property int slot: Logic.shiftedSlot(index, taskBar.dragFrom, taskBar.dragTo)

                // Dragged sideways it follows the pointer exactly, by a shift
                // on top of its place (its x does not move, so nothing eases
                // it); the others slide once to make room as it passes their
                // middle.
                readonly property real shift: dragging ? dragOffset
                                                       : index === taskBar.landIndex ? taskBar.landOffset : 0
                x: slot * taskBar.layout.pitch
                transform: Translate { x: taskButton.shift }
                z: dragging || index === taskBar.landIndex ? 1 : 0
                anchors.verticalCenter: parent.verticalCenter
                width: taskBar.layout.buttonWidth
                height: taskBar.small ? ShellTokens.buttonSmallSize : ShellTokens.buttonSize
                visible: index < taskBar.visibleCount
                Behavior on x {
                    enabled: !taskBar.settling
                    NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint }
                }

                title: model.display || model.AppName || ""
                iconSource: model.decoration || ""
                isLauncher: !!model.IsLauncher
                isStartup: !!model.IsStartup
                isActive: !!model.IsActive
                isMinimized: !!model.IsMinimized
                demandsAttention: !!model.IsDemandingAttention
                childCount: model.ChildCount || 0
                small: taskBar.small
                labelShown: taskBar.labels
                flashing: taskBar.settings.flashing
                count: taskBar.settings.badges ? (taskBar.badgeCounts[Logic.appKey(model.LauncherUrlWithoutIcon, model.AppId)] || 0) : 0
                keyboardCurrent: taskBar.activeFocus && taskBar.currentIndex === index

                onClicked: (mouseButton, shift, ctrl) => {
                    thumbnails.hide()
                    taskBar.click(index, mouseButton, shift, ctrl)
                }
                onContextMenuRequested: {
                    thumbnails.hide()
                    taskBar.openJumpList(index)
                }
                onDragHeld: if (!isLauncher) taskBar.trigger(index, "activate")
                onFilesDropped: urls => { if (taskBar.controller) taskBar.controller.openUrls(index, urls) }
                onMoving: (dx, px) => {
                    dragOffset = Logic.clampedShift(index, dx, taskBar.layout.pitch, taskBar.visibleCount)
                    taskBar.dragFrom = index
                    taskBar.dragTo = Logic.dragSlot(index, taskBar.dragTo, px, taskBar.layout.pitch,
                                                    taskBar.layout.buttonWidth, taskBar.visibleCount)
                }
                onMoveRequested: dx => taskBar.drop(index, dx)
                // the window manager learns where this button is, for the
                // minimize animation into it
                function publishGeometry() {
                    if (taskBar.controller && typeof taskBar.controller.publishGeometry === "function")
                        taskBar.controller.publishGeometry(index, taskButton)
                }
                onXChanged: Qt.callLater(publishGeometry)
                onWidthChanged: Qt.callLater(publishGeometry)
                onVisibleChanged: Qt.callLater(publishGeometry)
                Component.onCompleted: Qt.callLater(publishGeometry)
                // a drag that ends without a drop (the grab was taken away)
                onDraggingChanged: if (!dragging && taskBar.dragFrom === index) {
                    taskBar.dragFrom = -1
                    taskBar.dragTo = -1
                    dragOffset = 0
                }

                onHoveredChanged: {
                    if (hovered)
                        thumbnails.hover(taskButton)
                    else
                        thumbnails.leave(taskButton)
                }

                RayeTooltip {
                    // running apps get the hover preview instead
                    visible: taskButton.hovered && !taskButton.pressed && !taskBar.menuOpen
                             && (!thumbnails.available || taskButton.isLauncher)
                             && !taskBar.labels && taskButton.title !== ""
                    text: taskButton.title
                }
            }
        }

        // "..." when the buttons do not all fit
        PanelButton {
            id: overflowButton
            objectName: "overflowButton"
            readonly property int hiddenCount: taskBar.buttonCount - taskBar.visibleCount
            visible: taskBar.overflowing && hiddenCount > 0
            x: taskBar.visibleCount * taskBar.layout.pitch
            anchors.verticalCenter: parent.verticalCenter
            width: taskBar.small ? ShellTokens.buttonSmallSize : ShellTokens.buttonSize
            height: width
            selected: overflow.opened
            toolTipText: i18nd("raye_shell_taskbar", "Show more apps")
            onClicked: overflow.opened ? overflow.close() : overflow.open()

            Text {
                anchors.centerIn: parent
                text: "…"
                color: ShellTokens.taskbarText
                font.family: "Inter"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
        }
    }

    OverflowFlyout {
        id: overflow
        bar: taskBar
        anchorItem: overflowButton
    }

    // hover thumbnails (and the tooltip they replace)
    Loader {
        id: thumbnailLoader
        // made at the first hover
        active: false
        source: "ThumbnailFlyout.qml"
    }
    QtObject {
        id: thumbnails
        readonly property bool available: thumbnailLoader.status !== Loader.Error
                                          && (!thumbnailLoader.item || thumbnailLoader.item.available === true)
        function hover(b) {
            thumbnailLoader.active = true
            if (available && thumbnailLoader.item) {
                thumbnailLoader.item.controller = taskBar.controller
                thumbnailLoader.item.hover(b.index, b)
            }
        }
        function leave(b) {
            if (thumbnailLoader.item)
                thumbnailLoader.item.leave(b.index, b)
        }
        function hide() {
            if (thumbnailLoader.item)
                thumbnailLoader.item.hide()
        }
    }

    // the right-click menu
    Loader {
        id: jumpListLoader
        active: false
        source: "JumpList.qml"
    }
}
