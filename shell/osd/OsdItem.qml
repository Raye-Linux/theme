// What the Raye on-screen display shows: a pill of Raye acrylic, fully
// round, 192 x 48. With a value (volume, brightness) it has the glyph, a
// thin bar filled with the accent and the number; for a message (caps
// lock, airplane mode, microphone muted, keyboard layout) the glyph and
// the words. While the volume changes and a player has a track, a media
// card with the cover, the title and the player's buttons sits above the
// pill. Everything fades and slides in, stays, then fades out just before
// plasmashell hides the window.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.private.mpris as Mpris
import org.raye.ui
import org.raye.shell

Item {
    id: root

    // --- set by plasmashell, through Osd.qml ------------------------------
    // how long the window stays: in, 1.5 s, out
    property int timeout: 167 + 1500 + 167
    // a number when showingProgress, else the words to show
    property var osdValue
    property int osdMaxValue: 100
    // with a number: what it is about (a display, a player)
    property string osdAdditionalText: ""
    property string icon
    property bool showingProgress: false

    // --- set by Osd.qml ---------------------------------------------------
    // the window is on screen
    property bool shownWindow: false
    // the screen to show on, in desktop coordinates: the screen in use,
    // set by Osd.qml at each show (the window's own screen until then)
    property rect area: Qt.rect(Screen.virtualX, Screen.virtualY, Screen.width, Screen.height)

    // --- read by Osd.qml --------------------------------------------------
    // the window, centred, with the pill 24 px above the taskbar
    readonly property int windowX: area.x + Math.round((area.width - width) / 2)
    readonly property int windowY: area.y + area.height - ShellTokens.taskbarHeight
        - bottomGap - height + shadowRoom
    // something on it takes clicks
    readonly property bool interactive: mediaShown && shownWindow

    // --- sizes --------------------------------------------------------------
    readonly property int pillHeight: 48
    readonly property int pillMinWidth: 192
    readonly property int pillMaxWidth: 400
    readonly property int pillPadding: 16
    readonly property int gap: 12
    readonly property int glyphSize: 16
    readonly property int trackWidth: 92
    readonly property int trackHeight: 4
    readonly property int bottomGap: 24
    readonly property int cardWidth: 360
    readonly property int cardHeight: 76
    readonly property int cardGap: 8
    readonly property int coverSize: 48
    readonly property int mediaButtonSize: 32
    // room around the surfaces for their shadow
    readonly property int shadowRoom: 24
    readonly property int slideDistance: 8

    // --- state --------------------------------------------------------------
    readonly property real number: Number(osdValue) || 0
    readonly property real fraction: osdMaxValue > 0 ? Math.max(0, Math.min(1, number / osdMaxValue)) : 0
    readonly property bool volume: showingProgress && icon.startsWith("audio-volume")
    readonly property var player: mpris.currentPlayer
    readonly property bool hasMedia: player !== null && player !== undefined
        && (player.track ?? "") !== ""
        && (player.playbackStatus === Mpris.PlaybackStatus.Playing
            || player.playbackStatus === Mpris.PlaybackStatus.Paused)
    readonly property bool mediaShown: volume && hasMedia
    readonly property bool playing: hasMedia && player.playbackStatus === Mpris.PlaybackStatus.Playing

    width: Math.max(pill.width, mediaShown ? cardWidth : 0) + 2 * shadowRoom
    height: pillHeight + (mediaShown ? cardHeight + cardGap : 0) + 2 * shadowRoom
    implicitWidth: width
    implicitHeight: height

    // plasmashell showed the window, or showed it again with new values
    function show() {
        exitAnimation.stop();
        if (content.opacity < 1 && !enterAnimation.running) {
            enterAnimation.restart();
        }
        exitTimer.interval = Math.max(0, timeout - ShellTokens.durationFast);
        exitTimer.restart();
    }

    // the window is gone: start from nothing next time
    function hidden() {
        enterAnimation.stop();
        exitAnimation.stop();
        exitTimer.stop();
        content.opacity = 0;
        content.slide = slideDistance;
    }

    onOsdValueChanged: if (shownWindow) show()
    onIconChanged: if (shownWindow) show()
    onOsdAdditionalTextChanged: if (shownWindow) show()

    Mpris.Mpris2Model {
        id: mpris
    }

    Timer {
        id: exitTimer
        onTriggered: exitAnimation.restart()
    }

    ParallelAnimation {
        id: enterAnimation
        NumberAnimation {
            target: content; property: "opacity"; to: 1
            duration: ShellTokens.durationFast; easing: Motion.entrance
        }
        NumberAnimation {
            target: content; property: "slide"; to: 0
            duration: ShellTokens.durationFast; easing: Motion.entrance
        }
    }

    ParallelAnimation {
        id: exitAnimation
        NumberAnimation {
            target: content; property: "opacity"; to: 0
            duration: ShellTokens.durationFast; easing: Motion.exit
        }
        NumberAnimation {
            target: content; property: "slide"; to: root.slideDistance
            duration: ShellTokens.durationFast; easing: Motion.exit
        }
    }

    Item {
        id: content
        objectName: "osdContent"
        property real slide: root.slideDistance
        anchors.fill: parent
        opacity: 0
        transform: Translate { y: content.slide }

        // --- media card -----------------------------------------------------
        Item {
            id: card
            objectName: "osdMediaCard"
            width: root.cardWidth
            height: root.cardHeight
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: pill.top
            anchors.bottomMargin: root.cardGap
            visible: root.mediaShown

            Shadow { anchors.fill: parent; radius: ShellTokens.radiusFlyout }
            AcrylicSurface { anchors.fill: parent; radius: ShellTokens.radiusFlyout }

            RowLayout {
                anchors.fill: parent
                anchors.margins: (root.cardHeight - root.coverSize) / 2
                spacing: root.gap

                Item {
                    Layout.preferredWidth: root.coverSize
                    Layout.preferredHeight: root.coverSize

                    Rectangle {
                        anchors.fill: parent
                        radius: Tokens.radiusControl
                        color: Tokens.fillControl
                        visible: cover.status !== Image.Ready
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            source: "view-media-track"
                            isMask: true
                            color: Tokens.textSecondary
                        }
                    }
                    Kirigami.ShadowedImage {
                        id: cover
                        objectName: "osdCover"
                        anchors.fill: parent
                        radius: Tokens.radiusControl
                        source: root.player ? root.player.artUrl : ""
                        sourceSize.width: root.coverSize * 2
                        sourceSize.height: root.coverSize * 2
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        visible: status === Image.Ready
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 0

                    Text {
                        objectName: "osdMediaTitle"
                        Layout.fillWidth: true
                        text: root.player ? (root.player.track ?? "") : ""
                        font: Tokens.bodyStrong
                        color: Tokens.textPrimary
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                    Text {
                        objectName: "osdMediaArtist"
                        Layout.fillWidth: true
                        text: root.player ? (root.player.artist || root.player.identity || "") : ""
                        font: Tokens.caption
                        color: Tokens.textSecondary
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                        visible: text !== ""
                    }
                }

                RowLayout {
                    spacing: 2
                    MediaButton {
                        objectName: "osdPrevious"
                        iconName: "media-skip-backward"
                        label: i18nd("raye_shell_osd", "Previous track")
                        enabled: root.player ? root.player.canGoPrevious : false
                        onClicked: root.player.Previous()
                    }
                    MediaButton {
                        objectName: "osdPlayPause"
                        iconName: root.playing ? "media-playback-pause" : "media-playback-start"
                        label: root.playing ? i18nd("raye_shell_osd", "Pause") : i18nd("raye_shell_osd", "Play")
                        enabled: root.player ? (root.playing ? root.player.canPause : root.player.canPlay) : false
                        onClicked: root.player.PlayPause()
                    }
                    MediaButton {
                        objectName: "osdNext"
                        iconName: "media-skip-forward"
                        label: i18nd("raye_shell_osd", "Next track")
                        enabled: root.player ? root.player.canGoNext : false
                        onClicked: root.player.Next()
                    }
                }
            }
        }

        // --- pill -------------------------------------------------------------
        Item {
            id: pill
            objectName: "osdPill"
            width: Math.min(root.pillMaxWidth, Math.max(root.pillMinWidth, naturalWidth))
            // what the pill needs for its glyph, its value or its words
            readonly property real naturalWidth: 2 * root.pillPadding
                + (glyph.visible ? root.glyphSize + root.gap : 0)
                + (root.showingProgress
                   ? (additional.visible ? Math.min(additional.implicitWidth, additional.Layout.maximumWidth) + root.gap : 0)
                     + root.trackWidth + root.gap + valueText.Layout.preferredWidth
                   : Math.ceil(label.implicitWidth))
            height: root.pillHeight
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.shadowRoom

            Accessible.role: Accessible.StatusBar
            Accessible.name: root.showingProgress
                ? [root.osdAdditionalText, valueText.text].filter(s => s !== "").join(" ")
                : label.text

            Shadow { anchors.fill: parent; radius: height / 2 }
            AcrylicSurface { anchors.fill: parent; radius: height / 2 }

            RowLayout {
                id: row
                anchors.fill: parent
                anchors.leftMargin: root.pillPadding
                anchors.rightMargin: root.pillPadding
                spacing: root.gap

                Kirigami.Icon {
                    id: glyph
                    objectName: "osdGlyph"
                    Layout.preferredWidth: root.glyphSize
                    Layout.preferredHeight: root.glyphSize
                    Layout.alignment: Qt.AlignVCenter
                    source: root.icon
                    // the OSD glyphs are one-colour symbols: draw them in the text colour
                    isMask: true
                    color: Tokens.textPrimary
                    visible: valid
                }

                // what the value is about, when plasmashell says (a display, a player)
                Text {
                    id: additional
                    objectName: "osdAdditionalText"
                    Layout.maximumWidth: 140
                    text: root.osdAdditionalText
                    font: Tokens.caption
                    color: Tokens.textSecondary
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    visible: root.showingProgress && text !== ""
                }

                Item {
                    id: track
                    objectName: "osdTrack"
                    Layout.fillWidth: true
                    Layout.minimumWidth: root.trackWidth
                    implicitWidth: root.trackWidth
                    implicitHeight: root.trackHeight
                    Layout.alignment: Qt.AlignVCenter
                    visible: root.showingProgress

                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: Qt.alpha(Tokens.textPrimary, 0.24)
                    }
                    Rectangle {
                        id: fill
                        objectName: "osdFill"
                        width: Math.round(parent.width * root.fraction)
                        height: parent.height
                        radius: height / 2
                        color: Tokens.accent
                        Behavior on width {
                            enabled: root.shownWindow
                            NumberAnimation { duration: ShellTokens.durationFaster; easing: Motion.pointToPoint }
                        }
                    }
                }

                Text {
                    id: valueText
                    objectName: "osdValueText"
                    Layout.preferredWidth: valueMetrics.advanceWidth
                    Layout.alignment: Qt.AlignVCenter
                    text: String(Math.round(root.number))
                    font: valueFont
                    // over 100 % the speakers may distort: say so gently
                    color: root.number > 100 ? Tokens.statusCaution : Tokens.textPrimary
                    horizontalAlignment: Text.AlignRight
                    textFormat: Text.PlainText
                    visible: root.showingProgress

                    // tabular figures: the number keeps its width while it changes
                    readonly property font valueFont: Qt.font({
                        family: Tokens.caption.family,
                        pointSize: Tokens.caption.pointSize,
                        weight: 600,
                        features: { "tnum": 1 }
                    })
                    TextMetrics {
                        id: valueMetrics
                        font: valueText.valueFont
                        text: String(Math.max(100, root.osdMaxValue))
                    }
                }

                Text {
                    id: label
                    objectName: "osdLabel"
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: root.showingProgress ? "" : String(root.osdValue ?? "")
                    font: Tokens.body
                    color: Tokens.textPrimary
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    visible: !root.showingProgress
                }
            }
        }
    }

    // a soft shadow under a surface, drawn without shaders
    component Shadow: Item {
        id: shadow
        property real radius
        Repeater {
            model: 4
            Rectangle {
                required property int index
                readonly property real spread: (index + 1) * 3
                x: -spread
                y: -spread + 4
                width: shadow.width + 2 * spread
                height: shadow.height + 2 * spread
                radius: shadow.radius + spread
                color: Qt.alpha(Tokens.shadow, Tokens.night ? 0.1 - index * 0.02 : 0.05 - index * 0.01)
            }
        }
    }

    // previous, play or pause, next
    component MediaButton: Item {
        id: button
        property string iconName
        property string label
        signal clicked()

        Layout.preferredWidth: root.mediaButtonSize
        Layout.preferredHeight: root.mediaButtonSize
        Layout.alignment: Qt.AlignVCenter
        opacity: enabled ? 1 : 0.4
        activeFocusOnTab: true

        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: if (enabled) clicked()

        Rectangle {
            anchors.fill: parent
            radius: Tokens.radiusControl
            color: mouse.pressed ? ShellTokens.pressFill : mouse.containsMouse ? ShellTokens.hoverFill : "transparent"
            Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }
        }
        Kirigami.Icon {
            anchors.centerIn: parent
            width: 16
            height: 16
            source: button.iconName
            isMask: true
            color: Tokens.textPrimary
            scale: mouse.pressed ? ShellTokens.pressScale : 1
            Behavior on scale { NumberAnimation { duration: ShellTokens.durationFaster } }
        }
        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: button.enabled
            onClicked: button.clicked()
        }
    }
}
