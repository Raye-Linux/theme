// Every state of the sign-in module on one page.
//
//   qml6 -I <signin dir> gallery.qml                  all states, scaled down
//   qml6 -I <signin dir> gallery.qml -- --lang fr     in French
//   qml6 -I <signin dir> gallery.qml -- --view error  one state, full size
//
// Views: lock signin security typed caps error busy passwordless
// lockscreen power access network keyboard welcome signingOut
// shuttingDown restarting updating firstSignIn. --contrast and --larger turn on high contrast and
// larger text; --reduced turns animations off.
import QtQuick
import QtQuick.Window
import org.raye.signin

Window {
    id: window

    function arg(name, fallback) {
        var a = Qt.application.arguments
        var i = a.indexOf(name)
        return i >= 0 && i + 1 < a.length ? a[i + 1] : fallback
    }
    function flag(name) {
        return Qt.application.arguments.indexOf(name) >= 0
    }

    readonly property string single: arg("--view", "")
    readonly property var views: ["lock", "signin", "security", "typed", "caps", "error", "busy",
                                  "passwordless", "lockscreen", "keyboard", "welcome",
                                  "signingOut", "shuttingDown", "restarting", "updating",
                                  "firstSignIn", "power"]
    property bool ready: false

    visible: true
    visibility: Window.FullScreen
    width: Screen.width
    height: Screen.height
    color: "#000000"
    title: "org.raye.signin gallery"

    Component.onCompleted: {
        RayePalette.assets = Qt.resolvedUrl("../../assets/signin/")
        RayePalette.wallpaperDir = Qt.resolvedUrl("../../wallpaper/Raye/contents/images/")
        RayePalette.eye = Qt.resolvedUrl("../../assets/logo/raye-eye.png")
        Strings.lang = arg("--lang", "")
        RayePalette.highContrast = flag("--contrast")
        RayePalette.largerText = flag("--larger")
        Motion.reducedMotion = flag("--reduced")
        ready = true
    }

    Loader {
        anchors.fill: parent
        active: window.ready && window.single !== ""
        sourceComponent: Scene {
            view: window.single
            openFlyout: true
        }
    }

    Grid {
        id: grid
        visible: window.ready && window.single === ""
        anchors.fill: parent
        anchors.margins: 8
        columns: 4
        spacing: 8
        readonly property real cellWidth: (width - spacing * (columns - 1)) / columns
        readonly property real cellHeight: cellWidth * 1080 / 1920

        Repeater {
            model: grid.visible ? window.views : []
            delegate: Item {
                id: cell
                required property string modelData
                width: grid.cellWidth
                height: grid.cellHeight + 18
                Item {
                    width: grid.cellWidth
                    height: grid.cellHeight
                    clip: true
                    Scene {
                        width: 1920
                        height: 1080
                        scale: grid.cellWidth / 1920
                        transformOrigin: Item.TopLeft
                        view: cell.modelData
                    }
                }
                Text {
                    y: grid.cellHeight + 2
                    text: cell.modelData
                    color: "#b5b3bc"
                    font.pixelSize: 12
                }
            }
        }
    }
}
