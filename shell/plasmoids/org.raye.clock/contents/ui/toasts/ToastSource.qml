// The words of toastlogic.js in the user's language: who a notification
// is from, and the plain sentence for a network change.
import QtQml

QtObject {
    function nameOf(source) {
        switch (source.key) {
        case "system": return i18nd("raye_shell_clock", "System")
        case "network": return i18nd("raye_shell_clock", "Network")
        case "devices": return i18nd("raye_shell_clock", "Devices")
        case "storage": return i18nd("raye_shell_clock", "Storage")
        case "power": return i18nd("raye_shell_clock", "Power")
        case "bluetooth": return i18nd("raye_shell_clock", "Bluetooth")
        case "accessibility": return i18nd("raye_shell_clock", "Accessibility")
        case "notifications": return i18nd("raye_shell_clock", "Notifications")
        case "passwords": return i18nd("raye_shell_clock", "Passwords")
        case "security": return i18nd("raye_shell_clock", "Security")
        case "files": return i18nd("raye_shell_clock", "Files")
        default: return source.name
        }
    }

    function networkTitle(event) {
        return event.kind === "connected" ? i18nd("raye_shell_clock", "Connected to %1", event.name)
                                          : i18nd("raye_shell_clock", "Disconnected from %1", event.name)
    }
}
