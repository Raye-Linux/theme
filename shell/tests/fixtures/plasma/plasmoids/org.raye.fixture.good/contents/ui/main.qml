// Fixture: the smallest applet that passes every check of the load test.
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    preferredRepresentation: fullRepresentation

    fullRepresentation: Rectangle {
        Layout.minimumWidth: 420
        Layout.minimumHeight: 160
        Layout.preferredWidth: 420
        Layout.preferredHeight: 160
        color: "#1a1028"
        radius: 8
        border.color: "#2e2142"

        Rectangle {
            id: bar
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 16 }
            width: 4
            radius: 2
            color: "#7b3cff"
        }

        Column {
            anchors { left: bar.right; leftMargin: 16; verticalCenter: parent.verticalCenter }
            spacing: 6

            Text {
                text: i18nd("raye_shell_fixture", "Load test passed")
                color: "#f3eaff"
                font { family: "Inter"; pixelSize: 20; weight: Font.DemiBold }
            }
            Text {
                text: i18ndp("raye_shell_fixture", "%1 check ran", "%1 checks ran", Plasmoid.configuration.checks)
                color: "#b9a8d6"
                font { family: "Inter"; pixelSize: 14 }
            }
            Text {
                text: i18ndc("raye_shell_fixture", "button that closes the test", "Close")
                color: "#c040ff"
                font { family: "Inter"; pixelSize: 14 }
            }
        }
    }
}
