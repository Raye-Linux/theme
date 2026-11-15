// org.raye.devices: the USB icon of the tray, as in Windows. It shows
// while a removable drive is plugged in; a click opens a short menu,
// "Eject iodd" for each drive and "Open in Files" for what is mounted.
// The work (Solid, udisks, the toasts) is in devicesapplet.cpp.
import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.raye.shell

PlasmoidItem {
    id: root

    Plasmoid.status: Plasmoid.hasDevices ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus
    Plasmoid.hasConfigurationInterface: false
    Plasmoid.icon: "drive-removable-media"
    toolTipMainText: i18nd("raye_shell_devices", "Safely Remove Hardware and Eject Media")
    toolTipSubText: ""
    preferredRepresentation: compactRepresentation

    // the tray opens an applet by expanding it: the menu shows instead
    fullRepresentation: Item {}
    onExpandedChanged: {
        if (expanded) {
            expanded = false
            openMenu()
        }
    }

    function openMenu() {
        const anchor = root.compactRepresentationItem
        if (!anchor)
            return
        // made the first time it is needed
        menuLoader.active = true
        const menu = menuLoader.item
        menu.clear()
        const rows = Plasmoid.rows
        if (rows.length === 0) {
            menu.addItem(emptyRow.createObject(null))
        }
        for (let i = 0; i < rows.length; ++i) {
            const r = rows[i]
            if (r.kind === "separator") {
                menu.addItem(separatorRow.createObject(null))
                continue
            }
            const item = actionRow.createObject(null, {
                text: r.text,
                shortcutText: r.detail,
                "icon.name": r.kind === "eject" ? "media-eject" : "folder-open"
            })
            const udi = r.udi
            if (r.kind === "eject")
                item.triggered.connect(() => Plasmoid.eject(udi))
            else
                item.triggered.connect(() => Plasmoid.open(udi))
            menu.addItem(item)
        }
        menu.open(anchor, anchor.width / 2)
    }

    compactRepresentation: Kirigami.Icon {
        source: "drive-removable-media"
        active: mouse.containsMouse
        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: root.openMenu()
        }
    }

    Loader {
        id: menuLoader
        active: false
        sourceComponent: PanelMenu {}
    }

    Component { id: actionRow; RayeMenuItem {} }
    Component { id: separatorRow; RayeMenuSeparator {} }
    Component {
        id: emptyRow
        RayeMenuItem {
            text: i18nd("raye_shell_devices", "No devices to eject")
            enabled: false
        }
    }
}
