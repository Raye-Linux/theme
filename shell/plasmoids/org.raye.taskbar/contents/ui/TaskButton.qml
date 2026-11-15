// One button of the taskbar: the app's icon (and its window title when
// apps are not combined) on a 40 x 40 backplate with 4 px corners, the
// pill under the icon (short and grey when the app runs, long and accent
// for the window in use), the badge at the top right, the amber
// backplate of an app asking for attention, the hop of an app starting.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import "taskbarlogic.js" as Logic

Item {
    id: button

    // the row of the tasks model and its roles
    property int index: -1
    property string title: ""
    property var iconSource: ""
    property bool isLauncher: false
    property bool isStartup: false
    property bool isActive: false
    property bool isMinimized: false
    property bool demandsAttention: false
    property int childCount: 0

    property bool small: false
    property bool labelShown: false
    property int count: 0                 // the badge, 0 for none
    property bool flashing: true
    // the keyboard is on this button (Meta+T, then the arrows)
    property bool keyboardCurrent: false

    readonly property string indicator: Logic.indicator({ isLauncher: isLauncher, isStartup: isStartup,
                                                          isActive: isActive, isMinimized: isMinimized })
    readonly property string badge: Logic.badgeText(count)
    readonly property bool attention: demandsAttention && flashing && !isActive
    readonly property int iconSize: small ? ShellTokens.iconSmallSize : ShellTokens.iconSize
    readonly property bool hovered: mouse.containsMouse
    readonly property bool pressed: mouse.pressed && !dragging
    property bool dragging: false

    signal clicked(string button, bool shift, bool ctrl)
    signal contextMenuRequested()
    // a file held over the button for 500 ms
    signal dragHeld()
    signal filesDropped(var urls)
    // the button was dragged sideways by dx px and let go; px is where the
    // pointer is in the row. Both are taken in the row, not in the button,
    // which moves under the pointer.
    signal moveRequested(real dx)
    signal moving(real dx, real px)

    implicitWidth: small ? ShellTokens.buttonSmallSize : ShellTokens.buttonSize
    implicitHeight: small ? ShellTokens.buttonSmallSize : ShellTokens.buttonSize

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: badge !== "" ? i18ndp("raye_shell_taskbar", "%1 new item", "%1 new items", count) : ""
    Accessible.onPressAction: button.clicked("left", false, false)

    // the icon jumps 6 px up and back when the app starts
    function hop() {
        hopAnimation.restart()
    }

    onIsStartupChanged: if (isStartup) hop()

    // --- backplate ---------------------------------------------------------
    Rectangle {
        id: plate
        objectName: "backplate"
        anchors.fill: parent
        radius: ShellTokens.radiusButton
        color: button.pressed ? ShellTokens.taskbarPressFill
             : button.isActive && !button.isMinimized ? ShellTokens.taskbarSelectedFill
             : button.hovered ? ShellTokens.taskbarHoverFill : "transparent"
        Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }
    }

    // amber when the app asks for attention: three pulses, then steady
    Rectangle {
        id: attentionPlate
        objectName: "attention"
        anchors.fill: parent
        radius: ShellTokens.radiusButton
        color: Qt.alpha(ShellTokens.attention, 0.8)
        visible: button.attention
        opacity: 1
        onVisibleChanged: if (visible) pulse.restart(); else pulse.stop()

        SequentialAnimation {
            id: pulse
            loops: 3
            NumberAnimation { target: attentionPlate; property: "opacity"; to: 0.3; duration: ShellTokens.durationSlow; easing.type: Easing.InOutQuad }
            NumberAnimation { target: attentionPlate; property: "opacity"; to: 1; duration: ShellTokens.durationSlow; easing.type: Easing.InOutQuad }
        }
    }

    FocusRing {
        radius: ShellTokens.radiusButton
        shown: button.keyboardCurrent
    }

    // --- icon and title ----------------------------------------------------
    Item {
        id: content
        anchors.fill: parent
        scale: button.pressed ? ShellTokens.pressScale : 1
        Behavior on scale { NumberAnimation { duration: ShellTokens.durationFaster; easing: Motion.pointToPoint } }

        Kirigami.Icon {
            id: icon
            objectName: "icon"
            width: button.iconSize
            height: button.iconSize
            x: button.labelShown ? 8 : Math.round((parent.width - width) / 2)
            y: Math.round((parent.height - height) / 2) + hopOffset
            property real hopOffset: 0
            source: button.iconSource
            active: button.hovered
            opacity: button.isStartup ? 0.75 : 1
            Behavior on opacity { NumberAnimation { duration: ShellTokens.durationFast } }

            SequentialAnimation {
                id: hopAnimation
                NumberAnimation { target: icon; property: "hopOffset"; to: -ShellTokens.launchHop; duration: ShellTokens.durationSlow / 2; easing: Motion.entrance }
                NumberAnimation { target: icon; property: "hopOffset"; to: 0; duration: ShellTokens.durationSlow / 2; easing.type: Easing.OutBounce }
            }
        }

        Text {
            objectName: "label"
            visible: button.labelShown
            anchors { left: icon.right; leftMargin: 8; right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            text: button.title
            elide: Text.ElideRight
            color: ShellTokens.taskbarText
            font: ShellTokens.caption
            textFormat: Text.PlainText
        }

        // the badge: a 16 px accent circle (wider for 99+) on the icon's corner
        Rectangle {
            objectName: "badge"
            visible: button.badge !== ""
            height: ShellTokens.badgeSize
            width: Math.max(height, badgeText.implicitWidth + 8)
            radius: height / 2
            color: ShellTokens.badge
            border.width: 1
            border.color: ShellTokens.taskbarSolid
            x: icon.x + icon.width - width / 2 - 2
            y: icon.y - height / 2 + 2

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: button.badge
                color: ShellTokens.textOnAccent
                font.family: "Inter"
                font.pixelSize: 10
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
            }
        }
    }

    // --- the pill under the icon -------------------------------------------
    Item {
        id: pillHolder
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: button.small ? 1 : 2
        width: pill.width
        height: ShellTokens.pillHeight

        Rectangle {
            id: pill
            objectName: "pill"
            anchors.centerIn: parent
            height: ShellTokens.pillHeight
            radius: height / 2
            width: button.indicator === "focused" ? ShellTokens.pillLong : ShellTokens.pillShort
            color: button.indicator === "focused" ? ShellTokens.accent : ShellTokens.pillRunningOnTaskbar
            opacity: button.indicator === "none" ? 0 : 1

            // loading: the short pill breathes until the window shows (runs
            // only while the app starts)
            SequentialAnimation on width {
                running: button.indicator === "starting" && ShellTokens.durationSlow > 0
                loops: Animation.Infinite
                alwaysRunToEnd: true
                NumberAnimation { to: ShellTokens.pillShort + 6; duration: ShellTokens.durationSlow; easing.type: Easing.InOutQuad }
                NumberAnimation { to: ShellTokens.pillShort; duration: ShellTokens.durationSlow; easing.type: Easing.InOutQuad }
            }
            Behavior on width { NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint } }
            Behavior on color { ColorAnimation { duration: ShellTokens.durationFast } }
            Behavior on opacity { NumberAnimation { duration: ShellTokens.durationFast } }
        }
    }

    // --- input -------------------------------------------------------------
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        // the plain arrow, also while a button is dragged to a new place
        // (the drag is this area's own, never a drag and drop)
        cursorShape: Qt.ArrowCursor
        // in the row: the button follows the pointer, so a position in the
        // button would take its own move away from the next one
        property real pressX: 0
        function rowX(event) {
            return button.parent ? mapToItem(button.parent, event.x, 0).x : event.x
        }

        // the release that ends a drag is not also a click
        property bool dropped: false

        onPressed: event => {
            pressX = rowX(event)
            dropped = false
            button.dragging = false
        }
        onPositionChanged: event => {
            if (!pressed || !(pressedButtons & Qt.LeftButton))
                return
            const px = rowX(event)
            if (!button.dragging && Math.abs(px - pressX) > Qt.styleHints.startDragDistance)
                button.dragging = true
            if (button.dragging)
                button.moving(px - pressX, px)
        }
        onReleased: event => {
            if (button.dragging) {
                // asked first: the row settles the new order before the
                // button stops following the pointer
                button.moveRequested(rowX(event) - pressX)
                dropped = true
                button.dragging = false
            }
        }
        onCanceled: button.dragging = false
        onClicked: event => {
            if (button.dragging || dropped)
                return
            if (event.button === Qt.RightButton) {
                button.contextMenuRequested()
                return
            }
            if (event.button === Qt.LeftButton && button.isLauncher)
                button.hop()
            button.clicked(event.button === Qt.MiddleButton ? "middle" : "left",
                           (event.modifiers & Qt.ShiftModifier) !== 0,
                           (event.modifiers & Qt.ControlModifier) !== 0)
        }
    }

    // a file dragged over the button brings its window up after 500 ms
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onEntered: holdTimer.restart()
        onExited: holdTimer.stop()
        onDropped: drop => {
            holdTimer.stop()
            if (drop.hasUrls)
                button.filesDropped(drop.urls)
        }

        Timer {
            id: holdTimer
            interval: 500
            onTriggered: button.dragHeld()
        }
    }
}
