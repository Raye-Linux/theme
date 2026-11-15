import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

TestCase {
    id: test
    name: "PasswordField"
    width: 400
    height: 120
    when: windowShown
    // a TestCase is hidden by default, and so is everything made in it
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
        Strings.lang = "en"
    }

    function cleanup() {
        Motion.reducedMotion = false
    }

    Component {
        id: fieldComponent
        PasswordField {
            x: 50
            y: 40
        }
    }

    SignalSpy {
        id: acceptedSpy
        signalName: "accepted"
    }

    function make() {
        var f = createTemporaryObject(fieldComponent, test)
        verify(f)
        acceptedSpy.clear()
        acceptedSpy.target = f
        f.focusField()
        return f
    }

    function test_size_and_placeholder() {
        var f = make()
        compare(f.width, 300)
        compare(f.height, 36)
        compare(f.placeholder, "Password")
        compare(findChild(f, "input").placeholderText, "Password")
    }

    function test_typing_and_reveal() {
        var f = make()
        var input = findChild(f, "input")
        var reveal = findChild(f, "reveal")
        compare(input.echoMode, TextInput.Password)
        verify(!reveal.visible, "no eye on an empty field")
        keyClick(Qt.Key_A)
        keyClick(Qt.Key_B)
        compare(f.text, "ab")
        verify(reveal.visible)
        mouseClick(reveal)
        compare(input.echoMode, TextInput.Normal)
        verify(input.activeFocus, "focus goes back to the field")
        f.clear()
        compare(f.text, "")
        compare(f.revealed, false)
        compare(input.echoMode, TextInput.Password)
    }

    function test_enter_and_arrow_submit() {
        var f = make()
        keyClick(Qt.Key_S)
        keyClick(Qt.Key_Return)
        compare(acceptedSpy.count, 1)
        compare(acceptedSpy.signalArguments[0][0], "s")
        mouseClick(findChild(f, "submitButton"))
        compare(acceptedSpy.count, 2)
    }

    function test_busy_takes_no_input() {
        var f = make()
        f.text = "x"
        f.busy = true
        verify(!findChild(f, "submitButton").visible)
        verify(!findChild(f, "input").enabled)
        f.submit()
        compare(acceptedSpy.count, 0)
    }

    function test_shake() {
        var f = make()
        verify(!f.shaking)
        f.shake()
        verify(f.shaking)
        tryCompare(f, "shaking", false, 1000)
    }

    function test_shake_reduced_motion() {
        var f = make()
        Motion.reducedMotion = true
        f.shake()
        verify(!f.shaking)
    }

    function test_insert_first_key() {
        var f = make()
        f.insert("q")
        compare(f.text, "q")
        verify(findChild(f, "input").activeFocus)
    }
}
