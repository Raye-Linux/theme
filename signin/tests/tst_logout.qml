// The Ctrl+Alt+Del screen and the words that stay up while the session
// ends: what lookandfeel/.../contents/logout/Logout.qml does with what the
// greeter gives it, and what it asks of the session.
import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup
import "../../lookandfeel/org.raye.night.desktop/contents/logout"

TestCase {
    id: test
    name: "Logout"
    width: 800
    height: 600
    when: windowShown
    visible: true

    readonly property url logoutUrl: Qt.resolvedUrl("../../lookandfeel/org.raye.night.desktop/contents/logout/Logout.qml")
    readonly property int skip: 0

    function initTestCase() {
        Setup.assets(RayePalette)
    }

    function cleanup() {
        Strings.lang = "en"
        LogoutState.reset()
    }

    Component {
        id: hostComponent
        LogoutHost {}
    }

    QtObject {
        id: fakeLauncher
        property var calls: []
        property bool taskManagerThere: true
        property bool monitorThere: true
        function launch(id) {
            calls = calls.concat(["launch " + id])
            return id === "org.raye.taskmanager.desktop" ? taskManagerThere : monitorThere
        }
        function openSettings(page) {
            calls = calls.concat(["settings " + page])
            return true
        }
    }

    // Raye Update as Logout.qml makes it from org.raye.shell
    QtObject {
        id: fakeUpdates
        property bool ready: true
        property var calls: []
        signal failed(string reason)
        function arm(then) { calls = calls.concat([then]) }
    }

    SignalSpy { id: cancelSpy; signalName: "cancelRequested" }
    SignalSpy { id: lockSpy; signalName: "lockScreenRequested" }
    SignalSpy { id: suspendSpy; signalName: "suspendRequested" }
    SignalSpy { id: rebootSpy; signalName: "rebootRequested" }
    SignalSpy { id: haltSpy; signalName: "haltRequested" }
    SignalSpy { id: logoutSpy; signalName: "logoutRequested" }
    SignalSpy { id: haltUpdateSpy; signalName: "haltUpdateRequested" }
    SignalSpy { id: rebootUpdateSpy; signalName: "rebootUpdateRequested" }

    readonly property var spies: [cancelSpy, lockSpy, suspendSpy, rebootSpy, haltSpy,
                                  logoutSpy, haltUpdateSpy, rebootUpdateSpy]

    function make(props) {
        var all = { file: logoutUrl }
        for (var k in props)
            all[k] = props[k]
        var host = createTemporaryObject(hostComponent, test, all)
        verify(host)
        tryVerify(() => host.logout !== null, 2000, "Logout.qml did not load")
        var l = host.logout
        for (var i = 0; i < spies.length; ++i) {
            spies[i].clear()
            spies[i].target = l
        }
        l.launcher = fakeLauncher
        fakeLauncher.calls = []
        fakeLauncher.taskManagerThere = true
        fakeLauncher.monitorThere = true
        return l
    }

    function security(l) {
        return findChild(l, "security")
    }

    // --- which screen -----------------------------------------------------

    function test_default_type_is_the_security_screen() {
        var l = make({ sdtype: -1 })
        verify(security(l).visible)
        compare(LogoutState.phase, "")
        wait(800)
        compare(l.sessions.calls.length, 0, "the screen asked for something by itself")
        compare(cancelSpy.count, 0)
    }

    function test_specific_type_ends_the_session_data() {
        return [
            { tag: "shut down en", sdtype: 2, lang: "en", title: "Shutting down", call: "shutdown" },
            { tag: "restart en", sdtype: 1, lang: "en", title: "Restarting", call: "reboot" },
            { tag: "sign out en", sdtype: 0, lang: "en", title: "Signing out", call: "logout" },
            { tag: "old sign out", sdtype: 3, lang: "en", title: "Signing out", call: "logout" },
            { tag: "shut down fr", sdtype: 2, lang: "fr", title: "Arrêt en cours", call: "shutdown" },
            { tag: "restart fr", sdtype: 1, lang: "fr", title: "Redémarrage", call: "reboot" },
            { tag: "sign out fr", sdtype: 0, lang: "fr", title: "Déconnexion", call: "logout" }
        ]
    }

    // no question and no countdown: the words at once, the request after them
    function test_specific_type_ends_the_session(data) {
        Strings.lang = data.lang
        var l = make({ sdtype: data.sdtype })
        verify(!security(l).visible)
        var status = findChild(l, "status")
        verify(status.visible)
        compare(status.mode, LogoutState.phase)
        tryCompare(status, "title", data.title)
        compare(l.sessions.calls.length, 0, "asked before the words were up")
        tryVerify(() => l.sessions.calls.length === 1, 2000)
        compare(l.sessions.calls[0][0], data.call)
        compare(l.sessions.calls[0][1], skip, "the prompt would ask again")
    }

    // The stock signals end the greeter on the spot, and with it the
    // words. The screen has to stay up after the request goes out.
    function test_the_words_outlive_the_request() {
        var l = make({ sdtype: 2 })
        tryVerify(() => l.sessions.calls.length === 1, 2000)
        wait(400)
        compare(haltSpy.count, 0, "the stock signal closes this window")
        compare(cancelSpy.count, 0)
        verify(findChild(l, "status").visible)
    }

    // The same screen also covers the question of an application that
    // refuses to close, so it may not stay for ever.
    function test_the_words_step_aside_in_the_end() {
        var l = make({ sdtype: 2 })
        l.holdLimit = 300
        tryVerify(() => l.sessions.calls.length === 1, 2000)
        tryCompare(cancelSpy, "count", 1, 2000)
    }

    // one window per screen, one request
    function test_two_screens_ask_once() {
        var a = make({ sdtype: 2 })
        var b = createTemporaryObject(hostComponent, test, { file: logoutUrl, sdtype: 2 })
        tryVerify(() => b.logout !== null, 2000)
        tryVerify(() => a.sessions.calls.length + b.logout.sessions.calls.length > 0, 2000)
        wait(300)
        compare(a.sessions.calls.length + b.logout.sessions.calls.length, 1)
    }

    // what one screen chooses, the other shows
    function test_two_screens_follow_each_other() {
        var a = make({ sdtype: -1 })
        var b = createTemporaryObject(hostComponent, test, { file: logoutUrl, sdtype: -1 })
        tryVerify(() => b.logout !== null, 2000)
        security(a).pick("signOut")
        verify(!security(b.logout).visible)
        verify(findChild(b.logout, "status").visible)
    }

    // the session module missing must still end the session
    function test_without_the_session_module_data() {
        return [
            { tag: "halt", sdtype: 2, spy: "haltRequested" },
            { tag: "reboot", sdtype: 1, spy: "rebootRequested" },
            { tag: "logout", sdtype: 0, spy: "logoutRequested" }
        ]
    }

    function test_without_the_session_module(data) {
        var l = make({ sdtype: data.sdtype })
        l.sessions = null
        var spy = createTemporaryObject(spyComponent, test, { target: l, signalName: data.spy })
        tryCompare(spy, "count", 1, 2000)
    }

    Component {
        id: spyComponent
        SignalSpy {}
    }

    // --- the six lines ----------------------------------------------------

    function test_lock() {
        var l = make()
        security(l).pick("lock")
        compare(lockSpy.count, 1)
    }

    function test_switch_user() {
        var l = make()
        security(l).pick("switchUser")
        compare(l.sessions.calls[0][0], "switchUser")
        tryCompare(cancelSpy, "count", 1, 2000)
    }

    function test_sign_out_goes_through_the_words() {
        var l = make()
        security(l).pick("signOut")
        compare(LogoutState.phase, "signingOut")
        compare(l.sessions.calls.length, 0)
        tryVerify(() => l.sessions.calls.length === 1, 2000)
        compare(l.sessions.calls[0][0], "logout")
    }

    function test_cancel_and_escape() {
        var l = make()
        security(l).pick("cancel")
        compare(cancelSpy.count, 1)
        security(l).forceActiveFocus()
        keyClick(Qt.Key_Escape)
        compare(cancelSpy.count, 2)
    }

    function test_password_opens_the_sign_in_page() {
        var l = make()
        verify(security(l).canChangePassword)
        security(l).pick("changePassword")
        compare(fakeLauncher.calls[0], "settings accounts/sign-in")
        tryCompare(cancelSpy, "count", 1, 2000)
    }

    function test_task_manager() {
        var l = make()
        verify(security(l).canTaskManager)
        security(l).pick("taskManager")
        compare(fakeLauncher.calls, ["launch org.raye.taskmanager.desktop"])
        tryCompare(cancelSpy, "count", 1, 2000)
    }

    function test_task_manager_falls_back() {
        var l = make()
        fakeLauncher.taskManagerThere = false
        security(l).pick("taskManager")
        compare(fakeLauncher.calls, ["launch org.raye.taskmanager.desktop",
                                     "launch org.kde.plasma-systemmonitor.desktop"])
        tryCompare(cancelSpy, "count", 1, 2000)
    }

    // nothing to start: the screen stays, and the line says so
    function test_task_manager_absent() {
        var l = make()
        fakeLauncher.taskManagerThere = false
        fakeLauncher.monitorThere = false
        security(l).pick("taskManager")
        wait(600)
        compare(cancelSpy.count, 0)
        verify(!security(l).canTaskManager)
    }

    // no launcher at all (the shell is not installed): both lines refused
    function test_no_launcher() {
        var l = make()
        l.launcher = null
        verify(!security(l).canTaskManager)
        verify(!security(l).canChangePassword)
    }

    // the first line is the highlighted one, once the session has said
    // what it allows
    function test_starts_on_the_first_line() {
        var l = make()
        compare(security(l).current, 0)
    }

    function test_refused_sign_out() {
        var l = make({ canLogout: false })
        verify(!security(l).canSignOut)
        security(l).pick("signOut")
        compare(LogoutState.phase, "")
    }

    // --- the corner ---------------------------------------------------------

    function test_power_rights() {
        var l = make({ maysd: false, spdMethods: ({ SuspendState: false, HibernateState: true }) })
        var s = security(l)
        verify(!s.canShutDown)
        verify(!s.canRestart)
        verify(!s.canSleep)
        verify(s.canHibernate)
    }

    function test_sleep_and_hibernate() {
        var l = make({ spdMethods: ({ SuspendState: true, HibernateState: true }) })
        security(l).sleepRequested()
        compare(suspendSpy.count, 1)
        compare(suspendSpy.signalArguments[0][0], 2)
        security(l).hibernateRequested()
        compare(suspendSpy.signalArguments[1][0], 4)
    }

    function test_power_from_the_corner_data() {
        return [
            { tag: "shut down", fn: "shutDownRequested", phase: "shuttingDown", call: "shutdown" },
            { tag: "restart", fn: "restartRequested", phase: "restarting", call: "reboot" }
        ]
    }

    function test_power_from_the_corner(data) {
        var l = make()
        security(l)[data.fn]()
        compare(LogoutState.phase, data.phase)
        tryVerify(() => l.sessions.calls.length === 1, 2000)
        compare(l.sessions.calls[0][0], data.call)
    }

    function test_update_variants() {
        var l = make({ softwareUpdatePending: true })
        verify(security(l).updatePending)
        security(l).updateAndShutDownRequested()
        compare(LogoutState.phase, "shuttingDown")
        tryCompare(haltUpdateSpy, "count", 1, 2000)
        compare(l.sessions.calls.length, 0)
    }

    function test_update_restart_variant() {
        var l = make({ softwareUpdatePending: true })
        security(l).updateAndRestartRequested()
        compare(LogoutState.phase, "restarting")
        tryCompare(rebootUpdateSpy, "count", 1, 2000)
    }

    function withUpdates(props) {
        var l = make(props)
        fakeUpdates.ready = true
        fakeUpdates.calls = []
        l.updates = fakeUpdates
        return l
    }

    function test_raye_update_and_restart() {
        var l = withUpdates()
        verify(security(l).updatePending)
        security(l).updateAndRestartRequested()
        compare(LogoutState.phase, "restarting")
        tryCompare(fakeUpdates, "calls", ["restart"], 2000)
        compare(rebootUpdateSpy.count, 0)
        compare(l.sessions.calls.length, 0)
    }

    function test_raye_update_and_shut_down() {
        var l = withUpdates()
        security(l).updateAndShutDownRequested()
        compare(LogoutState.phase, "shuttingDown")
        tryCompare(fakeUpdates, "calls", ["shutdown"], 2000)
        compare(haltUpdateSpy.count, 0)
    }

    function test_no_update_row_without_a_ready_set() {
        var l = withUpdates()
        fakeUpdates.ready = false
        verify(!security(l).updatePending)
    }

    function test_cancelled_password_goes_back_to_the_choices() {
        var l = withUpdates()
        security(l).updateAndRestartRequested()
        tryCompare(fakeUpdates, "calls", ["restart"], 2000)
        fakeUpdates.failed("cancelled")
        compare(LogoutState.phase, "")
        verify(security(l).visible)
        wait(50)
        compare(l.sessions.calls.length, 0)
        compare(cancelSpy.count, 0)
    }

    function test_nothing_to_arm_still_restarts() {
        var l = withUpdates()
        security(l).updateAndRestartRequested()
        tryCompare(fakeUpdates, "calls", ["restart"], 2000)
        fakeUpdates.failed("nothing-ready")
        tryVerify(() => l.sessions.calls.length === 1, 2000)
        compare(l.sessions.calls[0][0], "reboot")
    }

    // Ctrl+click on Power: nothing happens before the warning is answered
    function test_emergency_restart_warns_first() {
        var l = make()
        var card = findChild(l, "emergency")
        verify(!card.visible)
        security(l).emergencyRestartRequested()
        verify(card.visible)
        compare(rebootSpy.count, 0)
        mouseClick(findChild(card, "emergencyCancel"))
        verify(!card.visible)
        compare(rebootSpy.count, 0)
        security(l).emergencyRestartRequested()
        mouseClick(findChild(card, "emergencyOk"))
        compare(rebootSpy.count, 1)
    }

    function test_emergency_escape_only_closes_the_warning() {
        var l = make()
        security(l).emergencyRestartRequested()
        var card = findChild(l, "emergency")
        card.forceActiveFocus()
        keyClick(Qt.Key_Escape)
        verify(!card.visible)
        compare(cancelSpy.count, 0)
        compare(rebootSpy.count, 0)
    }
}
