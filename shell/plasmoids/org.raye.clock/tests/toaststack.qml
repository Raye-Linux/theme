// For tst_toaststack: plain windows in place of the toasts, newest first
// as in the notification model, stacked by ToastStack.qml and moved by
// ToastMover.qml the way the real toast windows are.
import QtQuick
import QtQuick.Window

import "../contents/ui/toasts"

Item {
    id: root

    property rect area: Qt.rect(0, 0, 1366, 768)
    property int panelTop: 720
    property bool motion: true
    property alias rows: rows
    property alias windows: windows
    property alias stack: stack

    function windowAt(i) {
        return windows.objectAt(i)
    }
    function add(name, h) {
        rows.insert(0, { name: name, h: h })
    }
    function removeRow(row) {
        rows.remove(row)
    }

    ListModel { id: rows }

    Instantiator {
        id: windows
        model: rows
        delegate: Window {
            id: win
            required property string name
            required property int h
            readonly property ToastMover mover: ToastMover {
                target: win
                Component.onCompleted: if (!root.motion) duration = 0
            }
            function moveTo(x, y) {
                mover.moveTo(x, y)
            }
            objectName: name
            width: 380
            height: h
            flags: Qt.FramelessWindowHint
            visible: false
        }
    }

    ToastStack {
        id: stack
        windows: windows
        area: root.area
        panelTop: root.panelTop
    }
}
