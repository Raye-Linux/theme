// A window holding the panel on the stand-in for the screens.
import QtQuick
import QtQuick.Window

import "../qml"

Window {
    id: fixture

    property alias controller: fake
    property Item panel: quick
    property int closes: 0
    property int settingsOpened: 0

    width: quick.width
    height: quick.height
    color: "#140f20"

    FakeController { id: fake }

    ProjectPanel {
        id: quick
        controller: fake
        onRequestClose: fixture.closes++
        onOpenDisplaySettings: fixture.settingsOpened++
    }
}
