// The security screen: what Ctrl+Alt+Del shows. Six options, the corner
// bar, and -- the point of most of these checks -- an option that is not
// available must be visibly unavailable AND unable to act.
//
// "Change a password" is the one dangerous option on this screen: it
// touches authentication, from a screen reachable without unlocking
// anything. The repo has no safe path for it, so it ships disabled with
// a reason, and these checks require that both the mouse and the
// keyboard refuse it. A button that looks disabled and still fires is
// the whole defect.
import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

TestCase {
    id: test
    name: "SecurityScreen"
    // the offscreen platform's screen is 800 x 600: clicks below it are lost
    width: 800
    height: 600
    when: windowShown
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
        Strings.lang = "en"
    }

    function cleanup() {
        Strings.lang = "en"
    }

    Component {
        id: screenComponent
        SecurityScreen {
            width: 800
            height: 600
            userName: "Alex Martin"
        }
    }

    SignalSpy { id: lockSpy; signalName: "lockRequested" }
    SignalSpy { id: switchSpy; signalName: "switchUserRequested" }
    SignalSpy { id: signOutSpy; signalName: "signOutRequested" }
    SignalSpy { id: passwordSpy; signalName: "changePasswordRequested" }
    SignalSpy { id: taskSpy; signalName: "taskManagerRequested" }
    SignalSpy { id: cancelSpy; signalName: "cancelled" }

    readonly property var spies: [lockSpy, switchSpy, signOutSpy, passwordSpy, taskSpy, cancelSpy]

    function make(props) {
        var s = createTemporaryObject(screenComponent, test, props || {})
        verify(s)
        for (var i = 0; i < spies.length; ++i) {
            spies[i].clear()
            spies[i].target = s
        }
        // without the focus the key checks below pass with no key ever
        // reaching the screen -- a negative check that cannot fail
        s.forceActiveFocus()
        return s
    }

    function row(s, id) {
        return findChild(s, "row-" + id)
    }

    function ids(s) {
        var out = []
        for (var i = 0; i < s.rows.length; ++i)
            out.push(s.rows[i].id)
        return out
    }

    // --- the options Windows has, in the order Windows has them -------

    function test_the_six_options_are_there_in_order() {
        var s = make()
        compare(ids(s), ["lock", "switchUser", "signOut", "changePassword",
                         "taskManager", "cancel"])
    }

    function test_the_words_are_the_ones_of_the_design_data() {
        return [
            { tag: "lock/en", lang: "en", id: "lock", text: "Lock" },
            { tag: "lock/fr", lang: "fr", id: "lock", text: "Verrouiller" },
            { tag: "switchUser/en", lang: "en", id: "switchUser", text: "Switch user" },
            { tag: "switchUser/fr", lang: "fr", id: "switchUser", text: "Changer d'utilisateur" },
            { tag: "signOut/en", lang: "en", id: "signOut", text: "Sign out" },
            { tag: "signOut/fr", lang: "fr", id: "signOut", text: "Se déconnecter" },
            { tag: "changePassword/en", lang: "en", id: "changePassword", text: "Change a password" },
            { tag: "changePassword/fr", lang: "fr", id: "changePassword", text: "Modifier un mot de passe" },
            { tag: "taskManager/en", lang: "en", id: "taskManager", text: "Task Manager" },
            { tag: "taskManager/fr", lang: "fr", id: "taskManager", text: "Gestionnaire des tâches" },
            { tag: "cancel/en", lang: "en", id: "cancel", text: "Cancel" },
            { tag: "cancel/fr", lang: "fr", id: "cancel", text: "Annuler" }
        ]
    }

    function test_the_words_are_the_ones_of_the_design(data) {
        Strings.lang = data.lang
        var s = make()
        var r = row(s, data.id)
        verify(r, data.id + " has no row")
        compare(findChild(r, "label").text, data.text)
    }

    function test_the_corner_bar_is_there() {
        var s = make()
        var bar = findChild(s, "cornerBar")
        verify(bar, "no corner bar: no network, accessibility or power")
        verify(findChild(bar, "powerButton").visible)
        verify(findChild(bar, "accessButton").visible)
        s.networkIcon = "network-wired"
        verify(findChild(bar, "networkButton").visible)
        // the hidden emergency restart of the security screen
        verify(bar.emergencyRestart, "Ctrl+click on Power is not armed")
    }

    // --- the dangerous one -------------------------------------------

    function test_change_a_password_is_refused_by_default() {
        var s = make()
        compare(s.canChangePassword, false,
                "'Change a password' must not be wired to anything by default")
        var r = row(s, "changePassword")
        verify(!r.enabled, "it is shown as available")
        verify(s.changePasswordReason !== "",
               "disabled with no reason: the owner is left guessing")
        compare(findChild(s, "reason-changePassword").text, s.changePasswordReason)
    }

    function test_a_refused_option_cannot_act_by_mouse() {
        var s = make()
        mouseClick(row(s, "changePassword"))
        compare(passwordSpy.count, 0,
                "a disabled 'Change a password' fired anyway -- a button that"
                + " looks disabled and still acts is the whole defect")
    }

    function test_a_refused_option_cannot_act_by_keyboard() {
        var s = make()
        // reach for it by name rather than by counting key presses
        s.pick("changePassword")
        compare(passwordSpy.count, 0, "pick() ignored the refusal")
        // prove Enter works at all first, or the check after it is empty
        s.current = s.indexOf("lock")
        keyClick(Qt.Key_Return)
        compare(lockSpy.count, 1,
                "Enter reaches nothing, so the refusal below proves nothing")
        s.current = s.indexOf("changePassword")
        keyClick(Qt.Key_Return)
        compare(passwordSpy.count, 0, "Enter on a disabled row fired it")
    }

    function test_the_keyboard_walks_past_what_is_refused() {
        var s = make({ canTaskManager: false })
        s.current = s.indexOf("signOut")
        s.move(1)
        // changePassword and taskManager are both refused here
        compare(s.rows[s.current].id, "cancel")
        s.move(-1)
        compare(s.rows[s.current].id, "signOut")
    }

    // --- Task Manager: the app may not be installed -------------------

    function test_task_manager_is_refused_when_it_is_not_installed() {
        var s = make({ canTaskManager: false })
        var r = row(s, "taskManager")
        verify(!r.enabled)
        verify(s.taskManagerReason !== "", "no reason given")
        mouseClick(r)
        compare(taskSpy.count, 0)
    }

    function test_task_manager_acts_when_it_is_there() {
        var s = make({ canTaskManager: true })
        verify(row(s, "taskManager").enabled)
        mouseClick(row(s, "taskManager"))
        compare(taskSpy.count, 1)
    }

    // --- the options that do act --------------------------------------

    function test_each_option_fires_its_own_signal_data() {
        return [
            { tag: "lock", id: "lock", spy: "lockSpy" },
            { tag: "switchUser", id: "switchUser", spy: "switchSpy" },
            { tag: "signOut", id: "signOut", spy: "signOutSpy" },
            { tag: "cancel", id: "cancel", spy: "cancelSpy" }
        ]
    }

    function test_each_option_fires_its_own_signal(data) {
        var s = make()
        mouseClick(row(s, data.id))
        var total = 0
        for (var i = 0; i < spies.length; ++i)
            total += spies[i].count
        compare(total, 1, "exactly one signal for " + data.id)
        var spy = { lockSpy: lockSpy, switchSpy: switchSpy,
                    signOutSpy: signOutSpy, cancelSpy: cancelSpy }[data.spy]
        compare(spy.count, 1, data.id + " fired the wrong signal")
    }

    function test_an_option_the_session_refuses_is_refused() {
        // the session manager says it cannot switch user (a single-user
        // seat, or no seat manager at all)
        var s = make({ canSwitchUser: false })
        verify(!row(s, "switchUser").enabled)
        mouseClick(row(s, "switchUser"))
        compare(switchSpy.count, 0)
    }

    function test_cancel_is_never_refused() {
        // every other option can be taken away; the way out cannot, or
        // the screen becomes a trap
        var s = make({ canLock: false, canSwitchUser: false, canSignOut: false,
                       canChangePassword: false, canTaskManager: false })
        verify(row(s, "cancel").enabled)
        mouseClick(row(s, "cancel"))
        compare(cancelSpy.count, 1)
    }

    function test_escape_cancels() {
        var s = make()
        s.forceActiveFocus()
        keyClick(Qt.Key_Escape)
        compare(cancelSpy.count, 1)
    }

    function test_the_account_is_named() {
        var s = make()
        compare(findChild(s, "userName").text, "Alex Martin")
    }
}
