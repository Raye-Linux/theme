// org.raye.quicksettings: the network, volume and battery pill of the Raye
// taskbar, and the quick settings panel it opens (click, or Win+A). The
// panel is a window of its own and is made the first time it opens: most
// sessions open it a few times at most.
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.raye.shell

PlasmoidItem {
    id: root

    // the pill is the applet's only face: Plasma never builds a compact
    // representation for an applet that has no full one, so the pill is the
    // full representation and the panel is a window of its own
    preferredRepresentation: fullRepresentation
    activationTogglesExpanded: false
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.status: panelWindow.visible ? PlasmaCore.Types.RequiresAttentionStatus : PlasmaCore.Types.ActiveStatus
    // set up in Raye Settings, never in a Plasma dialog
    Plasmoid.hasConfigurationInterface: false
    toolTipMainText: ""
    toolTipSubText: ""

    readonly property var screenRect: Plasmoid.containment ? Plasmoid.containment.screenGeometry : Qt.rect(0, 0, 1280, 800)

    Backend { id: system }

    readonly property Item panel: panelLoader.item

    function panelTop() {
        const face = root.fullRepresentationItem
        return face && face.Window.window ? face.Window.window.y : screenRect.y + screenRect.height - ShellTokens.taskbarHeight
    }
    function place() {
        if (!panel)
            return
        panelWindow.x = screenRect.x + screenRect.width - panel.width + panel.margin - 12
        panelWindow.y = Math.max(screenRect.y, panelTop() - 12 - frame.height + panel.margin)
    }
    function open() {
        panelLoader.active = true
        panel.shown = true
        place()
        panelWindow.visible = true
        panelWindow.requestActivate()
    }
    function close() {
        if (panelWindow.visible && panel)
            panel.shown = false
    }
    function toggle() {
        if (panelWindow.visible)
            close()
        else
            open()
    }

    GlobalShortcut {
        name: "raye-quick-settings"
        text: i18nd("raye_shell_quicksettings", "Open quick settings")
        sequence: "Meta+A"
        onActivated: root.toggle()
    }
    Connections {
        target: Plasmoid
        function onActivated() { root.toggle() }
    }

    PlasmaCore.Dialog {
        id: panelWindow
        location: PlasmaCore.Types.Floating
        type: PlasmaCore.Dialog.AppletPopup
        backgroundHints: PlasmaCore.Dialog.NoBackground
        hideOnWindowDeactivate: true
        visible: false
        onVisibleChanged: {
            if (!visible && root.panel)
                root.panel.shown = false
        }

        mainItem: PanelFrame {
            id: frame
            panel: panelLoader.item
            focus: true
            // only a new tallest page (edit mode, a player showing up)
            // resizes the window; a page change does not
            onHeightChanged: if (panelWindow.visible) root.place()
            onOutsideClicked: root.close()

            Loader {
                id: panelLoader
                y: frame.height - height
                // without focus the panel never gets the keys of its
                // window, and Esc does not close it
                focus: true
                active: false
                sourceComponent: QuickPanel {
                    backend: system
                    onRequestClose: root.close()
                    onClosed: panelWindow.visible = false
                    onOpenSettings: page => Launcher.openSettings(page)
                    onLaunch: desktopId => Launcher.launch(desktopId)
                }
            }
        }
    }

    // right-click on the pill: the settings behind its three glyphs
    Loader {
        id: pillMenuLoader
        active: false
        sourceComponent: PanelMenu {
            RayeMenuItem {
                text: i18nd("raye_shell_quicksettings", "Network and internet settings")
                icon.name: "network-wired-symbolic"
                onTriggered: Launcher.openSettings("network")
            }
            RayeMenuItem {
                text: i18nd("raye_shell_quicksettings", "Sound settings")
                icon.name: "audio-volume-high-symbolic"
                onTriggered: Launcher.openSettings("system/sound")
            }
            RayeMenuItem {
                text: i18nd("raye_shell_quicksettings", "Power and battery settings")
                icon.name: "battery-080-symbolic"
                onTriggered: Launcher.openSettings("system/power")
            }
        }
    }

    fullRepresentation: Item {
        Layout.minimumWidth: pill.implicitWidth
        Layout.preferredWidth: pill.implicitWidth
        Layout.fillHeight: true
        implicitWidth: pill.implicitWidth
        implicitHeight: pill.implicitHeight

        QuickPill {
            id: pill
            anchors.verticalCenter: parent.verticalCenter
            backend: system
            selected: panelWindow.visible
            onClicked: root.toggle()

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: mouse => {
                    pillMenuLoader.active = true
                    pillMenuLoader.item.open(pill, mouse.x)
                }
            }
        }
    }
}
