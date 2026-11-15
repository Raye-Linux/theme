// The sign-in column: account picture, name, password field, and the
// lines under it. It fills its parent; the picture's centre sits at 40 %
// of the height.
//
// In (properties):
//   userName          name shown under the picture
//   userPicture       picture url; empty = the Raye default face
//   needsPassword     false: a "Sign in" button instead of the field
//   capsLock          shows "Caps Lock is on" under the field
//   busy              a sign-in is running: the field takes no input
//   methodHint        a line above the field ("Scan your finger...")
//   methodHintIcon    its icon name, optional
//   message           an information line from the backend, under the field
//   showSwitchUser    the "Switch user" link (lock screen)
//   showSignInOptions the "Sign-in options" link (no other method yet)
//   bottomInset       space taken at the bottom (the on-screen keyboard):
//                     the column moves up so the field stays above it
// Out (signals):
//   submitted(password)     Enter, the arrow, or "Sign in"
//   switchUserRequested()   signInOptionsRequested()
//   backRequested()         Esc: back to the lock view
// Do:
//   fail(text)     wrong password: shake, clear, show the message (default
//                  "The password is incorrect. Try again.") and an OK link
//   reset()        clear the field and the message
//   focusField()   typeText(t)  (the first key typed on the lock view)
//
// The password is never stored here; the field is cleared on fail().
import QtQuick
import QtQuick.Templates as T

FocusScope {
    id: column

    property string userName: ""
    property url userPicture: ""
    property bool needsPassword: true
    property bool capsLock: false
    property bool busy: false
    property string methodHint: ""
    property string methodHintIcon: ""
    property string message: ""
    property bool showSwitchUser: false
    property bool showSignInOptions: false
    property real bottomInset: 0

    property string errorText: ""
    readonly property bool failed: errorText !== ""
    readonly property alias passwordField: field

    signal submitted(string password)
    signal switchUserRequested()
    signal signInOptionsRequested()
    signal backRequested()

    function fail(text) {
        errorText = text !== undefined && text !== "" ? text : Strings.tr.wrongPassword
        field.clear()
        field.shake()
        focusField()
    }

    function reset() {
        errorText = ""
        field.clear()
    }

    function focusField() {
        if (needsPassword)
            field.focusField()
        else
            signInButton.forceActiveFocus()
    }

    function typeText(t) {
        if (needsPassword && t !== "")
            field.insert(t)
        else
            focusField()
    }

    Keys.onEscapePressed: backRequested()

    Column {
        id: stack
        anchors.horizontalCenter: parent.horizontalCenter
        y: {
            var centred = column.height * 0.40 - picture.height / 2
            if (column.bottomInset <= 0)
                return Math.round(centred)
            var above = column.height - column.bottomInset - 24 - (field.y + field.height + 40)
            return Math.round(Math.max(24 - field.y, Math.min(centred, above)))
        }
        width: Math.min(column.width - 48, 560)

        Behavior on y {
            NumberAnimation { duration: Motion.normal; easing.type: Motion.enter }
        }

        UserPicture {
            id: picture
            objectName: "picture"
            anchors.horizontalCenter: parent.horizontalCenter
            size: 192
            source: column.userPicture
        }

        Item { width: 1; height: 16 }

        ShadowText {
            id: name
            objectName: "name"
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, stack.width)
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: column.userName
            font.family: RayePalette.displayFamily
            font.weight: Font.DemiBold
            font.pixelSize: Math.round(28 * RayePalette.textScale)
            Accessible.role: Accessible.Heading
        }

        Item { width: 1; height: 20 }

        MethodHint {
            id: hint
            objectName: "hint"
            anchors.horizontalCenter: parent.horizontalCenter
            text: column.methodHint
            iconName: column.methodHintIcon
        }

        Item { width: 1; height: hint.visible ? 12 : 0 }

        PasswordField {
            id: field
            anchors.horizontalCenter: parent.horizontalCenter
            visible: column.needsPassword
            width: Math.round(300 * RayePalette.textScale)
            height: Math.round(36 * RayePalette.textScale)
            busy: column.busy
            focus: column.needsPassword
            onAccepted: password => {
                column.errorText = ""
                column.submitted(password)
            }
            onTextChanged: if (text !== "") column.errorText = ""
        }

        T.AbstractButton {
            id: signInButton
            objectName: "signInButton"
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !column.needsPassword
            enabled: !column.busy
            focus: !column.needsPassword
            focusPolicy: Qt.StrongFocus
            hoverEnabled: true
            text: Strings.tr.signIn
            implicitWidth: Math.max(160, signInLabel.implicitWidth + 48)
            implicitHeight: Math.round(36 * RayePalette.textScale)
            Accessible.role: Accessible.Button
            Accessible.name: text
            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
            onClicked: column.submitted("")

            contentItem: ShadowText {
                id: signInLabel
                shadow: false
                text: signInButton.text
                color: RayePalette.textOnAccent
                font.pixelSize: Math.round(14 * RayePalette.textScale)
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: RayePalette.fieldRadius
                color: signInButton.pressed ? RayePalette.accentPressed
                     : signInButton.hovered ? RayePalette.accentHover : RayePalette.accent
                border.width: signInButton.visualFocus ? RayePalette.focusWidth : 0
                border.color: RayePalette.focus
            }
        }

        Item { width: 1; height: 13 }

        // caps lock, the error, or the backend's message: one at a time
        ShadowText {
            id: capsLine
            objectName: "capsLine"
            anchors.horizontalCenter: parent.horizontalCenter
            visible: column.capsLock && column.needsPassword && !column.failed
            text: Strings.tr.capsLock
            font.pixelSize: Math.round(13 * RayePalette.textScale)
            color: RayePalette.text
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        ShadowText {
            id: errorLine
            objectName: "errorLine"
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, Math.round(320 * RayePalette.textScale))
            visible: column.failed
            text: column.errorText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Math.round(14 * RayePalette.textScale)
            color: RayePalette.highContrast ? "#ffffff" : RayePalette.error
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Item { width: 1; height: okLink.visible ? 4 : 0 }

        TextLink {
            id: okLink
            objectName: "okLink"
            anchors.horizontalCenter: parent.horizontalCenter
            visible: column.failed
            text: Strings.tr.ok
            onClicked: {
                column.errorText = ""
                column.focusField()
            }
        }

        ShadowText {
            id: messageLine
            objectName: "messageLine"
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, Math.round(320 * RayePalette.textScale))
            visible: column.message !== "" && !column.failed && !capsLine.visible
            text: column.message
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Math.round(14 * RayePalette.textScale)
            color: RayePalette.textSecondary
        }

        Item { width: 1; height: optionsLink.visible || switchLink.visible ? 12 : 0 }

        TextLink {
            id: optionsLink
            objectName: "optionsLink"
            anchors.horizontalCenter: parent.horizontalCenter
            visible: column.showSignInOptions
            text: Strings.tr.signInOptions
            onClicked: column.signInOptionsRequested()
        }

        TextLink {
            id: switchLink
            objectName: "switchLink"
            anchors.horizontalCenter: parent.horizontalCenter
            visible: column.showSwitchUser
            text: Strings.tr.switchUser
            onClicked: column.switchUserRequested()
        }
    }
}
