// The picture behind the windows, one per screen, on the background layer.
import QtQuick
import QtQuick.Window
import org.kde.layershell as LayerShell
import org.raye.hypr

Window {
    id: wall

    width: screen ? screen.width : 1280
    height: screen ? screen.height : 800
    color: "#0e0a17"
    flags: Qt.FramelessWindowHint
    visible: true

    LayerShell.Window.scope: "raye-wallpaper"
    LayerShell.Window.layer: LayerShell.Window.LayerBackground
    LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorBottom
                               | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
    // under the taskbar too
    LayerShell.Window.exclusionZone: -1
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityNone

    Image {
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        sourceSize: Qt.size(wall.width * Screen.devicePixelRatio, wall.height * Screen.devicePixelRatio)
        source: Views.wallpaper(Qt.size(wall.width * Screen.devicePixelRatio, wall.height * Screen.devicePixelRatio))
    }
}
