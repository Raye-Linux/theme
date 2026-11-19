// Quick settings: the Raye panel (network, volume, brightness, battery and
// the toggles) of the quick settings applet, bottom right above the clock.
import QtQuick
import org.raye.hypr
import org.raye.shell

Flyout {
    id: popup

    property var system
    layerName: "raye-quick"
    content: frame.item
    readonly property bool open: visible

    onOutsideClicked: close()

    Loader {
        id: frame
        focus: true
        source: Views.url("org.raye.quicksettings", "PanelFrame.qml")
        x: popup.width - width - 12 + (frame.item && frame.item.panel ? frame.item.panel.margin || 0 : 0)
        y: popup.height - height - ShellTokens.flyoutGap + (frame.item && frame.item.panel ? frame.item.panel.margin || 0 : 0)
        onLoaded: item.outsideClicked.connect(popup.close)
    }
    Loader {
        id: panelLoader
        active: false
        parent: frame.item
        focus: true
        source: Views.url("org.raye.quicksettings", "QuickPanel.qml")
        y: frame.item ? frame.item.height - height : 0
        onLoaded: {
            item.backend = Qt.binding(() => popup.system)
            item.requestClose.connect(popup.close)
            item.closed.connect(() => popup.visible = false)
            item.openSettings.connect(page => Launcher.openSettings(page))
            item.launch.connect(desktopId => Launcher.launch(desktopId))
            frame.item.panel = item
        }
    }

    function show() {
        panelLoader.active = true
        if (!panelLoader.item)
            return
        panelLoader.item.shown = true
        visible = true
        requestActivate()
    }
    function close() {
        if (visible && panelLoader.item)
            panelLoader.item.shown = false
    }
    function toggle() {
        if (visible)
            close()
        else
            show()
    }
}
