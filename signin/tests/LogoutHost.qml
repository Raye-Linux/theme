// What ksmserver-logout-greeter puts in the context of Logout.qml, with
// the values a normal session has. Plain properties here are found by the
// file the Loader makes, exactly as context properties would be.
import QtQuick

Item {
    id: host

    property int sdtype: -1
    property rect screenGeometry: Qt.rect(0, 0, 800, 600)
    property bool maysd: true
    property bool canLogout: true
    property bool softwareUpdatePending: false
    property var spdMethods: ({ SuspendState: true, HibernateState: false })

    property url file: ""
    readonly property alias logout: loader.item

    width: 800
    height: 600

    Loader {
        id: loader
        source: host.file
    }
}
