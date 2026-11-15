import QtQuick
import QtTest
import org.raye.shell
import ".."

// The session pieces of the lock screen: power goes out without a prompt
// (a prompt cannot show over a locked session, so the buttons did
// nothing), with the confirmation mode Plasma means by "skip".
TestCase {
    id: test
    name: "LockAdapter"

    Component {
        id: adapterComponent
        LockAdapter {
            active: false
        }
    }

    function makeWithSessions() {
        var a = createTemporaryObject(adapterComponent, test)
        verify(a)
        // the real source, against the stand-in module (tests/fakes)
        a.sessions = Qt.createQmlObject(a.sessionsSource, a, "tst_lockadapter")
        verify(a.sessions)
        return a
    }

    function test_shutDownSkipsThePrompt() {
        var a = makeWithSessions()
        compare(a.canShutDown, true)
        a.shutDown()
        compare(a.sessions.calls.length, 1)
        compare(a.sessions.calls[0][0], "shutdown")
        // Skip, not ForcePrompt (1): the value that used to be sent
        compare(a.sessions.calls[0][1], 0)
    }

    function test_restartSkipsThePrompt() {
        var a = makeWithSessions()
        compare(a.canRestart, true)
        a.restart()
        compare(a.sessions.calls.length, 1)
        compare(a.sessions.calls[0][0], "reboot")
        compare(a.sessions.calls[0][1], 0)
    }

    function test_nothingWhenNotAllowed() {
        var a = makeWithSessions()
        a.sessions.canShutdown = false
        a.sessions.canReboot = false
        compare(a.canShutDown, false)
        compare(a.canRestart, false)
        a.shutDown()
        a.restart()
        compare(a.sessions.calls.length, 0)
    }

    function test_noSessionsIsHarmless() {
        var a = createTemporaryObject(adapterComponent, test)
        compare(a.canShutDown, false)
        compare(a.canRestart, false)
        a.shutDown()
        a.restart()
        a.switchUser()
    }

    function makeWithUpdates() {
        var a = createTemporaryObject(adapterComponent, test)
        verify(a)
        UpdateState.calls = []
        UpdateState.restartPending = true
        UpdateState.armsWithoutPassword = true
        a.updates = Qt.createQmlObject(a.updatesSource, a, "tst_lockadapter")
        verify(a.updates)
        return a
    }

    function test_updatesReadyNeedsBoth() {
        var a = makeWithUpdates()
        compare(a.updatesReady, true)
        UpdateState.armsWithoutPassword = false
        compare(a.updatesReady, false)
        UpdateState.armsWithoutPassword = true
        UpdateState.restartPending = false
        compare(a.updatesReady, false)
    }

    function test_armGoesToUpdateState() {
        var a = makeWithUpdates()
        a.armUpdates("restart")
        a.armUpdates("shutdown")
        compare(UpdateState.calls, ["restart", "shutdown"])
    }

    SignalSpy { id: armFailedSpy; signalName: "updateArmFailed" }

    function test_armFailurePassesThrough() {
        var a = makeWithUpdates()
        armFailedSpy.target = a
        armFailedSpy.clear()
        UpdateState.armFailed("cancelled")
        compare(armFailedSpy.count, 1)
        compare(armFailedSpy.signalArguments[0][0], "cancelled")
    }

    function test_noUpdatesPiece() {
        var a = createTemporaryObject(adapterComponent, test)
        compare(a.updatesReady, false)
        armFailedSpy.target = a
        armFailedSpy.clear()
        a.armUpdates("restart")
        compare(armFailedSpy.count, 1)
    }
}
