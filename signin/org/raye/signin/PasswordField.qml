// The password field: 300 x 36, radius 6, a 2 px accent line at the
// bottom when it has focus, an eye inside its right end (shows the
// password; only once something is typed) and the submit arrow joined on
// the right (accent fill on hover).
//
// In:  placeholder, showSubmit, busy (the arrow turns into a spinner and
//      the field stops taking input)
// Out: accepted(password) on Enter or the arrow
// Do:  clear(), shake() (wrong password), focusField(), insert(text)
//
// The password is only ever in the field and in the accepted() call.
import QtQuick
import QtQuick.Controls.Basic as Basic

FocusScope {
    id: root

    property string placeholder: Strings.tr.password
    property bool showSubmit: true
    property bool busy: false
    property bool revealed: false
    property alias text: input.text
    readonly property bool shaking: shakeAnimation.running
    readonly property alias inputItem: input

    signal accepted(string password)

    implicitWidth: 300
    implicitHeight: 36

    function clear() {
        input.clear()
        revealed = false
    }

    function focusField() {
        input.forceActiveFocus()
    }

    // the first key typed on the lock view lands here
    function insert(t) {
        input.insert(input.cursorPosition, t)
        input.forceActiveFocus()
    }

    function shake() {
        if (Motion.reducedMotion)
            return
        shakeAnimation.restart()
    }

    function submit() {
        if (!busy && enabled)
            root.accepted(input.text)
    }

    Item {
        id: body
        width: root.width
        height: root.height
        transform: Translate { id: offset }

        Rectangle {
            id: frame
            anchors.fill: parent
            radius: RayePalette.fieldRadius
            color: RayePalette.fieldFill
            border.width: RayePalette.highContrast && input.activeFocus ? RayePalette.focusWidth : 1
            border.color: RayePalette.highContrast && input.activeFocus ? RayePalette.focus
                                                                     : RayePalette.fieldBorder
        }

        // the accent line when the field has focus
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: RayePalette.fieldRadius / 2
            anchors.rightMargin: RayePalette.fieldRadius / 2
            height: 2
            radius: 1
            color: RayePalette.accent
            opacity: input.activeFocus && !RayePalette.highContrast ? 1 : 0
            Behavior on opacity {
                NumberAnimation { duration: Motion.fast }
            }
        }

        Basic.TextField {
            id: input
            objectName: "input"
            anchors.left: parent.left
            anchors.right: reveal.visible ? reveal.left : buttons.left
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height
            leftPadding: 12
            rightPadding: 4
            topPadding: 0
            bottomPadding: 0
            verticalAlignment: TextInput.AlignVCenter
            background: null
            focus: true
            enabled: !root.busy
            echoMode: root.revealed ? TextInput.Normal : TextInput.Password
            passwordCharacter: "●"
            passwordMaskDelay: 0
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                              | Qt.ImhNoAutoUppercase | Qt.ImhHiddenText
            placeholderText: root.placeholder
            placeholderTextColor: RayePalette.textSecondary
            color: RayePalette.text
            selectionColor: RayePalette.accent
            selectedTextColor: RayePalette.textOnAccent
            font.family: RayePalette.family
            font.pixelSize: Math.round(14 * RayePalette.textScale)
            selectByMouse: true
            Accessible.name: root.placeholder
            Accessible.passwordEdit: !root.revealed

            onAccepted: root.submit()
            onTextChanged: if (text === "") root.revealed = false
        }

        CornerButton {
            id: reveal
            objectName: "reveal"
            anchors.right: buttons.left
            anchors.rightMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            size: 28
            iconSize: 16
            visible: input.text.length > 0 && !root.busy
            iconName: root.revealed ? "eye-hide" : "eye-reveal"
            accessibleName: root.revealed ? Strings.tr.hidePassword : Strings.tr.showPassword
            onClicked: {
                root.revealed = !root.revealed
                input.forceActiveFocus()
            }
        }

        Item {
            id: buttons
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: root.showSubmit ? root.height : 4

            CornerButton {
                id: submitButton
                objectName: "submitButton"
                anchors.fill: parent
                visible: root.showSubmit && !root.busy
                size: root.height
                iconSize: 16
                radius: RayePalette.fieldRadius
                accentHover: true
                iconName: "arrow-right"
                accessibleName: Strings.tr.signIn
                onClicked: root.submit()
            }

            Spinner {
                anchors.centerIn: parent
                size: 18
                visible: root.showSubmit && root.busy
            }
        }
    }

    // 3 times 6 px left and right, then back, in 300 ms
    readonly property int _step: Math.round(Motion.shakeDuration / (2 * Motion.shakeCount + 1))
    readonly property real _d: Motion.shakeDistance

    SequentialAnimation {
        id: shakeAnimation
        onStopped: offset.x = 0

        NumberAnimation { target: offset; property: "x"; to: -root._d; duration: root._step; easing.type: Easing.OutQuad }
        NumberAnimation { target: offset; property: "x"; to: root._d; duration: root._step; easing.type: Easing.InOutQuad }
        NumberAnimation { target: offset; property: "x"; to: -root._d; duration: root._step; easing.type: Easing.InOutQuad }
        NumberAnimation { target: offset; property: "x"; to: root._d; duration: root._step; easing.type: Easing.InOutQuad }
        NumberAnimation { target: offset; property: "x"; to: -root._d; duration: root._step; easing.type: Easing.InOutQuad }
        NumberAnimation { target: offset; property: "x"; to: root._d; duration: root._step; easing.type: Easing.InOutQuad }
        NumberAnimation { target: offset; property: "x"; to: 0; duration: root._step; easing.type: Easing.InQuad }
    }
}
