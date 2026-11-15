// Stand-in for org.raye.shell's UpdateState: it records what is asked and
// never runs raye-update.
pragma Singleton
import QtQuick

QtObject {
    property bool restartPending: false
    property bool armsWithoutPassword: false
    property var calls: []
    signal armFailed(string reason)
    signal armSucceeded()
    function updateAndRestart() { calls = calls.concat(["restart"]) }
    function updateAndShutdown() { calls = calls.concat(["shutdown"]) }
}
