import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

TestCase {
    id: test
    name: "SignInColumn"
    // the offscreen platform's screen is 800 x 600: clicks below it are lost
    width: 800
    height: 600
    when: windowShown
    // a TestCase is hidden by default, and so is everything made in it
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
        Strings.lang = "en"
    }

    Component {
        id: columnComponent
        SignInColumn {
            width: 800
            height: 600
            userName: "Alex Martin"
        }
    }

    SignalSpy { id: submittedSpy; signalName: "submitted" }
    SignalSpy { id: backSpy; signalName: "backRequested" }
    SignalSpy { id: switchSpy; signalName: "switchUserRequested" }

    function make() {
        var c = createTemporaryObject(columnComponent, test)
        verify(c)
        for (var s of [submittedSpy, backSpy, switchSpy]) {
            s.clear()
            s.target = c
        }
        c.focusField()
        return c
    }

    function test_layout() {
        var c = make()
        var picture = findChild(c, "picture")
        var p = picture.mapToItem(c, 0, 0)
        compare(picture.width, 192)
        compare(Math.round(p.y + picture.height / 2), 240, "picture centre at 40 %")
        compare(findChild(c, "name").text, "Alex Martin")
    }

    function test_caps_lock_line() {
        var c = make()
        var caps = findChild(c, "capsLine")
        verify(!caps.visible)
        c.capsLock = true
        verify(caps.visible)
        compare(caps.text, "Caps Lock is on")
    }

    function test_wrong_password() {
        var c = make()
        c.capsLock = true
        c.passwordField.text = "nope"
        c.fail()
        compare(c.errorText, "The password is incorrect. Try again.")
        compare(c.passwordField.text, "", "the field is cleared")
        verify(c.passwordField.shaking)
        verify(findChild(c, "errorLine").visible)
        verify(!findChild(c, "capsLine").visible, "the error replaces the caps lock line")
        verify(findChild(c, "okLink").visible)
        waitForRendering(c)
        mouseClick(findChild(c, "okLink"))
        verify(!c.failed)
        verify(findChild(c, "input").activeFocus, "OK gives the focus back to the field")
    }

    function test_typing_clears_the_error() {
        var c = make()
        c.fail("Try once more.")
        compare(c.errorText, "Try once more.")
        keyClick(Qt.Key_X)
        verify(!c.failed)
    }

    function test_submit() {
        var c = make()
        c.typeText("p")
        keyClick(Qt.Key_W)
        keyClick(Qt.Key_Enter)
        compare(submittedSpy.count, 1)
        compare(submittedSpy.signalArguments[0][0], "pw")
    }

    function test_passwordless() {
        var c = make()
        c.needsPassword = false
        verify(!c.passwordField.visible)
        var button = findChild(c, "signInButton")
        verify(button.visible)
        compare(button.text, "Sign in")
        waitForRendering(c)
        mouseClick(button)
        compare(submittedSpy.count, 1)
        compare(submittedSpy.signalArguments[0][0], "")
    }

    function test_escape_and_links() {
        var c = make()
        keyClick(Qt.Key_Escape)
        compare(backSpy.count, 1)
        verify(!findChild(c, "switchLink").visible)
        verify(!findChild(c, "optionsLink").visible, "no other sign-in method yet")
        c.showSwitchUser = true
        verify(findChild(c, "switchLink").visible)
        waitForRendering(c)
        mouseClick(findChild(c, "switchLink"))
        compare(switchSpy.count, 1)
    }

    function test_method_hint() {
        var c = make()
        var hint = findChild(c, "hint")
        verify(!hint.visible)
        c.methodHint = Strings.tr.fingerprint
        tryVerify(function () { return hint.visible })
        var h = hint.mapToItem(c, 0, 0)
        var f = c.passwordField.mapToItem(c, 0, 0)
        verify(h.y < f.y, "the hint is above the field")
    }

    function test_keyboard_inset() {
        var c = make()
        Motion.reducedMotion = true
        c.bottomInset = 300
        tryVerify(function () {
            var f = c.passwordField.mapToItem(c, 0, 0)
            return f.y + c.passwordField.height <= 600 - 300
        }, 1000, "the field stays above the keyboard")
        Motion.reducedMotion = false
    }
}
