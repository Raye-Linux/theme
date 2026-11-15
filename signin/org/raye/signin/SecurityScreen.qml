// The security screen: what Ctrl+Alt+Del shows while someone is signed
// in. The six options Windows has -- Lock, Switch user, Sign out, Change
// a password, Task Manager, Cancel -- over the blurred picture, with the
// account named at the top and the corner bar (network, accessibility,
// power) where every other Raye sign-in surface keeps it.
//
// THIS FILE DECIDES NOTHING. It draws the options and says which one was
// chosen; what each one DOES belongs to whatever shows the screen, which
// is the only thing holding a session manager. Each option is gated by a
// capability its host sets, and a capability that is false shows the
// reason next to the greyed line instead of leaving someone to wonder.
//
// "Change a password" touches authentication, and this file never takes
// a password: the host opens the Settings page for it, and has to say it
// can (`canChangePassword`, false by default). A refused line says why, and
// cannot act -- not by mouse, not by Enter, and the keyboard walks past it.
// "Task Manager" is gated the same way: it is a separate application.
//
// The host is lookandfeel/.../contents/logout/Logout.qml. Ctrl+Alt+Del is
// the session's own shortcut, and it does not reach this screen while the
// session is locked.
//
// In:  userName userPicture
//      canLock canSwitchUser canSignOut canChangePassword canTaskManager
//      changePasswordReason taskManagerReason  (shown when refused)
//      networkIcon networkText layoutCode showLayout
//      canSleep canHibernate canShutDown canRestart updatePending
//      keyboardOn keyboardAvailable
// Out: lockRequested() switchUserRequested() signOutRequested()
//      changePasswordRequested() taskManagerRequested() cancelled()
//      plus every signal of the corner bar, passed straight on
import QtQuick
import QtQuick.Templates as T

FocusScope {
    id: screen

    property string userName: ""
    property url userPicture: ""

    property bool canLock: true
    property bool canSwitchUser: true
    property bool canSignOut: true
    // see the header: never true unless a host has a safe path
    property bool canChangePassword: false
    property bool canTaskManager: false

    property string lockReason: ""
    property string switchUserReason: ""
    property string signOutReason: ""
    property string changePasswordReason: Strings.tr.changePasswordElsewhere
    property string taskManagerReason: Strings.tr.taskManagerMissing

    // passed to the corner bar
    property string networkIcon: ""
    property string networkText: Strings.tr.networkSignIn
    property string layoutCode: ""
    property bool showLayout: false
    property bool canSleep: true
    property bool canHibernate: false
    property bool canShutDown: true
    property bool canRestart: true
    property bool updatePending: false
    property bool keyboardOn: false
    property bool keyboardAvailable: true

    property bool shown: true
    // which line the keyboard is on
    property int current: 0
    // the focus ring waits for a key: the mouse has its own highlight
    property bool keyboardUsed: false

    readonly property alias cornerBar: bar

    signal lockRequested()
    signal switchUserRequested()
    signal signOutRequested()
    signal changePasswordRequested()
    signal taskManagerRequested()
    signal cancelled()
    signal sleepRequested()
    signal hibernateRequested()
    signal shutDownRequested()
    signal restartRequested()
    signal updateAndShutDownRequested()
    signal updateAndRestartRequested()
    signal emergencyRestartRequested()
    signal keyboardToggled(bool on)
    signal layoutClicked()

    // The six, in the order Windows has them. `enabled` false carries a
    // reason, so the line can say why instead of only looking dead.
    // Cancel is never refused: it is the way out, and a screen you
    // cannot leave is a trap.
    readonly property var rows: [
        { id: "lock", text: Strings.tr.lock, enabled: canLock, reason: lockReason },
        { id: "switchUser", text: Strings.tr.switchUser, enabled: canSwitchUser, reason: switchUserReason },
        { id: "signOut", text: Strings.tr.signOut, enabled: canSignOut, reason: signOutReason },
        { id: "changePassword", text: Strings.tr.changePassword, enabled: canChangePassword, reason: changePasswordReason },
        { id: "taskManager", text: Strings.tr.taskManager, enabled: canTaskManager, reason: taskManagerReason },
        { id: "cancel", text: Strings.tr.cancel, enabled: true, reason: "" }
    ]

    function indexOf(id) {
        for (var i = 0; i < rows.length; ++i)
            if (rows[i].id === id)
                return i
        return -1
    }

    function iconFor(id) {
        switch (id) {
        case "lock": return "lock"
        case "switchUser": return "switch-user"
        case "signOut": return "sign-out"
        case "changePassword": return "key"
        case "taskManager": return "task-manager"
        }
        return ""
    }

    function firstEnabled(from, step) {
        for (var n = 0; n < rows.length; ++n) {
            var i = ((from + step * n) % rows.length + rows.length) % rows.length
            if (rows[i].enabled)
                return i
        }
        return -1
    }

    function move(step) {
        keyboardUsed = true
        var i = firstEnabled(current + step, step)
        if (i >= 0)
            current = i
    }

    // Takes an id or an index. A refused line does nothing here: this
    // is the one gate both the mouse and the keyboard go through, so
    // there is a single place it can be got wrong.
    function pick(which) {
        var i = typeof which === "string" ? indexOf(which) : which
        if (i < 0 || i >= rows.length || !rows[i].enabled)
            return
        current = i
        switch (rows[i].id) {
        case "lock": lockRequested(); break
        case "switchUser": switchUserRequested(); break
        case "signOut": signOutRequested(); break
        case "changePassword": changePasswordRequested(); break
        case "taskManager": taskManagerRequested(); break
        case "cancel": cancelled(); break
        }
    }

    focus: true
    Keys.onEscapePressed: screen.cancelled()
    Keys.onUpPressed: screen.move(-1)
    Keys.onDownPressed: screen.move(1)
    Keys.onTabPressed: screen.move(1)
    Keys.onBacktabPressed: screen.move(-1)
    Keys.onReturnPressed: screen.pick(screen.current)
    Keys.onEnterPressed: screen.pick(screen.current)
    Keys.onSpacePressed: screen.pick(screen.current)

    Component.onCompleted: current = Math.max(0, firstEnabled(0, 1))

    Backdrop {
        anchors.fill: parent
        blurred: true
    }

    // the whole plate comes up together, and stands still when the
    // session asks for no animation (Motion.reducedMotion)
    Item {
        id: plate
        anchors.fill: parent
        opacity: screen.shown ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: Motion.normal; easing.type: Motion.enter }
        }
        transform: Translate {
            y: screen.shown ? 0 : 12
            Behavior on y {
                NumberAnimation { duration: Motion.normal; easing.type: Motion.enter }
            }
        }

        // the glass plate; its soft edge is drawn, since the scene graph of
        // a machine without a GPU has no shadow effect to offer
        Repeater {
            model: RayePalette.highContrast ? 0 : 3
            delegate: Rectangle {
                required property int index
                anchors.centerIn: plateFill
                width: plateFill.width + (index + 1) * 16
                height: plateFill.height + (index + 1) * 16
                radius: plateFill.radius + (index + 1) * 8
                color: Qt.rgba(0.02, 0.01, 0.05, 0.10)
            }
        }

        Rectangle {
            id: plateFill
            objectName: "plate"
            anchors.centerIn: parent
            width: card.width + 2 * padding
            height: card.implicitHeight + 2 * padding
            readonly property int padding: Math.round(20 * RayePalette.textScale)
            radius: 16
            color: RayePalette.highContrast ? "#000000" : Qt.rgba(0x1a / 255, 0x10 / 255, 0x28 / 255, 0.72)
            border.width: 1
            border.color: RayePalette.highContrast ? "#ffffff" : RayePalette.flyoutBorder
        }

        Column {
            id: card
            objectName: "card"
            anchors.centerIn: parent
            width: Math.min(parent.width - 2 * RayePalette.edge - 40,
                            Math.round(380 * RayePalette.textScale))
            spacing: 4

            UserPicture {
                objectName: "picture"
                anchors.horizontalCenter: parent.horizontalCenter
                source: screen.userPicture
                size: Math.round(72 * RayePalette.textScale)
            }

            Item {
                width: 1
                height: 4
            }

            ShadowText {
                objectName: "userName"
                visible: screen.userName !== ""
                text: screen.userName
                font.family: RayePalette.displayFamily
                font.pixelSize: Math.round(20 * RayePalette.textScale)
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                width: card.width
                elide: Text.ElideRight
            }

            Item {
                width: 1
                height: 8
            }

            Repeater {
                model: screen.rows
                delegate: Item {
                    id: slot

                    required property var modelData
                    required property int index
                    readonly property bool isCancel: modelData.id === "cancel"

                    width: card.width
                    height: line.height + (isCancel ? rule.height + 16 : 0)

                    // the lines come up one after the other, quickly
                    opacity: Motion.normal === 0 ? 1 : 0
                    SequentialAnimation {
                        running: Motion.normal > 0
                        PauseAnimation { duration: 40 + slot.index * 30 }
                        NumberAnimation { target: slot; property: "opacity"; to: 1; duration: Motion.normal; easing.type: Motion.enter }
                    }

                    Rectangle {
                        id: rule
                        visible: slot.isCancel
                        y: 8
                        width: parent.width
                        height: 1
                        color: RayePalette.ring
                    }

                    T.AbstractButton {
                        id: line

                        readonly property var modelData: slot.modelData
                        readonly property bool refused: !modelData.enabled
                        readonly property bool current: screen.current === slot.index

                        objectName: "row-" + modelData.id
                        anchors.bottom: parent.bottom
                        width: card.width
                        height: Math.max(Math.round(44 * RayePalette.textScale),
                                         words.implicitHeight + Math.round(16 * RayePalette.textScale))
                        enabled: modelData.enabled
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus

                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.text
                        Accessible.description: line.refused ? modelData.reason : ""

                        onClicked: screen.pick(slot.index)
                        onHoveredChanged: if (hovered) screen.current = slot.index

                        background: Rectangle {
                            radius: RayePalette.controlRadius + 2
                            color: line.pressed ? RayePalette.pressFill
                                 : line.current && !line.refused ? RayePalette.hoverFill
                                 : "transparent"
                            border.width: line.current && !line.refused && screen.activeFocus
                                          && screen.keyboardUsed
                                          ? RayePalette.focusWidth : 0
                            border.color: RayePalette.focus
                            Behavior on color {
                                ColorAnimation { duration: Motion.fast }
                            }

                            // Raye's selection pill
                            Rectangle {
                                anchors.left: parent.left
                                anchors.leftMargin: 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: 3
                                height: parent.height * (line.pressed ? 0.3 : 0.45)
                                radius: 1.5
                                color: RayePalette.accent
                                visible: line.current && !line.refused && !slot.isCancel
                                         && !RayePalette.highContrast
                            }
                        }

                        contentItem: Item {
                            Image {
                                id: lineIcon
                                visible: !slot.isCancel
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                source: visible ? RayePalette.icon(screen.iconFor(line.modelData.id)) : ""
                                sourceSize: Qt.size(20 * Screen.devicePixelRatio, 20 * Screen.devicePixelRatio)
                                width: 20
                                height: 20
                                opacity: line.refused ? 0.45 : 0.95
                                Accessible.ignored: true
                            }

                            Column {
                                id: words
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: slot.isCancel ? parent.left : lineIcon.right
                                anchors.right: parent.right
                                anchors.leftMargin: slot.isCancel ? 0 : 14
                                spacing: 2

                                ShadowText {
                                    id: lineText
                                    objectName: "label"
                                    width: parent.width
                                    text: line.modelData.text
                                    horizontalAlignment: slot.isCancel ? Text.AlignHCenter : Text.AlignLeft
                                    elide: Text.ElideRight
                                    // the line still has to be readable while
                                    // it is refused: it says what it is and why
                                    opacity: line.refused ? 0.55 : 1
                                    font.pixelSize: Math.round(15 * RayePalette.textScale)
                                    font.weight: Font.Medium
                                }
                                ShadowText {
                                    id: why
                                    objectName: "reason-" + line.modelData.id
                                    width: parent.width
                                    rightPadding: 12
                                    visible: line.refused && line.modelData.reason !== ""
                                    text: line.modelData.reason
                                    color: RayePalette.textSecondary
                                    font.pixelSize: Math.round(12 * RayePalette.textScale)
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }

        // the time, quietly, in the corner across from the buttons
        Column {
            objectName: "corner-time"
            visible: !RayePalette.highContrast
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: RayePalette.edge
            anchors.bottomMargin: RayePalette.edge
            spacing: 0

            property date now: new Date()
            Timer {
                interval: 1000
                repeat: true
                running: screen.visible
                onTriggered: parent.now = new Date()
            }

            ShadowText {
                text: Strings.timeText(parent.now, Strings.locale)
                font.family: RayePalette.displayFamily
                font.weight: Font.DemiBold
                font.pixelSize: Math.round(20 * RayePalette.textScale)
                font.features: { "tnum": 1 }
            }
            ShadowText {
                text: {
                    var d = Strings.dateText(parent.now, Strings.locale)
                    return d.charAt(0).toUpperCase() + d.slice(1)
                }
                color: RayePalette.textSecondary
                font.pixelSize: Math.round(13 * RayePalette.textScale)
            }
        }
    }

    CornerBar {
        id: bar
        objectName: "cornerBar"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: RayePalette.edge
        anchors.bottomMargin: RayePalette.edge
        networkIcon: screen.networkIcon
        networkText: screen.networkText
        layoutCode: screen.layoutCode
        showLayout: screen.showLayout
        canSleep: screen.canSleep
        canHibernate: screen.canHibernate
        canShutDown: screen.canShutDown
        canRestart: screen.canRestart
        updatePending: screen.updatePending
        keyboardOn: screen.keyboardOn
        keyboardAvailable: screen.keyboardAvailable
        // the hidden Ctrl+click restart belongs to this screen
        emergencyRestart: true
        onSleepRequested: screen.sleepRequested()
        onHibernateRequested: screen.hibernateRequested()
        onShutDownRequested: screen.shutDownRequested()
        onRestartRequested: screen.restartRequested()
        onUpdateAndShutDownRequested: screen.updateAndShutDownRequested()
        onUpdateAndRestartRequested: screen.updateAndRestartRequested()
        onEmergencyRestartRequested: screen.emergencyRestartRequested()
        onKeyboardToggled: on => screen.keyboardToggled(on)
        onLayoutClicked: screen.layoutClicked()
    }
}
