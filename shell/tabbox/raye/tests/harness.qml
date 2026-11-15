// The switcher over the wallpaper, to look at under Xvfb (not a test):
//
//   QML_IMPORT_PATH=shell/tabbox/raye/tests/mock:theme/qml qml6 harness.qml -- 12
//
// The number is how many windows; 0 shows "No open windows".
import QtQuick
import QtQuick.Window

Window {
    id: root
    visible: true
    x: 0
    y: 0
    width: Screen.width
    height: Screen.height
    flags: Qt.FramelessWindowHint

    readonly property int windowCount: {
        const args = Qt.application.arguments
        const n = Number(args[args.length - 1])
        return isNaN(n) ? 4 : n
    }
    readonly property var apps: [
        ["Terminal", "utilities-terminal", "1600x900"],
        ["Quarterly report.odt - Writer", "x-office-document", "1920x1080"],
        ["Files - Downloads", "system-file-manager", "1200x800"],
        ["Raye Store", "org.raye.store", "1400x900"],
        ["Notes.txt - Text Editor", "accessories-text-editor", "800x1200"],
        ["Calculator", "accessories-calculator", "400x600"],
        ["Settings", "preferences-system", "1100x760"],
        ["Music", "multimedia-audio-player", "1600x1000"],
        ["Photos - Holidays", "image-viewer", "1920x1200"],
        ["System Monitor", "utilities-system-monitor", "1300x900"],
        ["A very long window title that cannot fit in the card at all", "firefox", "1920x1080"],
        ["Archive", "utilities-file-archiver", "900x700"]
    ]

    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl("../../../../wallpaper/Raye/contents/images/1920x1080.png")
        fillMode: Image.PreserveAspectCrop
    }

    // after this window is up, so the switcher lands on top of it
    Timer {
        interval: 500
        running: true
        onTriggered: windows.source = "windows.qml"
    }

    Loader {
        id: windows
        onLoaded: {
            for (let i = 0; i < root.windowCount; ++i) {
                const a = root.apps[i % root.apps.length]
                item.add({ caption: a[0], icon: a[1], windowId: (i + 1) + "-" + a[2], closeable: true, minimized: false })
            }
            const c = Qt.createComponent(Qt.resolvedUrl("../package/contents/ui/main.qml"))
            if (c.status !== Component.Ready) {
                console.warn(c.errorString())
                return
            }
            c.createObject(root, { model: item, currentIndex: Math.min(1, root.windowCount - 1),
                                   screenGeometry: Qt.rect(0, 0, root.width, root.height), visible: true })
        }
    }
}
