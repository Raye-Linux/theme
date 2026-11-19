// The part of Shell.qml titlebar-preview.sh needs: a title bar's Minimize,
// through the shell's bus and logic, to Hyprland. Loaded with
// raye-hypr-shell --qml, without the taskbar.
import QtQuick
import org.raye.hypr
import "qrc:/qt/qml/org/raye/hypr/hyprlogic.js" as Logic

Item {
    Connections {
        target: ShellBus
        function onMinimize(pid, title) {
            const w = Logic.windowOf(Hyprland.clients, pid, title, Hyprland.activeAddress)
            console.log("minimize", pid, title, w ? w.address : "none")
            if (w)
                Hyprland.request(Logic.batch(Logic.minimizeCommands(w)))
        }
    }
    Component.onCompleted: Hyprland.refresh()
}
