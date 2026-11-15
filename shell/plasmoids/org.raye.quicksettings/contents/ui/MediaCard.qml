// What is playing: a card of its own above the panel, with the cover, the
// title and artist, previous, play or pause, next.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.shell

Item {
    id: card

    property QtObject backend

    function playPause() { backend.mediaPlayPause() }

    implicitHeight: 88

    AcrylicSurface {
        anchors.fill: parent
    }

    Rectangle {
        id: cover
        x: 12
        y: 12
        width: 64
        height: 64
        radius: 4
        color: Qt.alpha(ShellTokens.accent, 0.25)
        clip: true
        Kirigami.Icon {
            anchors.centerIn: parent
            visible: art.status !== Image.Ready
            width: 24
            height: 24
            source: "media-playback-start-symbolic"
            color: ShellTokens.text
            isMask: true
        }
        Image {
            id: art
            anchors.fill: parent
            source: card.backend ? card.backend.mediaArt : ""
            sourceSize: Qt.size(128, 128)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }
    }

    Column {
        anchors.left: cover.right
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        y: 10
        spacing: 0
        Text {
            width: parent.width
            text: card.backend ? card.backend.mediaSource : ""
            font: ShellTokens.caption
            color: ShellTokens.textSecondary
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
        Text {
            width: parent.width
            text: card.backend ? card.backend.mediaTitle : ""
            font: ShellTokens.bodyStrong
            color: ShellTokens.text
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
        Text {
            width: parent.width
            text: card.backend ? card.backend.mediaArtist : ""
            font: ShellTokens.caption
            color: ShellTokens.textSecondary
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        spacing: 0

        component MediaButton: PanelButton {
            property string glyph
            width: 32
            height: 32
            onTaskbar: false
            Kirigami.Icon {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: parent.parent ? glyph : ""
                color: ShellTokens.text
                isMask: true
            }
        }
        MediaButton {
            glyph: "media-skip-backward-symbolic"
            enabled: card.backend && card.backend.mediaCanPrevious
            toolTipText: i18nd("raye_shell_quicksettings", "Previous")
            onClicked: card.backend.mediaPrevious()
        }
        MediaButton {
            glyph: card.backend && card.backend.mediaPlaying ? "media-playback-pause-symbolic" : "media-playback-start-symbolic"
            toolTipText: card.backend && card.backend.mediaPlaying ? i18nd("raye_shell_quicksettings", "Pause")
                                                                   : i18nd("raye_shell_quicksettings", "Play")
            onClicked: card.playPause()
        }
        MediaButton {
            glyph: "media-skip-forward-symbolic"
            enabled: card.backend && card.backend.mediaCanNext
            toolTipText: i18nd("raye_shell_quicksettings", "Next")
            onClicked: card.backend.mediaNext()
        }
    }
}
