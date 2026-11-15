// Fixture: every way a string can miss its French.
import QtQuick
import org.kde.plasma.plasmoid

PlasmoidItem {
    fullRepresentation: Column {
        Text { text: i18n("Bare string") }
        Text { text: i18nd("plasma_applet_other", "Wrong domain") }
        Text { text: i18nd("raye_shell_fixture", "Never translated") }
        Text { text: i18nd("raye_shell_fixture", "Only a guess") }
        Text { text: i18nd("raye_shell_nopo", "No po file") }
        Text { text: i18nd("raye_shell_fixture", "Load test passed") }
    }
}
