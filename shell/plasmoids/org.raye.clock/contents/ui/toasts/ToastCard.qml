// One toast: the Raye acrylic card, above the clock. Its header holds the
// app's icon and name, the "..." menu and the close button, then the
// title, three lines of text, a picture, the app's buttons and its reply
// field. It slides in from the right and runs out by itself unless the
// pointer is on it, a reply is being typed or its menu is open.
import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrolsaddons as KQuickAddons
import org.raye.ui
import org.raye.shell

import ".."
import "toastlogic.js" as Logic

Item {
    id: toast

    property string applicationName: ""
    property string applicationIconName: ""
    property string desktopEntry: ""
    property string notifyRcName: ""
    property string summary: ""
    property string body: ""
    property var iconName: ""
    property var image
    property var urls: []
    property var actionNames: []
    property var actionLabels: []
    property bool hasDefaultAction: false
    property bool critical: false
    property bool isJob: false
    property bool killable: false
    property bool suspendable: false
    // 0 stopped, 1 running, 2 suspended, as in the notification model
    property int jobState: 0
    property int percentage: 0
    property string jobError: ""
    property bool hasReplyAction: false
    property string replyActionLabel: ""
    property string replyPlaceholderText: ""
    property string replySubmitButtonText: ""
    // from toastlogic.timeoutMs(); 0: it stays
    property int timeoutMs: 5000
    property bool menuOpen: false
    // room around the card, inside the window, for the shadow and the slide
    readonly property int margin: 8
    readonly property int rightMargin: Logic.EDGE

    readonly property var source: Logic.source(applicationName, applicationIconName || Logic.themeIcon(iconName), desktopEntry, notifyRcName, isJob)
    readonly property var networkEvent: Logic.networkEvent(notifyRcName, body)
    readonly property string thumbnailUrl: isJob ? "" : Logic.thumbnail(urls)
    readonly property bool hovered: pointer.hovered
    property bool replying: false
    property bool bodyOpen: false
    readonly property bool leaving: state === "leaving"
    readonly property bool timing: timer.running
    readonly property string shownTitle: networkEvent ? words.networkTitle(networkEvent) : Logic.title(summary)
    readonly property string shownBody: networkEvent ? "" : Logic.plain(body)

    signal closeRequested()
    signal timeUp()
    signal defaultInvoked()
    signal actionInvoked(string name)
    signal replied(string text)
    signal menuRequested(Item button)
    signal killRequested()
    signal suspendRequested()
    signal resumeRequested()
    signal thumbnailOpened(string url)
    signal pointerEntered()

    width: Logic.WIDTH + margin + rightMargin
    height: card.height + 2 * margin
    implicitWidth: width
    implicitHeight: height

    ToastSource { id: words }

    // A second arrival replaces the words of this one: it gets its time again.
    function restartTime() {
        if (timer.running)
            timer.restart()
    }

    // here, not in the button: the button goes away as soon as the reply starts
    function beginReply() {
        replying = true
        replyInput.forceActiveFocus()
    }

    function leave(then) {
        afterLeaving = then
        state = "leaving"
    }
    property var afterLeaving: null

    Timer {
        id: timer
        interval: Math.max(1, toast.timeoutMs)
        running: toast.timeoutMs > 0 && !toast.hovered && !toast.replying && !toast.menuOpen && !toast.leaving
        onTriggered: toast.leave(() => toast.timeUp())
    }

    HoverHandler {
        id: pointer
        onHoveredChanged: if (hovered) toast.pointerEntered()
        cursorShape: toast.hasDefaultAction ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    Item {
        id: slide
        x: toast.margin
        y: toast.margin
        width: Logic.WIDTH
        height: card.height
        opacity: 0
        transform: Translate { id: offset; x: Logic.WIDTH / 3 }

        // soft shadow, drawn inside the window as the centre's cards do
        Repeater {
            model: 3
            Rectangle {
                required property int index
                anchors.fill: parent
                anchors.margins: -(index + 1) * 2
                anchors.topMargin: -(index + 1) * 2 + 2
                radius: ShellTokens.radiusFlyout + (index + 1) * 2
                color: Qt.rgba(0, 0, 0, Tokens.night ? 0.08 : 0.03)
            }
        }

        AcrylicSurface {
            anchors.fill: parent
            focused: toast.critical
        }

        Item {
            id: card
            width: parent.width
            height: column.implicitHeight + 8 + 12

            TapHandler {
                enabled: toast.hasDefaultAction
                onTapped: toast.defaultInvoked()
            }
            TapHandler {
                acceptedButtons: Qt.MiddleButton
                onTapped: toast.closeRequested()
            }

            Column {
                id: column
                anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 8 }
                spacing: 4

                // the header: icon, app, "...", close
                Item {
                    anchors { left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 8 }
                    height: 28
                    Kirigami.Icon {
                        id: appIcon
                        objectName: "appIcon"
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                        width: 16
                        height: 16
                        source: toast.source.icon
                    }
                    Text {
                        objectName: "appName"
                        anchors { left: appIcon.right; leftMargin: 8; right: controls.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        text: words.nameOf(toast.source)
                        color: ShellTokens.textSecondary
                        font: ShellTokens.caption
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                    Row {
                        id: controls
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        spacing: 2
                        FlyoutButton {
                            id: moreButton
                            objectName: "more"
                            width: 28
                            height: 28
                            visible: !toast.isJob
                            toolTipText: i18nd("raye_shell_clock", "More options")
                            contentItem: Item {
                                Row {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Repeater {
                                        model: 3
                                        Rectangle { width: 3; height: 3; radius: 1.5; color: ShellTokens.text }
                                    }
                                }
                            }
                            onClicked: toast.menuRequested(moreButton)
                        }
                        FlyoutButton {
                            objectName: "close"
                            width: 28
                            height: 28
                            icon.name: "window-close-symbolic"
                            toolTipText: toast.isJob && toast.jobState !== 0 ? i18nd("raye_shell_clock", "Hide") : i18nd("raye_shell_clock", "Close")
                            onClicked: toast.leave(() => toast.closeRequested())
                        }
                    }
                }

                // a picture of its own beside the words (a sender's photo)
                Item {
                    anchors { left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 }
                    implicitHeight: Math.max(words2.implicitHeight, sidePicture.visible ? sidePicture.height : 0)
                    height: implicitHeight

                    Kirigami.Icon {
                        id: sidePicture
                        objectName: "sidePicture"
                        anchors { left: parent.left; top: parent.top; topMargin: 2 }
                        width: 40
                        height: 40
                        visible: Logic.pictureFile(toast.iconName) !== ""
                        source: Logic.pictureFile(toast.iconName)
                    }

                    Column {
                        id: words2
                        anchors { left: sidePicture.visible ? sidePicture.right : parent.left; leftMargin: sidePicture.visible ? 12 : 0; right: parent.right }
                        spacing: 2

                        Text {
                            objectName: "summary"
                            width: parent.width
                            text: toast.shownTitle
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            color: ShellTokens.text
                            font: ShellTokens.bodyStrong
                            textFormat: Text.PlainText
                            visible: text !== ""
                        }
                        Text {
                            id: bodyText
                            objectName: "body"
                            width: parent.width
                            text: toast.isJob && toast.jobError !== "" ? Logic.plain(toast.jobError) : toast.shownBody
                            wrapMode: Text.Wrap
                            maximumLineCount: toast.bodyOpen ? Logic.BODY_LINES_OPEN : Logic.BODY_LINES
                            elide: Text.ElideRight
                            // with no title of its own, the text is what you read first
                            color: toast.shownTitle === "" ? ShellTokens.text : ShellTokens.textSecondary
                            font: ShellTokens.body
                            textFormat: Text.PlainText
                            visible: text !== ""
                        }
                        Text {
                            objectName: "moreText"
                            visible: bodyText.truncated && !toast.bodyOpen
                            text: i18nd("raye_shell_clock", "See more")
                            color: ShellTokens.accent
                            font: ShellTokens.caption
                            TapHandler { onTapped: toast.bodyOpen = true }
                        }
                    }
                }

                // a job's progress: the bar and the percentage
                Item {
                    objectName: "progress"
                    anchors { left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 }
                    visible: toast.isJob && toast.jobState !== 0
                    height: visible ? 24 : 0
                    Rectangle {
                        anchors { left: parent.left; right: percent.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        height: 4
                        radius: 2
                        color: Qt.alpha(ShellTokens.text, 0.15)
                        Rectangle {
                            height: parent.height
                            radius: 2
                            width: parent.width * Math.max(0, Math.min(100, toast.percentage)) / 100
                            color: toast.jobState === 2 ? Tokens.statusCaution : ShellTokens.accent
                        }
                    }
                    Text {
                        id: percent
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        text: i18ndc("raye_shell_clock", "progress of a file copy", "%1%", Math.round(toast.percentage))
                        color: ShellTokens.textSecondary
                        font.family: "Inter"
                        font.pixelSize: 12
                        font.features: { "tnum": 1 }
                    }
                }

                // the picture an app sent, or the file it offers
                KQuickAddons.QImageItem {
                    id: picture
                    objectName: "picture"
                    anchors { left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 }
                    visible: !toast.isJob && nativeWidth > 0
                    height: visible ? Math.min(180, Math.round(width * nativeHeight / Math.max(1, nativeWidth))) : 0
                    fillMode: KQuickAddons.QImageItem.PreserveAspectCrop
                    smooth: true
                    Binding on image {
                        when: toast.image !== undefined && toast.image !== null
                        value: toast.image
                    }
                }
                Image {
                    objectName: "thumbnail"
                    anchors { left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 }
                    visible: toast.thumbnailUrl !== "" && status === Image.Ready && !picture.visible
                    source: toast.thumbnailUrl
                    asynchronous: true
                    sourceSize.width: 2 * Logic.WIDTH
                    fillMode: Image.PreserveAspectCrop
                    height: visible ? Math.min(180, Math.round(width * implicitHeight / Math.max(1, implicitWidth))) : 0
                    TapHandler { onTapped: toast.thumbnailOpened(toast.thumbnailUrl) }
                }

                // the buttons, side by side, the full width
                Row {
                    id: buttons
                    objectName: "buttons"
                    anchors { left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 }
                    readonly property var entries: {
                        const out = []
                        if (toast.isJob) {
                            if (toast.jobState === 1 && toast.suspendable)
                                out.push({ id: "suspend", label: i18nd("raye_shell_clock", "Pause") })
                            if (toast.jobState === 2)
                                out.push({ id: "resume", label: i18nd("raye_shell_clock", "Resume") })
                            if (toast.jobState !== 0 && toast.killable)
                                out.push({ id: "kill", label: i18nd("raye_shell_clock", "Cancel") })
                            return out
                        }
                        if (toast.hasReplyAction && !toast.replying)
                            out.push({ id: "reply", label: toast.replyActionLabel || i18nd("raye_shell_clock", "Reply") })
                        for (let i = 0; i < toast.actionNames.length && out.length < 3; ++i)
                            out.push({ id: "action", name: toast.actionNames[i], label: toast.actionLabels[i] || "" })
                        return out
                    }
                    visible: entries.length > 0
                    spacing: 8
                    topPadding: 4
                    Repeater {
                        model: buttons.entries
                        FlyoutButton {
                            required property var modelData
                            objectName: modelData.id === "action" ? "action:" + modelData.name : modelData.id
                            width: (buttons.width - (buttons.entries.length - 1) * buttons.spacing) / buttons.entries.length
                            text: modelData.label
                            background: Rectangle {
                                radius: ShellTokens.radiusButton
                                color: parent.down ? Tokens.fillControlPressed : parent.hovered ? Tokens.fillControlHover : Tokens.fillControl
                                border.width: 1
                                border.color: Tokens.strokeControl
                            }
                            onClicked: {
                                switch (modelData.id) {
                                case "reply": toast.beginReply(); break
                                case "suspend": toast.suspendRequested(); break
                                case "resume": toast.resumeRequested(); break
                                case "kill": toast.killRequested(); break
                                default: toast.actionInvoked(modelData.name)
                                }
                            }
                        }
                    }
                }

                // the reply field
                Item {
                    objectName: "replyRow"
                    anchors { left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 }
                    visible: toast.replying
                    height: visible ? 36 : 0
                    Rectangle {
                        anchors { left: parent.left; right: sendButton.left; rightMargin: 8; top: parent.top; bottom: parent.bottom; topMargin: 2; bottomMargin: 2 }
                        radius: ShellTokens.radiusButton
                        color: Tokens.fillControl
                        border.width: 1
                        border.color: replyInput.activeFocus ? ShellTokens.accent : Tokens.strokeControl
                        TextInput {
                            id: replyInput
                            objectName: "replyInput"
                            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                            verticalAlignment: TextInput.AlignVCenter
                            color: ShellTokens.text
                            selectionColor: ShellTokens.accent
                            font: ShellTokens.body
                            clip: true
                            Keys.onReturnPressed: sendButton.send()
                            Keys.onEnterPressed: sendButton.send()
                            Keys.onEscapePressed: { toast.replying = false; text = "" }
                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                visible: replyInput.text === ""
                                text: toast.replyPlaceholderText || i18nd("raye_shell_clock", "Write a reply")
                                color: ShellTokens.textSecondary
                                font: ShellTokens.body
                                elide: Text.ElideRight
                            }
                        }
                    }
                    FlyoutButton {
                        id: sendButton
                        objectName: "send"
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        accent: true
                        enabled: replyInput.text.trim() !== ""
                        text: toast.replySubmitButtonText || i18nd("raye_shell_clock", "Send")
                        function send() {
                            if (enabled)
                                toast.replied(replyInput.text)
                        }
                        onClicked: send()
                    }
                }
            }
        }
    }

    // in from the right; out the same way, faster
    state: "shown"
    states: [
        State { name: "shown"; PropertyChanges { slide.opacity: 1; offset.x: 0 } },
        State { name: "leaving"; PropertyChanges { slide.opacity: 0; offset.x: Logic.WIDTH / 3 } }
    ]
    transitions: [
        Transition {
            to: "shown"
            NumberAnimation { target: offset; property: "x"; duration: ShellTokens.durationNormal > 0 ? Logic.SLIDE_IN : 0; easing: Motion.entrance }
            NumberAnimation { target: slide; property: "opacity"; duration: ShellTokens.durationNormal > 0 ? Logic.SLIDE_IN : 0; easing: Motion.entrance }
        },
        Transition {
            to: "leaving"
            SequentialAnimation {
                ParallelAnimation {
                    NumberAnimation { target: offset; property: "x"; duration: ShellTokens.durationNormal > 0 ? Logic.SLIDE_OUT : 0; easing: Motion.exit }
                    NumberAnimation { target: slide; property: "opacity"; duration: ShellTokens.durationNormal > 0 ? Logic.SLIDE_OUT : 0 }
                }
                ScriptAction {
                    script: {
                        const then = toast.afterLeaving
                        toast.afterLeaving = null
                        if (then)
                            then()
                    }
                }
            }
        }
    ]
}
