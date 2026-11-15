// The hover preview of a running app: after 400 ms over its button, a
// flyout on the Raye acrylic above it with one card per window (the
// app's icon and the window title, a live picture of the window, a close
// button on hover). A click on a card brings that window up. Moving to
// another button swaps the cards at once; leaving both the button and
// the flyout closes it.
//
// The taskbar loads this file by name and talks to it through:
//   available          true: the taskbar shows no tooltip for running apps
//   controller         the taskbar's controller (windowsOf, activateWindow, closeWindow)
//   hover(index, button), leave(index, button), hide()
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.raye.ui
import org.raye.shell

Item {
    id: thumbnails

    readonly property bool available: true
    property var controller
    // the button the cards belong to, and its row in the tasks model
    property Item button: null
    property int index: -1
    property var windows: []

    readonly property int cardWidth: 200
    readonly property int pictureHeight: 112

    function hover(index, button) {
        closeTimer.stop()
        if (!controller || button.isLauncher || button.isStartup) {
            hide()
            return
        }
        pending = button
        pendingIndex = index
        // already open over another button: swap without waiting
        if (flyout.visible)
            show()
        else
            openTimer.restart()
    }

    function leave(index, button) {
        if (pending === button)
            openTimer.stop()
        if (flyout.visible)
            closeTimer.restart()
    }

    function hide() {
        openTimer.stop()
        closeTimer.stop()
        flyout.visible = false
    }

    property Item pending: null
    property int pendingIndex: -1

    function show() {
        if (!pending || !controller)
            return
        const list = controller.windowsOf(pendingIndex)
        if (list.length === 0) {
            hide()
            return
        }
        button = pending
        index = pendingIndex
        windows = list
        place()
        if (!flyout.visible) {
            cards.opacity = 0
            flyout.visible = true
            entrance.restart()
        }
    }

    // centred over the button, the flyout gap above the taskbar, on screen
    function place() {
        const w = button.Window.window
        const p = button.mapToGlobal(0, 0)
        const width = cards.implicitWidth + 2 * frame.padding
        const height = cards.implicitHeight + 2 * frame.padding
        let x = Math.round(p.x + (button.width - width) / 2)
        if (w && w.screen)
            x = Math.max(w.screen.virtualX + 8, Math.min(x, w.screen.virtualX + w.screen.width - width - 8))
        flyout.x = x
        flyout.y = Math.round(p.y - (ShellTokens.taskbarHeight - button.height) / 2 - ShellTokens.flyoutGap - height)
    }

    function refresh() {
        if (flyout.visible && controller) {
            const list = controller.windowsOf(index)
            if (list.length === 0)
                hide()
            else
                windows = list
        }
    }

    Timer {
        id: openTimer
        interval: 400
        onTriggered: thumbnails.show()
    }
    Timer {
        id: closeTimer
        interval: 300
        onTriggered: if (!flyoutHover.hovered) thumbnails.hide()
    }

    // a window of its own, placed by hand: a popup of the panel window
    // cannot be given a gap above the taskbar
    PlasmaCore.Dialog {
        id: flyout

        location: PlasmaCore.Types.Floating
        type: PlasmaCore.Dialog.Tooltip
        flags: Qt.WindowDoesNotAcceptFocus
        backgroundHints: PlasmaCore.Dialog.NoBackground
        visible: false

        mainItem: Item {
            id: frame
            readonly property int padding: 8
            implicitWidth: cards.implicitWidth + 2 * padding
            implicitHeight: cards.implicitHeight + 2 * padding
            width: implicitWidth
            height: implicitHeight

            HoverHandler {
                id: flyoutHover
                onHoveredChanged: if (hovered) closeTimer.stop(); else closeTimer.restart()
            }

            AcrylicSurface {
                anchors.fill: parent
            }

            Row {
                id: cards
                x: frame.padding
                y: frame.padding
                spacing: 8
                onImplicitWidthChanged: if (flyout.visible) thumbnails.place()

                Repeater {
                    model: thumbnails.windows

                    delegate: Item {
                        id: card
                        required property var modelData
                        required property int index
                        width: thumbnails.cardWidth
                        height: 32 + thumbnails.pictureHeight + 8
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.title

                        HoverHandler { id: cardHover }
                        TapHandler {
                            onTapped: {
                                thumbnails.controller.activateWindow(thumbnails.index, card.modelData.child)
                                thumbnails.hide()
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: ShellTokens.radiusButton
                            color: cardHover.hovered ? ShellTokens.hoverFill : "transparent"
                            border.width: card.modelData.active ? 1 : 0
                            border.color: Qt.alpha(ShellTokens.accent, 0.7)
                            Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }
                        }

                        // icon and title
                        Kirigami.Icon {
                            id: cardIcon
                            x: 8
                            y: 8
                            width: 16
                            height: 16
                            source: card.modelData.icon
                        }
                        Text {
                            anchors { left: cardIcon.right; leftMargin: 8; right: closeButton.left; rightMargin: 4; verticalCenter: cardIcon.verticalCenter }
                            text: card.modelData.title
                            elide: Text.ElideRight
                            color: ShellTokens.text
                            font: ShellTokens.caption
                            textFormat: Text.PlainText
                        }
                        T.AbstractButton {
                            id: closeButton
                            objectName: "closeWindow"
                            width: 24
                            height: 24
                            x: card.width - width - 4
                            y: 4
                            visible: cardHover.hovered
                            Accessible.name: i18nd("raye_shell_taskbar", "Close window")
                            background: Rectangle {
                                radius: ShellTokens.radiusButton
                                color: closeButton.down ? "#c42b1c" : closeButton.hovered ? "#e81123" : "transparent"
                            }
                            contentItem: Kirigami.Icon {
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                source: "window-close-symbolic"
                                isMask: true
                                color: ShellTokens.text
                            }
                            onClicked: {
                                thumbnails.controller.closeWindow(thumbnails.index, card.modelData.child)
                                refreshTimer.restart()
                            }
                        }

                        // the live picture of the window, or the app's icon
                        Item {
                            x: 8
                            y: 32
                            width: card.width - 16
                            height: thumbnails.pictureHeight

                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: 48
                                height: 48
                                source: card.modelData.icon
                                visible: !picture.item || !picture.item.shown
                            }
                            Loader {
                                id: picture
                                anchors.fill: parent
                                active: flyout.visible && String(card.modelData.winId || "") !== ""
                                source: "WindowPicture.qml"
                                onLoaded: item.winId = String(card.modelData.winId)
                            }
                        }
                    }
                }
            }

            ParallelAnimation {
                id: entrance
                NumberAnimation { target: cards; property: "opacity"; from: 0; to: 1; duration: ShellTokens.durationFast; easing: Motion.entrance }
                NumberAnimation { target: cards; property: "y"; from: frame.padding + ShellTokens.menuSlide; to: frame.padding; duration: ShellTokens.durationFast; easing: Motion.entrance }
            }
        }
    }

    // a closed window leaves the model a moment later
    Timer {
        id: refreshTimer
        interval: 250
        onTriggered: thumbnails.refresh()
    }
}
