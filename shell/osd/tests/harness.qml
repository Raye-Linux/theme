// The on-screen display over the wallpaper and a stand-in taskbar, to look
// at under Xvfb (not a test). It shows OsdItem inside this window, where
// Osd.qml would put its own window: Xvfb has no compositor, so a window
// of its own would be drawn on black. Put a copy of the module (our three files,
// the stock plugin) and the stand-ins on the import path:
//
//   QML_IMPORT_PATH=<module copy>:shell/osd/tests/mock:<build>/qml:theme/qml \
//       qml6 harness.qml -- volume|media|brightness|caps|layout|boost
import QtQuick
import QtQuick.Window
import org.kde.plasma.workspace.osd

Window {
    id: root
    visible: true
    x: 0
    y: 0
    width: Screen.width
    height: Screen.height
    flags: Qt.FramelessWindowHint
    color: "black"

    readonly property string mode: {
        const args = Qt.application.arguments
        return args[args.length - 1]
    }

    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl("../../../wallpaper/Raye/contents/images/1920x1080.png")
        fillMode: Image.PreserveAspectCrop
    }
    // the taskbar's place
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 48
        color: "#d9140f20"
        Rectangle { width: parent.width; height: 1; color: "#17ffffff" }
    }

    // a player with a track, for the media card
    QtObject {
        id: player
        property string track: "Aurora over the lake"
        property string artist: "Northern Lights"
        property string identity: "Music"
        property string iconName: "multimedia-audio-player"
        property url artUrl
        property int playbackStatus: 2
        property bool canGoNext: true
        property bool canGoPrevious: true
        property bool canPlay: true
        property bool canPause: true
    }

    OsdItem {
        id: osd
        x: windowX - Screen.virtualX
        y: windowY - Screen.virtualY
        shownWindow: true
        Component.onCompleted: {
            const m = root.mode
            if (m === "caps") {
                set("input-caps-on", Qt.locale().name.startsWith("fr") ? "Verrouillage majuscules activé" : "Caps Lock On", false)
            } else if (m === "layout") {
                set("input-keyboard", "French (Canada)", false)
            } else if (m === "brightness") {
                set("video-display-brightness", 70, true, 100, "Built-in display")
            } else if (m === "boost") {
                set("audio-volume-high", 120, true, 150)
            } else {
                set("audio-volume-medium", 46, true)
            }
            if (m === "media") {
                for (const child of osd.data) {
                    if (child.currentPlayer !== undefined) {
                        child.currentPlayer = player
                    }
                }
            }
        }
        function set(iconName, value, progress, max, more) {
            // stay on screen while the picture is taken
            timeout = 600000
            icon = iconName
            osdValue = value
            osdMaxValue = max ?? 100
            osdAdditionalText = more ?? ""
            showingProgress = progress
            show()
        }
    }
}
