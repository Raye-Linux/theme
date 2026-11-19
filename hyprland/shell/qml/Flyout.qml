// A window of the shell above everything but the taskbar: Start, quick
// settings, the notification centre. It covers the whole screen above the
// taskbar (Hyprland keeps it off the taskbar's reserved edge), its empty
// part is transparent, and a click there closes it, the way a click
// outside a flyout does on Windows.
import QtQuick
import QtQuick.Window
import org.kde.layershell as LayerShell

Window {
    id: flyout

    property string layerName: "raye-start"
    // a click outside `content`
    signal outsideClicked()
    property Item content: null

    width: screen ? screen.width : 1280
    height: screen ? screen.height : 800
    color: "transparent"
    flags: Qt.FramelessWindowHint
    visible: false
    // declared inside the shell's Item: without this Qt waits for that
    // item's window, which never shows, before showing this one
    transientParent: null

    LayerShell.Window.scope: layerName
    LayerShell.Window.layer: LayerShell.Window.LayerTop
    LayerShell.Window.anchors: LayerShell.Window.AnchorTop | LayerShell.Window.AnchorBottom
                               | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight
    LayerShell.Window.exclusionZone: 0
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityExclusive
    LayerShell.Window.wantsToBeOnActiveScreen: true

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onPressed: mouse => {
            const c = flyout.content
            if (!c || !c.contains(c.mapFromItem(this, mouse.x, mouse.y)))
                flyout.outsideClicked()
            else
                mouse.accepted = false
        }
    }
}
