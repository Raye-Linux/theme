// A window holding the taskbar's row on a stand-in for Plasma's tasks
// model. The test adds tasks with add(), changes options with setOption()
// and reads what the row asked for in `calls`.
import QtQuick
import QtQuick.Window

Window {
    id: fixture

    property url uiDir
    property int barWidth: 1000
    property Item bar: loader.item
    property var calls: []
    // the night taskbar, so pictures taken from here look like the real one
    color: "#140f20"
    width: barWidth
    height: 48

    function add(task) {
        tasks.append(task)
        controller.revision++
    }
    function setOption(key, value) {
        settings[key] = value
    }
    function setBadge(appKey, count) {
        const b = Object.assign({}, loader.item.badgeCounts)
        b[appKey] = count
        loader.item.badgeCounts = b
    }
    function button(index) {
        return loader.item.buttonAt(index)
    }

    ListModel { id: tasks }

    QtObject {
        id: settings
        property string alignment: "center"
        property string combine: "always"
        property string smallButtons: "never"
        property bool badges: true
        property bool flashing: true
    }

    QtObject {
        id: controller
        property int revision: 0
        function record(what) {
            fixture.calls = fixture.calls.concat([what])
        }
        function counts() {
            const apps = {}
            let grouped = 0, ungrouped = 0
            for (let i = 0; i < tasks.count; ++i) {
                const t = tasks.get(i)
                if (!apps[t.AppId]) { apps[t.AppId] = true; ++grouped }
                ungrouped += Math.max(1, t.ChildCount)
            }
            return { grouped: grouped, ungrouped: ungrouped }
        }
        function launch(i) { record("launch " + i) }
        function newInstance(i) { record("newInstance " + i) }
        function activate(i) { record("activate " + i) }
        function minimize(i) { record("minimize " + i) }
        function cycle(i) { record("cycle " + i) }
        function close(i) { record("close " + i) }
        function openUrls(i, urls) { record("openUrls " + i) }
        function desktopId(i) { return "" }
        function windowsOf(i) { return [] }
        function activateWindow(i, child) { record("activateWindow " + i + " " + child) }
        function closeWindow(i, child) { record("closeWindow " + i + " " + child) }
        function endTask(i) { record("endTask " + i) }
        function move(from, to) { record("move " + from + " " + to); tasks.move(from, to, 1) }
        function isPinned(i) { return tasks.get(i).IsLauncher }
        function togglePin(i) { record("togglePin " + i) }
    }

    Loader {
        id: loader
        width: fixture.width
        height: fixture.height
        source: fixture.uiDir + "TaskBar.qml"
        onLoaded: {
            item.settings = settings
            item.controller = controller
            item.model = tasks
        }
    }
}
