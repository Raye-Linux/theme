// The media card of the lock screen: when something plays, a 360 x 72
// card at the bottom centre with the cover, the title and artist, and
// previous / play-pause / next, like the media controls of Windows' lock
// screen.
//
// In:  model   an MPRIS model (one row per player; the session's
//              multiplexer gives one row for the player in front). Roles
//              used: track, artist, identity, artUrl, playbackStatus,
//              canControl, canGoPrevious, canGoNext, container (with
//              Previous(), PlayPause(), Next()).
//      lang    "en" or "fr" for the button names
// Out: shown   true while a track is playing or paused
//      used()  a button was pressed (the lock screen counts it as input)
import QtQuick
import QtQuick.Templates as T
import org.raye.signin
import "hints.js" as Hints

Item {
    id: card

    property var model: null
    property string lang: Strings.language

    // MPRIS playback states, as the model gives them
    readonly property int stopped: 1
    readonly property int playing: 2
    readonly property int paused: 3

    readonly property bool shown: repeater.count > 0 && _firstActive
    property bool _firstActive: false

    signal used()

    // sends an MPRIS call ("Previous", "PlayPause", "Next") to a player
    property var invoke: function (container, name) {
        if (typeof container[name] === "function")
            container[name]()
    }

    implicitWidth: 360
    implicitHeight: 72
    width: implicitWidth
    height: implicitHeight
    opacity: shown ? 1 : 0
    visible: opacity > 0
    Behavior on opacity {
        NumberAnimation { duration: Motion.normal; easing.type: Motion.enter }
    }

    Repeater {
        id: repeater
        model: card.model

        delegate: Item {
            id: row

            required property var model
            required property int index

            readonly property int status: model.playbackStatus !== undefined ? model.playbackStatus : 0
            readonly property bool active: index === 0
                                           && (status === card.playing || status === card.paused)
            readonly property bool isPlaying: status === card.playing
            readonly property var container: model.container !== undefined ? model.container : null

            function call(name) {
                card.used()
                if (container)
                    card.invoke(container, name)
            }

            visible: active
            anchors.fill: parent

            Binding {
                target: card
                property: "_firstActive"
                value: row.active
                when: row.index === 0
                restoreMode: Binding.RestoreNone
            }

            Rectangle {
                anchors.fill: parent
                radius: RayePalette.flyoutRadius
                color: RayePalette.flyoutFill
                border.width: 1
                border.color: RayePalette.flyoutBorder
            }

            // the cover, or a plain tile with a note when there is none
            Rectangle {
                id: coverFrame
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 48
                height: 48
                radius: RayePalette.controlRadius
                color: RayePalette.nightHigh
                clip: true

                Image {
                    id: cover
                    anchors.fill: parent
                    source: row.model.artUrl ? row.model.artUrl : ""
                    sourceSize: Qt.size(48 * Screen.devicePixelRatio, 48 * Screen.devicePixelRatio)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    visible: status === Image.Ready
                }

                Canvas {
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    visible: cover.status !== Image.Ready
                    onPaint: {
                        var c = getContext("2d")
                        c.reset()
                        c.fillStyle = RayePalette.textSecondary
                        c.strokeStyle = RayePalette.textSecondary
                        c.lineWidth = 2
                        c.beginPath()
                        c.moveTo(8, 15); c.lineTo(8, 3); c.lineTo(17, 1); c.lineTo(17, 13)
                        c.stroke()
                        c.beginPath(); c.arc(5.5, 15.5, 3, 0, 2 * Math.PI); c.fill()
                        c.beginPath(); c.arc(14.5, 13.5, 3, 0, 2 * Math.PI); c.fill()
                    }
                }
            }

            Column {
                anchors.left: coverFrame.right
                anchors.leftMargin: 12
                anchors.right: buttons.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    objectName: "mediaTitle"
                    width: parent.width
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    text: row.model.track ? row.model.track : Hints.text("noTitle", card.lang)
                    color: RayePalette.text
                    font.family: RayePalette.family
                    font.weight: Font.DemiBold
                    font.pixelSize: Math.round(14 * RayePalette.textScale)
                }
                Text {
                    objectName: "mediaArtist"
                    width: parent.width
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    // no artist: the player's name
                    text: row.model.artist ? row.model.artist
                                           : (row.model.identity ? row.model.identity : "")
                    visible: text !== ""
                    color: RayePalette.textSecondary
                    font.family: RayePalette.family
                    font.pixelSize: Math.round(12 * RayePalette.textScale)
                }
            }

            Row {
                id: buttons
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0
                enabled: row.model.canControl !== false

                MediaButton {
                    objectName: "previousButton"
                    glyph: "previous"
                    enabled: row.model.canGoPrevious === true
                    accessibleName: Hints.text("previousTrack", card.lang)
                    onClicked: row.call("Previous")
                }
                MediaButton {
                    objectName: "playButton"
                    glyph: row.isPlaying ? "pause" : "play"
                    accessibleName: Hints.text(row.isPlaying ? "pause" : "play", card.lang)
                    onClicked: row.call("PlayPause")
                }
                MediaButton {
                    objectName: "nextButton"
                    glyph: "next"
                    enabled: row.model.canGoNext === true
                    accessibleName: Hints.text("nextTrack", card.lang)
                    onClicked: row.call("Next")
                }
            }
        }
    }

    // a 36 x 36 round button with a drawn 16 px glyph
    component MediaButton: T.AbstractButton {
        id: button

        property string glyph: "play"
        property string accessibleName: ""

        implicitWidth: 36
        implicitHeight: 36
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        opacity: enabled ? 1 : 0.4
        Accessible.role: Accessible.Button
        Accessible.name: accessibleName
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        Keys.onSpacePressed: clicked()

        background: Rectangle {
            radius: RayePalette.controlRadius
            color: button.pressed ? RayePalette.pressFill
                 : button.hovered ? RayePalette.hoverFill : "transparent"
            border.width: button.visualFocus ? RayePalette.focusWidth : 0
            border.color: RayePalette.focus
        }

        contentItem: Canvas {
            id: glyphCanvas
            implicitWidth: 16
            implicitHeight: 16
            property string glyph: button.glyph
            property color ink: RayePalette.text
            onGlyphChanged: requestPaint()
            onInkChanged: requestPaint()
            onPaint: {
                var c = getContext("2d")
                c.reset()
                c.fillStyle = ink
                var w = width, h = height
                var cx = w / 2, cy = h / 2
                function tri(x0, dir) {
                    // a 6 px wide triangle pointing right (dir 1) or left (-1)
                    c.beginPath()
                    c.moveTo(x0, cy - 5)
                    c.lineTo(x0 + dir * 7, cy)
                    c.lineTo(x0, cy + 5)
                    c.closePath()
                    c.fill()
                }
                if (glyph === "play") {
                    c.beginPath()
                    c.moveTo(cx - 4, cy - 6)
                    c.lineTo(cx + 6, cy)
                    c.lineTo(cx - 4, cy + 6)
                    c.closePath()
                    c.fill()
                } else if (glyph === "pause") {
                    c.fillRect(cx - 5, cy - 6, 3.5, 12)
                    c.fillRect(cx + 1.5, cy - 6, 3.5, 12)
                } else if (glyph === "next") {
                    tri(cx - 6, 1)
                    tri(cx, 1)
                    c.fillRect(cx + 5, cy - 5, 2, 10)
                } else if (glyph === "previous") {
                    tri(cx + 6, -1)
                    tri(cx, -1)
                    c.fillRect(cx - 7, cy - 5, 2, 10)
                }
            }
        }
    }
}
