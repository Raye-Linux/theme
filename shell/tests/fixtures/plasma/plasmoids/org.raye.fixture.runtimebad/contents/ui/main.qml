// Fixture: compiles, but reads a name that does not exist once it runs.
import QtQuick
import org.kde.plasma.plasmoid

PlasmoidItem {
    property string label: missingThing.name

    preferredRepresentation: fullRepresentation
    fullRepresentation: Text {
        text: i18nd("raye_shell_fixture", "Load test passed")
    }
}
