// The admin prompt, one per screen: the screen dimmed, and on the screen
// with the active window the card that asks for permission.
//
// prompt carries what polkit asked and who asks (see src/prompt.h; the
// tests give a stand-in with the same names).
import QtQuick
import QtQuick.Templates as T
import org.raye.signin

Item {
    id: root

    property var prompt
    property bool showCard: true
    readonly property var request: prompt ? prompt.request : null

    readonly property bool verified: prompt && prompt.publisher !== ""
    readonly property var t: Strings.language === "fr" ? fr : en

    // words only this prompt uses; the rest comes from the sign-in module
    readonly property var en: ({
        action: "Action",
        description: "Description",
        program: "Program location",
        command: "Command",
        account: "Account"
    })
    readonly property var fr: ({
        action: "Action",
        description: "Description",
        program: "Emplacement du programme",
        command: "Commande",
        account: "Compte"
    })

    function yes() {
        if (request && !request.busy)
            request.accept(field.text)
    }

    function no() {
        if (request)
            request.reject()
    }

    Rectangle {
        id: dim
        anchors.fill: parent
        color: "#000000"
        opacity: 0
        Component.onCompleted: opacity = 0.6
        Behavior on opacity {
            NumberAnimation { duration: Motion.reducedMotion ? 0 : 150 }
        }

        // a click beside the card does nothing, as on the secure desktop
        MouseArea {
            anchors.fill: parent
            onPressed: if (root.request) root.request.touch()
        }
    }

    // shadow: two soft layers under the card (no effects in the software
    // scene graph)
    Rectangle {
        visible: card.visible
        x: card.x - 6
        y: card.y + 2
        width: card.width + 12
        height: card.height + 14
        radius: 14
        color: "#33000000"
        opacity: card.opacity
        scale: card.scale
    }
    Rectangle {
        visible: card.visible
        x: card.x - 2
        y: card.y + 2
        width: card.width + 4
        height: card.height + 6
        radius: 10
        color: "#40000000"
        opacity: card.opacity
        scale: card.scale
    }

    FocusScope {
        id: card
        objectName: "card"
        visible: root.showCard && root.prompt
        focus: true
        width: 456
        height: body.implicitHeight + footer.height
        anchors.centerIn: parent
        opacity: 0
        scale: 0.96
        Component.onCompleted: {
            opacity = 1
            scale = 1
        }
        Behavior on opacity {
            NumberAnimation { duration: Motion.normal; easing.type: Motion.enter }
        }
        Behavior on scale {
            NumberAnimation { duration: Motion.normal; easing.type: Motion.enter }
        }

        Accessible.role: Accessible.Dialog
        Accessible.name: Strings.tr.rootAccess
        Accessible.description: Strings.tr.allowChanges + " " + (root.prompt ? root.prompt.appName : "")

        Keys.onPressed: function (event) {
            if (root.request)
                root.request.touch()
            if (event.key === Qt.Key_Escape) {
                root.no()
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                if (field.inputItem.activeFocus)
                    return // the field answers Enter itself
                if (noButton.activeFocus)
                    root.no()
                else if (detailsLink.activeFocus)
                    detailsLink.clicked()
                else
                    root.yes()
                event.accepted = true
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: RayePalette.flyoutRadius
            color: RayePalette.nightHigh
            border.width: 1
            border.color: RayePalette.highContrast ? "#ffffff" : "#1ff1f0f4"
        }

        Column {
            id: body
            width: parent.width
            topPadding: 20
            bottomPadding: 20
            leftPadding: 24
            rightPadding: 24
            spacing: 0

            readonly property real inner: width - leftPadding - rightPadding

            Row {
                spacing: 8
                height: 16
                Image {
                    width: 16
                    height: 16
                    source: RayePalette.eye
                    sourceSize: Qt.size(32, 32)
                    smooth: true
                    Accessible.ignored: true
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Strings.tr.rootAccess
                    color: RayePalette.textSecondary
                    font.family: RayePalette.family
                    font.pixelSize: Math.round(12 * RayePalette.textScale)
                }
            }

            Item { width: 1; height: 14 }

            Text {
                width: body.inner
                text: Strings.tr.allowChanges
                wrapMode: Text.WordWrap
                color: RayePalette.text
                font.family: RayePalette.displayFamily
                font.pixelSize: Math.round(20 * RayePalette.textScale)
                font.weight: Font.DemiBold
                lineHeight: 1.1
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Item { width: 1; height: 20 }

            // the app: a quiet amber frame when nobody vouches for it
            Rectangle {
                width: body.inner
                height: appRow.implicitHeight + (root.verified ? 0 : 20)
                radius: RayePalette.fieldRadius
                color: root.verified ? "transparent" : RayePalette.warningBand
                border.width: root.verified ? 0 : 1
                border.color: "#4dffb84d"

                Row {
                    id: appRow
                    x: root.verified ? 0 : 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 14

                    Image {
                        id: appIcon
                        width: 32
                        height: 32
                        sourceSize: Qt.size(64, 64)
                        source: "image://icon/" + (root.prompt && root.prompt.appIcon !== ""
                                                   ? root.prompt.appIcon : "application-x-executable")
                        Accessible.ignored: true
                    }

                    Column {
                        width: body.inner - appIcon.width - appRow.spacing - (root.verified ? 0 : 24)
                        spacing: 3
                        Text {
                            width: parent.width
                            text: root.prompt ? root.prompt.appName : ""
                            elide: Text.ElideRight
                            color: RayePalette.text
                            font.family: RayePalette.family
                            font.pixelSize: Math.round(14 * RayePalette.textScale)
                            font.weight: Font.DemiBold
                        }
                        Text {
                            width: parent.width
                            text: root.verified ? Strings.tr.verifiedPublisher.arg(root.prompt.publisher)
                                                : Strings.tr.unknownPublisher
                            wrapMode: Text.WordWrap
                            color: root.verified ? RayePalette.textSecondary : RayePalette.warning
                            font.family: RayePalette.family
                            font.pixelSize: Math.round(12 * RayePalette.textScale)
                        }
                        Text {
                            width: parent.width
                            text: Strings.tr.fileOrigin
                            color: RayePalette.textSecondary
                            font.family: RayePalette.family
                            font.pixelSize: Math.round(12 * RayePalette.textScale)
                        }
                    }
                }
            }

            Item { width: 1; height: 10 }

            TextLink {
                id: detailsLink
                objectName: "detailsLink"
                x: -4
                property bool open: false
                text: open ? Strings.tr.hideDetails : Strings.tr.showDetails
                onClicked: open = !open
                Accessible.description: open ? "" : Strings.tr.showDetails
            }

            Grid {
                id: details
                objectName: "details"
                visible: detailsLink.open
                columns: 2
                columnSpacing: 12
                rowSpacing: 6
                topPadding: 6

                // the longest label of the language, within reason
                readonly property real labelWidth: Math.min(180, Math.max(96, labelMetrics.advanceWidth + 4))

                TextMetrics {
                    id: labelMetrics
                    font.family: RayePalette.family
                    font.pixelSize: Math.round(12 * RayePalette.textScale)
                    text: [root.t.action, root.t.description, root.t.program, root.t.command]
                          .reduce(function (a, b) { return a.length >= b.length ? a : b })
                }
                readonly property var rows: root.prompt ? [
                    [root.t.action, root.prompt.actionId],
                    [root.t.description, root.prompt.message],
                    [root.t.program, root.prompt.program],
                    [root.t.command, root.prompt.command]
                ].filter(function (r) { return r[1] !== "" }) : []

                Repeater {
                    model: details.rows.length * 2
                    delegate: TextEdit {
                        readonly property bool label: index % 2 === 0
                        readonly property var row: details.rows[Math.floor(index / 2)]
                        width: label ? details.labelWidth : body.inner - details.labelWidth - details.columnSpacing
                        text: label ? row[0] : row[1]
                        readOnly: true
                        selectByMouse: !label
                        activeFocusOnPress: false
                        wrapMode: TextEdit.Wrap
                        color: label ? RayePalette.textSecondary : RayePalette.text
                        selectionColor: RayePalette.accent
                        font.family: RayePalette.family
                        font.pixelSize: Math.round(12 * RayePalette.textScale)
                        Accessible.role: Accessible.StaticText
                        Accessible.name: label ? text : row[0] + ": " + text
                    }
                }
            }

            Item { width: 1; height: 16 }
            Rectangle { width: body.inner; height: 1; color: "#1ff1f0f4" }
            Item { width: 1; height: 16 }

            Text {
                visible: !!root.prompt
                width: body.inner
                bottomPadding: 12
                text: root.prompt && root.prompt.userIsSelf ? Strings.tr.enterOwnPassword : Strings.tr.enterSudoPassword
                wrapMode: Text.WordWrap
                color: RayePalette.text
                font.family: RayePalette.family
                font.pixelSize: Math.round(14 * RayePalette.textScale)
            }

            Row {
                spacing: 12
                UserPicture {
                    size: 40
                    source: root.selected && root.selected.face !== "" ? "file://" + root.selected.face : ""
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: root.selected ? root.selected.name : ""
                        color: RayePalette.text
                        font.family: RayePalette.family
                        font.pixelSize: Math.round(14 * RayePalette.textScale)
                        font.weight: Font.DemiBold
                    }
                    Text {
                        visible: root.selected && root.selected.name.toLowerCase() !== root.selected.user.toLowerCase()
                        text: root.selected ? root.selected.user : ""
                        color: RayePalette.textSecondary
                        font.family: RayePalette.family
                        font.pixelSize: Math.round(12 * RayePalette.textScale)
                    }
                }
            }

            // more than one sudo user: pick another one
            TextLink {
                id: choicesLink
                visible: root.prompt && root.prompt.users.length > 1
                x: -4
                topPadding: 8
                property bool open: false
                text: Strings.tr.moreChoices
                onClicked: open = !open
            }

            Column {
                visible: choicesLink.visible && choicesLink.open
                width: body.inner
                Repeater {
                    model: root.prompt ? root.prompt.users : []
                    delegate: T.AbstractButton {
                        width: body.inner
                        height: 48
                        visible: modelData.user !== root.prompt.user
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.name
                        onClicked: {
                            root.prompt.user = modelData.user
                            choicesLink.open = false
                            field.clear()
                            field.focusField()
                        }
                        Keys.onReturnPressed: clicked()
                        background: Rectangle {
                            radius: RayePalette.controlRadius
                            color: parent.hovered ? RayePalette.hoverFill : "transparent"
                            border.width: parent.visualFocus ? RayePalette.focusWidth : 0
                            border.color: RayePalette.focus
                        }
                        contentItem: Row {
                            spacing: 12
                            leftPadding: 4
                            UserPicture {
                                anchors.verticalCenter: parent.verticalCenter
                                size: 32
                                source: modelData.face !== "" ? "file://" + modelData.face : ""
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name
                                color: RayePalette.text
                                font.family: RayePalette.family
                                font.pixelSize: Math.round(14 * RayePalette.textScale)
                            }
                        }
                    }
                }
            }

            Item { width: 1; height: 12 }

            PasswordField {
                id: field
                objectName: "passwordField"
                width: body.inner
                focus: true
                showSubmit: false
                placeholder: root.request && root.request.question !== ""
                             ? root.request.question.replace(/:\s*$/, "") : Strings.tr.password
                busy: root.request ? root.request.busy : false
                onAccepted: root.yes()
                Component.onCompleted: focusField()
            }

            Text {
                id: problem
                objectName: "problem"
                width: body.inner
                topPadding: 8
                visible: text !== ""
                text: root.request ? (root.request.wrongPassword ? Strings.tr.wrongPassword : root.request.message) : ""
                wrapMode: Text.WordWrap
                color: root.request && root.request.wrongPassword ? RayePalette.error : RayePalette.textSecondary
                font.family: RayePalette.family
                font.pixelSize: Math.round(12 * RayePalette.textScale)
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }

        Rectangle {
            id: footer
            anchors.bottom: parent.bottom
            width: parent.width
            height: 72
            radius: RayePalette.flyoutRadius
            color: "#140d20"
            // square top corners, round bottom ones
            Rectangle {
                width: parent.width
                height: parent.radius
                color: parent.color
            }
            Rectangle {
                width: parent.width
                height: 1
                color: "#14f1f0f4"
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                PromptButton {
                    id: yesButton
                    objectName: "yesButton"
                    text: Strings.tr.yes
                    accent: true
                    busy: root.request ? root.request.busy : false
                    enabled: !busy
                    onClicked: root.yes()
                }
                PromptButton {
                    id: noButton
                    objectName: "noButton"
                    text: Strings.tr.no
                    onClicked: root.no()
                }
            }
        }
    }

    readonly property var selected: {
        if (!prompt)
            return null
        for (var i = 0; i < prompt.users.length; ++i) {
            if (prompt.users[i].user === prompt.user)
                return prompt.users[i]
        }
        return prompt.users.length > 0 ? prompt.users[0] : null
    }

    Connections {
        target: root.request
        ignoreUnknownSignals: true
        function onStateChanged() {
            if (root.request.wrongPassword) {
                field.clear()
                field.shake()
                field.focusField()
            } else if (root.request.question !== "") {
                field.clear()
                field.focusField()
            }
        }
    }
}
