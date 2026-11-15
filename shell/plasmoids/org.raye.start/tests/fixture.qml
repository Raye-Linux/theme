// A window holding either the row of taskbar buttons ("row") or the
// popup frame ("frame") of the Start applet, with stand-in options. The
// test changes options with setOption() and reads what the row asked
// for in `calls`. "quicklinks": the quick link menu, over a stand-in button.
import QtQuick
import QtQuick.Window

Window {
    id: fixture

    property url uiDir
    property string what: "row"
    property Item item: loader.item
    property var calls: []
    // the frame gets a stand-in backend when the test asks for one
    property bool withBackend: false
    // the frame opens as soon as it is made; false leaves it closed
    property bool autoOpen: true
    property StandInBackend backend: StandInBackend {}
    color: "#140f20"
    width: what === "frame" ? 700 : 1000
    height: what === "frame" ? 760 : 48

    // what the quick link menu does lands in `calls`
    property QtObject linkActions: QtObject {
        function launch(id) { fixture.record("launch " + id) }
        function settings(page) { fixture.record("settings " + page) }
        function search() { fixture.record("search") }
        function desktop() { fixture.record("desktop") }
        function sessions() { return ({ canLogout: true, canSuspend: true, canShutdown: true, canReboot: true }) }
        function power(action) { fixture.record("power " + action) }
    }
    property Item linkButtonItem: linkButton
    Item {
        id: linkButton
        y: fixture.height - 40
        x: 200
        width: 40
        height: 40
    }
    function openLinks() {
        loader.item.open(linkButton)
    }

    function setOption(key, value) {
        if (key === "leadingSpace")
            loader.item.leadingSpace = value
        else
            settings[key] = value
    }
    function record(what) {
        fixture.calls = fixture.calls.concat([what])
    }

    QtObject {
        id: settings
        property string searchMode: "box"
        property bool taskView: true
        property string alignment: "center"
    }

    Loader {
        id: loader
        focus: true
        source: fixture.uiDir + (fixture.what === "frame" ? "StartFrame.qml"
                                : fixture.what === "quicklinks" ? "QuickLinkMenu.qml" : "StartRow.qml")
        onLoaded: {
            if (fixture.what === "quicklinks") {
                item.actions = fixture.linkActions
                // everything but the system log
                item.appInstalled = id => id !== "org.kde.ksystemlog.desktop"
            } else if (fixture.what === "frame") {
                item.screenWidth = 1280
                item.screenHeight = 800
                if (fixture.withBackend)
                    item.backend = fixture.backend
                if (fixture.autoOpen) {
                    item.reset("start", "")
                    item.present()
                }
            } else {
                item.settings = settings
                item.height = 48
                item.startClicked.connect(() => fixture.record("start"))
                item.searchClicked.connect(() => fixture.record("search"))
                item.taskViewClicked.connect(() => fixture.record("taskView"))
                item.quickLinksRequested.connect(() => fixture.record("quickLinks"))
            }
        }
    }
}
