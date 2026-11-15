// A window holding the panel on the stand-in for the system services.
// The test changes the stand-in and reads what the panel asked for.
import QtQuick
import QtQuick.Window

import "../contents/ui"

Window {
    id: fixture

    property alias backend: fake
    property Item panel: quick
    property list<string> opened
    property list<string> launched

    width: quick.width
    height: quick.height
    color: "#140f20"

    FakeBackend { id: fake }

    QuickPanel {
        id: quick
        backend: fake
        onOpenSettings: page => fixture.opened = Array.from(fixture.opened).concat([page])
        onLaunch: id => fixture.launched = Array.from(fixture.launched).concat([id])
    }
}
