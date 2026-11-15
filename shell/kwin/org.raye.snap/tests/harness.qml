// The snap layouts over the wallpaper, to look at under Xvfb (not a test):
//
//   QML_IMPORT_PATH=theme/qml qml6 harness.qml -- flyout|numbered|picked|bar|narrow
//
// The plate stands in for the Plasma dialog frame KWin draws around them.
import QtQuick
import QtQuick.Window
import org.raye.ui
import "../package/contents/code/snap.js" as Snap
import "../package/contents/ui"

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
        source: Qt.resolvedUrl("../../../../wallpaper/Raye/contents/images/1920x1080.png")
        fillMode: Image.PreserveAspectCrop
    }

    // a window with its title bar, under the flyout
    Rectangle {
        visible: root.mode !== "bar"
        x: root.width / 2 - 520
        y: 90
        width: 760
        height: 480
        radius: 8
        color: "#1b1529"
        border.color: "#17ffffff"
        Rectangle {
            width: parent.width
            height: 32
            radius: 8
            color: "#221b33"
            Text { x: 12; anchors.verticalCenter: parent.verticalCenter; text: "Notes"; color: "#f3eaff"; font: Tokens.caption }
            Row {
                anchors.right: parent.right
                Repeater {
                    model: 3
                    Rectangle {
                        width: 46
                        height: 32
                        color: index === 1 ? "#1affffff" : "transparent"
                        Text { anchors.centerIn: parent; text: ["–", "□", "✕"][index]; color: "#f3eaff" }
                    }
                }
            }
        }
    }

    Rectangle {
        id: plate
        x: root.mode === "bar" ? (root.width - width) / 2 : root.width / 2 - 520 + 760 - 69 - width / 2
        y: root.mode === "bar" ? 8 : 90 + 32 + 8
        width: layouts.width
        height: layouts.height
        radius: Tokens.radiusOverlay
        color: Tokens.acrylicTintSolid
        border.color: Tokens.strokeSurface

        Layouts {
            id: layouts
            layouts: Snap.layoutsFor(root.mode === "narrow" ? 1366 : 1920)
            columns: root.mode === "bar" ? layouts.layouts.length : (layouts.layouts.length > 4 ? 3 : 2)
            numbered: root.mode === "numbered" || root.mode === "picked"
            pickedLayout: root.mode === "picked" ? 3 : -1
            litLayout: root.mode === "bar" || root.mode === "flyout" ? 1 : -1
            litZone: 0
        }
    }
}
