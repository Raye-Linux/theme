// Stand-in for Plasma's SessionManagement: the same names, and a record of
// what was asked. Static, so every instance a test makes shares it.
import QtQuick

QtObject {
    enum ConfirmationMode {
        Default = -1,
        Skip = 0,
        ForcePrompt = 1
    }

    property bool canLock: true
    property bool canSwitchUser: true
    property var calls: []

    function requestShutdown(mode) { calls = calls.concat([["shutdown", mode]]) }
    function requestReboot(mode) { calls = calls.concat([["reboot", mode]]) }
    function requestLogout(mode) { calls = calls.concat([["logout", mode]]) }
    function switchUser() { calls = calls.concat([["switchUser"]]) }
}
