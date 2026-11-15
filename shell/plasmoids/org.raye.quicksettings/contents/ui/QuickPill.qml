// The button on the taskbar: network, volume and battery as one rounded
// pill, one hover target. Click opens quick settings, the wheel changes
// the volume, like Windows.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.shell

import "quicklogic.js" as Logic

PanelButton {
    id: pill

    property QtObject backend

    readonly property string volumeIcon: Logic.volumeIcon(backend ? backend.volume : 0, backend ? backend.muted : true)
    readonly property string batteryIcon: backend ? Logic.batteryIcon(backend.batteryPercent, backend.charging) : ""

    implicitWidth: glyphs.implicitWidth + 2 * 8
    implicitHeight: ShellTokens.buttonSize
    // no tooltip over the open panel
    toolTipText: backend && !selected ? Logic.joinLines([
        backend.networkText,
        backend.volumeAvailable ? (backend.muted ? i18nd("raye_shell_quicksettings", "Sound off")
                                                 : i18nd("raye_shell_quicksettings", "Volume: %1%", backend.volume)) : "",
        backend.hasBattery ? i18nd("raye_shell_quicksettings", "Battery: %1%", backend.batteryPercent) : "",
    ]) : ""
    Accessible.name: i18nd("raye_shell_quicksettings", "Quick settings")
    Accessible.description: toolTipText

    Row {
        id: glyphs
        anchors.centerIn: parent
        spacing: 8

        Kirigami.Icon {
            objectName: "networkGlyph"
            width: 16
            height: 16
            source: pill.backend ? pill.backend.networkIcon : ""
            color: ShellTokens.taskbarText
            isMask: true
        }
        Kirigami.Icon {
            objectName: "volumeGlyph"
            width: 16
            height: 16
            visible: pill.backend && pill.backend.volumeAvailable
            source: pill.volumeIcon
            color: ShellTokens.taskbarText
            isMask: true
        }
        Kirigami.Icon {
            objectName: "batteryGlyph"
            width: 16
            height: 16
            visible: pill.backend && pill.backend.hasBattery
            source: pill.batteryIcon
            color: ShellTokens.taskbarText
            isMask: true
        }
    }

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            if (pill.backend && pill.backend.volumeAvailable)
                pill.backend.setVolume(Logic.wheelVolume(pill.backend.volume, event.angleDelta.y))
        }
    }
}
