// The window of one toast: no frame, no focus unless a reply is being
// typed. Toasts.qml places it and hands the card its notification.
import QtQuick
import org.kde.plasma.core as PlasmaCore

PlasmaCore.Dialog {
    id: window

    default property alias card: holder.data
    property bool critical: false
    property bool takeFocus: false
    readonly property ToastMover mover: ToastMover { target: window }
    onTakeFocusChanged: if (takeFocus) window.requestActivate()
    // KWin takes a new position with the next frame the window draws, and a
    // toast that only moves draws none: it would stay where it was. A frame
    // with nothing changed is skipped, so a dot nobody can see changes. A
    // place sent while a frame was already under way can still be missed
    // (seen with animations off, several moves in a row): once things are
    // still, the place is sent once more.
    onXChanged: moved()
    onYChanged: moved()
    property bool resending: false
    function moved() {
        dot.flip = !dot.flip
        update()
        if (!resending)
            settle.restart()
    }
    readonly property Timer settle: Timer {
        interval: 100
        onTriggered: {
            window.resending = true
            const y = window.y
            window.y = y + 1
            window.y = y
            window.resending = false
        }
    }

    location: PlasmaCore.Types.Floating
    type: critical ? PlasmaCore.Dialog.CriticalNotification : PlasmaCore.Dialog.Notification
    backgroundHints: PlasmaCore.Dialog.NoBackground
    hideOnWindowDeactivate: false
    flags: takeFocus ? Qt.FramelessWindowHint : Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    visible: false

    // ToastStack.qml calls this; the slide is the mover's
    function moveTo(x, y) {
        mover.moveTo(x, y)
    }

    mainItem: Item {
        id: holder
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
        width: implicitWidth
        height: implicitHeight

        Rectangle {
            id: dot
            property bool flip: false
            width: 1
            height: 1
            color: Qt.rgba(0, 0, 0, flip ? 1 / 255 : 0)
        }
    }
}
