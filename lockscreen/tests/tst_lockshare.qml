import QtQuick
import QtTest
import org.raye.signin
import "../../signin/tests/Setup.js" as Setup
import ".."

// Two screens, one sign-in: the greeter's views share a LockShare. KWin
// gives the keys of a locked session to one view only, so whatever is
// typed into it must show on the other screen too, and PAM must be asked
// once, not once per screen.
TestCase {
    id: test
    name: "LockShare"
    width: 1280
    height: 800
    when: windowShown
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
        Strings.lang = "en"
        Motion.reducedMotion = true
    }

    function cleanupTestCase() {
        Motion.reducedMotion = false
    }

    Component {
        id: fakeComponent
        QtObject {
            property bool busy: false
            property bool hadPrompt: false
            property string prompt: ""
            property string promptForSecret: ""
            property string infoMessage: ""
            property string errorMessage: ""
            property int authenticatorTypes: 0
            property int starts: 0
            property var responses: []

            signal succeeded()
            signal failed(int kind, var authenticator)
            signal noninteractiveInfo(int kind, var authenticator)
            signal noninteractiveError(int kind, var authenticator)

            function startAuthenticating() { starts += 1 }
            function respond(p) { responses = responses.concat([p]) }

            function askPassword() {
                hadPrompt = true
                promptForSecret = "Password: "
                promptForSecretChanged()
            }
        }
    }

    Component {
        id: shareComponent
        LockShare {}
    }

    Component {
        id: uiComponent
        LockUi {
            width: 640
            height: 800
            userName: "Anna Roy"
        }
    }

    property var auth: null
    property var share: null

    // two views side by side, like the laptop's screen and a monitor
    function makeTwo() {
        auth = createTemporaryObject(fakeComponent, test)
        // kept until the end: the views go first, as in the greeter
        share = shareComponent.createObject(test)
        var a = createTemporaryObject(uiComponent, test, { authenticator: auth, share: share, x: 0 })
        var b = createTemporaryObject(uiComponent, test, { authenticator: auth, share: share, x: 640 })
        verify(a && b)
        a.lockView.clockRunning = false
        b.lockView.clockRunning = false
        return [a, b]
    }

    function test_firstViewDrives() {
        var v = makeTwo()
        compare(share.views, 2)
        compare(v[0].driving, true)
        compare(v[1].driving, false)
    }

    function test_signInOpensOnEveryScreen() {
        var v = makeTwo()
        v[1].openSignIn("")
        compare(v[0].signingIn, true)
        compare(v[1].signingIn, true)
        verify(v[0].column.visible)
        verify(v[1].column.visible)
        // PAM is asked once, whichever screen opened it
        compare(auth.starts, 1)
    }

    function test_typedTextShowsOnTheOtherScreen() {
        var v = makeTwo()
        // the keys go to the first view (KWin's pick), the user looks at
        // the second one
        v[0].openSignIn("s")
        v[0].column.passwordField.insert("ecret")
        compare(v[0].column.passwordField.text, "secret")
        compare(v[1].column.passwordField.text, "secret")
        // and the other way round (the arrow clicked on the monitor)
        v[1].column.passwordField.insert("!")
        compare(v[0].column.passwordField.text, "secret!")
    }

    function test_submitFromTheOtherScreenGoesOutOnce() {
        var v = makeTwo()
        v[0].openSignIn("")
        auth.askPassword()
        v[1].column.passwordField.insert("pw")
        v[1].column.passwordField.submit()
        compare(auth.responses.length, 1)
        compare(auth.responses[0], "pw")
        compare(v[0].busy, true)
        compare(v[1].busy, true)
    }

    function test_wrongPasswordShowsOnEveryScreen() {
        var v = makeTwo()
        v[0].openSignIn("")
        auth.askPassword()
        v[0].column.passwordField.insert("bad")
        v[0].column.passwordField.submit()
        auth.failed(0, null)
        verify(v[0].column.failed)
        verify(v[1].column.failed)
        compare(v[0].column.passwordField.text, "")
        compare(v[1].column.passwordField.text, "")
        compare(v[1].busy, false)
        compare(share.password, "")
    }

    function test_rightPasswordUnlocksEveryScreen() {
        var v = makeTwo()
        var spyA = createTemporaryObject(spyComponent, test, { target: v[0] })
        var spyB = createTemporaryObject(spyComponent, test, { target: v[1] })
        v[1].openSignIn("")
        auth.askPassword()
        v[1].column.passwordField.insert("good")
        v[1].column.passwordField.submit()
        auth.succeeded()
        compare(spyA.count, 1)
        compare(spyB.count, 1)
        compare(share.password, "")
    }

    function test_escapeOnOneScreenClosesBoth() {
        var v = makeTwo()
        v[0].openSignIn("abc")
        v[1].closeSignIn()
        compare(v[0].signingIn, false)
        compare(v[0].column.passwordField.text, "")
        verify(v[0].lockView.shown)
        verify(v[1].lockView.shown)
    }

    // the keys go to the second screen's view (KWin's pick): Esc puts the
    // curtain back, and the next key lifts it again on both screens
    function test_escapeThenKeyOnTheOtherScreen() {
        var v = makeTwo()
        v[1].lockView.forceActiveFocus()
        keyClick(Qt.Key_A)
        compare(v[0].signingIn, true)
        compare(v[1].column.passwordField.text, "")
        keyClick(Qt.Key_B)
        compare(v[0].column.passwordField.text, "b")
        keyClick(Qt.Key_Escape)
        compare(v[1].signingIn, false)
        verify(v[1].lockView.activeFocus)
        keyClick(Qt.Key_Space)
        compare(v[1].signingIn, true)
        compare(v[0].column.passwordField.text, "")
    }

    function test_driverMovesWhenItsScreenGoes() {
        var v = makeTwo()
        v[0].openSignIn("x")
        v[0].destroy()
        tryCompare(share, "views", 1)
        compare(v[1].driving, true)
        // the try goes on from the screen that is left
        compare(v[1].column.passwordField.text, "x")
        auth.askPassword()
        v[1].column.passwordField.submit()
        compare(auth.responses.length, 1)
    }

    function test_lastScreenLeavingClearsTheState() {
        var s = shareComponent.createObject(test)
        var a = createTemporaryObject(uiComponent, test, { share: s })
        a.openSignIn("left over")
        compare(s.password, "left over")
        a.destroy()
        tryCompare(s, "views", 0)
        compare(s.signingIn, false)
        compare(s.password, "")
    }

    function test_newScreenGetsWhatIsTyped() {
        var v = makeTwo()
        v[0].openSignIn("abc")
        var c = createTemporaryObject(uiComponent, test, { authenticator: auth, share: share })
        compare(c.signingIn, true)
        compare(c.column.passwordField.text, "abc")
        compare(c.driving, false)
    }

    Component {
        id: spyComponent
        SignalSpy { signalName: "unlocked" }
    }
}
