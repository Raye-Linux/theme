// Stand-in for Plasma's SessionManagement in the tests: the same names
// and the same numbers for the confirmation mode as the real one
// (libkworkspace/sessionmanagement.h), and a record of what was asked.
import QtQuick

QtObject {
    enum ConfirmationMode {
        Skip = 0,
        ForcePrompt = 1
    }

    property bool canShutdown: true
    property bool canReboot: true
    property bool canSwitchUser: true
    property var calls: []

    signal aboutToSuspend()

    function requestShutdown(mode) { calls = calls.concat([["shutdown", mode]]) }
    function requestReboot(mode) { calls = calls.concat([["reboot", mode]]) }
    function switchUser() { calls = calls.concat([["switchUser"]]) }
}
