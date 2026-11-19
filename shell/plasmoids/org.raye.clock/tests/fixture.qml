// A window holding the clock button ("clock") or the notification centre
// ("centre", "centreEmpty") on stand-ins: a list model shaped like Plasma's
// notification model and a controller that writes down what is asked.
// The time is fixed: Sunday 1 November 2026, 14:05:09.
import QtQuick
import QtQuick.Window

Window {
    id: fixture

    property url uiDir
    property string what: "clock"
    property Item item: loader.item
    property var calls: []
    readonly property date now: new Date(2026, 10, 1, 14, 5, 9)
    color: "#140f20"
    width: what === "clock" ? 200 : 420
    height: what === "clock" ? 48 : Math.max(900, room + 24)
    // for "toast": what the card shows
    property var toast
    // for "centre": that many plain notifications instead of the usual
    // three (-1), the room the centre has (0: its own default) and
    // whether Raye Calendar is there
    property int notificationCount: -1
    property int room: 0
    property bool withCalendar: true

    function record(what) {
        fixture.calls = fixture.calls.concat([what])
    }

    ListModel { id: empty }
    ListModel { id: notifications }

    function fill() {
        const minutes = m => new Date(fixture.now.getTime() - m * 60000)
        notifications.append({ isGroup: true, isInGroup: false, type: 1, applicationName: "Chat", applicationIconName: "org.kde.neochat",
                               summary: "", body: "", created: minutes(5), actionNames: "", actionLabels: "", hasDefaultAction: false })
        notifications.append({ isGroup: false, isInGroup: true, type: 1, applicationName: "Chat", applicationIconName: "org.kde.neochat",
                               summary: "New message", body: "<b>Lunch</b> at noon? &lt;script&gt;", created: minutes(5),
                               actionNames: "reply\nmark", actionLabels: "Reply\nMark as read", hasDefaultAction: true })
        notifications.append({ isGroup: false, isInGroup: false, type: 2, applicationName: "Files", applicationIconName: "org.kde.dolphin",
                               summary: "Copying 3 files", body: "", created: minutes(1), percentage: 45, jobState: 1, killable: true,
                               actionNames: "", actionLabels: "", hasDefaultAction: false })
    }

    QtObject {
        id: controller
        function close(row) { fixture.record("close " + row) }
        function clearAll() { fixture.record("clearAll") }
        function invokeDefault(row) { fixture.record("default " + row) }
        function invokeAction(row, id) { fixture.record("action " + row + " " + id) }
        function killJob(row) { fixture.record("kill " + row) }
        function toggleDnd() { fixture.record("toggleDnd") }
        function startFocus(minutes) { fixture.record("startFocus " + minutes) }
        function stopFocus() { fixture.record("stopFocus") }
        function requestAgenda(from, to) { fixture.record("agenda " + from + " " + to) }
        function openCalendar(day, isNew) { fixture.record("open " + day + (isNew ? " new" : "")) }
        function openClock() { fixture.record("openClock") }
    }

    Loader {
        id: loader
        source: fixture.uiDir + (fixture.what === "clock" ? "ClockButton.qml" : fixture.what === "toast" ? "toasts/ToastCard.qml" : "Centre.qml")
        onLoaded: {
            if (fixture.what === "toast") {
                for (const key in fixture.toast)
                    item[key] = fixture.toast[key]
                item.closeRequested.connect(() => fixture.record("close"))
                item.timeUp.connect(() => fixture.record("timeUp"))
                item.defaultInvoked.connect(() => fixture.record("default"))
                item.actionInvoked.connect(name => fixture.record("action " + name))
                item.replied.connect(text => fixture.record("reply " + text))
                item.menuRequested.connect(button => fixture.record("menu"))
                item.killRequested.connect(() => fixture.record("kill"))
                item.suspendRequested.connect(() => fixture.record("suspend"))
                item.resumeRequested.connect(() => fixture.record("resume"))
                return
            }
            item.now = fixture.now
            if (fixture.what === "clock") {
                item.twentyFour = true
                item.width = Qt.binding(() => item.implicitWidth)
                item.height = 40
                item.clicked.connect(() => fixture.record("clicked"))
            } else {
                if (fixture.room > 0)
                    item.maxHeight = fixture.room
                if (fixture.what === "centre" && fixture.notificationCount >= 0) {
                    for (let i = 0; i < fixture.notificationCount; ++i)
                        notifications.append({ isGroup: false, isInGroup: false, type: 1, applicationName: "Spectacle",
                                               applicationIconName: "org.kde.spectacle", summary: "Rectangular Region",
                                               body: "A screenshot was saved to your clipboard.", created: new Date(fixture.now.getTime() - i * 60000),
                                               actionNames: "", actionLabels: "", hasDefaultAction: true })
                } else if (fixture.what === "centre") {
                    fixture.fill()
                }
                item.model = fixture.what === "centre" ? notifications : empty
                if (fixture.what === "centre" && fixture.withCalendar) {
                    item.calendarAvailable = true
                    item.calendarEvents = [
                        { title: "Dentist", start: new Date(2026, 10, 1, 9, 30), end: new Date(2026, 10, 1, 10, 30), allDay: false, color: "#2f80ed" },
                        { title: "Trip", start: new Date(2026, 9, 31), end: new Date(2026, 10, 3), allDay: true, color: "" },
                    ]
                }
                item.controller = controller
                item.collapseToggled.connect(() => fixture.record("collapse"))
            }
        }
    }
}
