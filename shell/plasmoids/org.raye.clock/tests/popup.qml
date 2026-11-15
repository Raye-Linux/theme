// The notification centre in its own window, placed by CentrePlacement as
// the applet does, on stand-ins for the notification model and the
// controller. The time is fixed: Sunday 1 November 2026, 14:05:09.
import QtQuick
import QtQuick.Window

Window {
    id: popup

    property url uiDir
    property int notificationCount: 5
    readonly property Item centre: loader.item
    readonly property date now: new Date(2026, 10, 1, 14, 5, 9)
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint

    function open(screenRect, panelTop, area) {
        placement.open(screenRect, panelTop, area)
    }
    function fit(screenRect, panelTop) {
        placement.fit(screenRect, panelTop)
    }

    ListModel { id: notifications }

    QtObject {
        id: controller
        function close(row) {}
        function clearAll() {}
        function invokeDefault(row) {}
        function invokeAction(row, id) {}
        function killJob(row) {}
        function toggleDnd() {}
        function startFocus(minutes) {}
        function stopFocus() {}
        function requestAgenda(from, to) {}
        function openCalendar(day, isNew) {}
    }

    Item {
        id: holder

        Loader {
            id: loader
            source: popup.uiDir + "Centre.qml"
            onLoaded: {
                item.now = popup.now
                for (let i = 0; i < popup.notificationCount; ++i)
                    notifications.append({ isGroup: false, isInGroup: false, type: 1, applicationName: "Spectacle",
                                           applicationIconName: "org.kde.spectacle", summary: "Rectangular Region",
                                           body: "A screenshot was saved to your clipboard.",
                                           created: new Date(popup.now.getTime() - i * 60000),
                                           actionNames: "", actionLabels: "", hasDefaultAction: true })
                item.model = notifications
                item.calendarAvailable = true
                item.calendarEvents = [
                    { title: "Dentist", start: new Date(2026, 10, 1, 9, 30), end: new Date(2026, 10, 1, 10, 30), allDay: false, color: "#2f80ed" },
                ]
                item.controller = controller
            }
        }
    }

    Loader {
        id: placementLoader
        source: popup.uiDir + "CentrePlacement.qml"
        onLoaded: {
            item.window = popup
            item.holder = holder
            item.centre = Qt.binding(() => loader.item)
        }
    }
    readonly property var placement: placementLoader.item
}
