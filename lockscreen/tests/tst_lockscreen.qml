import QtQuick
import QtTest
import org.raye.signin
import "../../signin/tests/Setup.js" as Setup

// The greeter's main file: loads without the greeter's context (every
// context object is read safely), keeps the names the greeter writes and
// connects, and passes them through.
TestCase {
    id: test
    name: "LockScreen"
    width: 1280
    height: 800
    when: windowShown
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
        Strings.lang = "en"
    }

    function cleanupTestCase() {
        Motion.reducedMotion = false
    }

    SignalSpy { id: ramSpy; signalName: "suspendToRam" }
    SignalSpy { id: diskSpy; signalName: "suspendToDisk" }

    function make() {
        var c = Qt.createComponent(Qt.resolvedUrl("../LockScreen.qml"))
        compare(c.status, Component.Ready, c.errorString())
        var o = createTemporaryObject(c, test, { width: 1280, height: 800 })
        verify(o)
        ramSpy.target = o
        ramSpy.clear()
        diskSpy.target = o
        diskSpy.clear()
        return o
    }

    function test_magicNames() {
        var o = make()
        // properties the greeter writes
        compare(o.viewVisible, false)
        compare(o.suspendToRamSupported, false)
        compare(o.suspendToDiskSupported, false)
        compare(o.locked, true)
        compare(typeof o.notification, "string")
        // signals it connects
        compare(typeof o.suspendToRam, "function")
        compare(typeof o.suspendToDisk, "function")
        compare(typeof o.clearPassword, "function")
        compare(typeof o.notificationRepeated, "function")
    }

    function test_noContextIsSafe() {
        var o = make()
        compare(o.pam, null)
        compare(o.userName, "")
        compare(o.userImage, "")
        compare(o.wallpaperImage, "")
        compare(o.showMedia, true)
        var view = findChild(o, "lockView")
        verify(view)
        // it fades in from black, like after Win+L
        tryVerify(function () { return view.opacity === 1 })
    }

    function test_sleepAndHibernateGoToTheGreeter() {
        var o = make()
        o.suspendToRamSupported = true
        o.suspendToDiskSupported = true
        var bar = findChild(o, "cornerBar")
        verify(bar)
        compare(bar.canSleep, true)
        compare(bar.canHibernate, true)
        bar.sleepRequested()
        compare(ramSpy.count, 1)
        bar.hibernateRequested()
        compare(diskSpy.count, 1)
    }

    function test_graceFlagReachesTheView() {
        var o = make()
        o.locked = false
        var ui = findChild(o, "lockView").parent
        compare(ui.locked, false)
        o.locked = true
        compare(ui.locked, true)
    }

    function test_clearPasswordSignal() {
        var o = make()
        var ui = findChild(o, "lockView").parent
        ui.openSignIn("abc")
        compare(ui.column.passwordField.text, "abc")
        o.clearPassword()
        compare(ui.column.passwordField.text, "")
    }

    // the greeter makes one LockScreen per screen in one engine: they get
    // one sign-in, and it survives the screen that made it going away
    function test_screensShareOneSignIn() {
        var a = make()
        var b = make()
        var uiA = findChild(a, "lockView").parent
        var uiB = findChild(b, "lockView").parent
        verify(uiA.share !== null)
        compare(uiA.share, uiB.share)
        uiA.openSignIn("pw")
        compare(uiB.signingIn, true)
        compare(uiB.column.passwordField.text, "pw")
        a.destroy()
        gc()
        tryVerify(function () { return uiB.share.views === 1 })
        compare(uiB.driving, true)
        uiB.column.passwordField.insert("!")
        compare(uiB.share.password, "pw!")
        uiB.closeSignIn()
        compare(uiB.signingIn, false)
    }
}
