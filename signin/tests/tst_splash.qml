// The login splash: the same frame as the "Welcome" of the sign-in screen,
// or the first sign-in on the first login of an account. Which one comes
// from a marker picture written by session/raye-first-login.sh.
import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

TestCase {
    id: test
    name: "Splash"
    width: 1280
    height: 800
    when: windowShown
    visible: true

    readonly property url splashUrl: Qt.resolvedUrl("../../lookandfeel/org.raye.night.desktop/contents/splash/Splash.qml")
    readonly property url marker: Qt.resolvedUrl("../../assets/signin/first-login-marker.png")

    function initTestCase() {
        Setup.assets(RayePalette)
    }

    function cleanup() {
        Strings.lang = "en"
    }

    function make(props) {
        var c = Qt.createComponent(splashUrl)
        compare(c.status, Component.Ready, c.errorString())
        var all = { width: 1280, height: 800 }
        for (var k in props)
            all[k] = props[k]
        var s = c.createObject(test, all)
        verify(s)
        return s
    }

    // ksplash writes this one property as Plasma starts
    function test_stage_is_there() {
        var s = make({ markerUrl: "file:///nonexistent/first-login.png" })
        verify("stage" in s)
        s.stage = 3
        compare(s.stage, 3)
        s.destroy()
    }

    function test_no_marker_says_welcome_data() {
        return [
            { tag: "en", lang: "en", title: "Welcome" },
            { tag: "fr", lang: "fr", title: "Bienvenue" }
        ]
    }

    function test_no_marker_says_welcome(data) {
        Strings.lang = data.lang
        var s = make({ markerUrl: "file:///nonexistent/first-login.png" })
        compare(s.mode, "welcome")
        compare(findChild(s, "status").title, data.title)
        s.destroy()
    }

    function test_marker_says_hi_data() {
        return [
            { tag: "en", lang: "en", text: "Hi" },
            { tag: "fr", lang: "fr", text: "Bonjour" }
        ]
    }

    function test_marker_says_hi(data) {
        Strings.lang = data.lang
        var s = make({ markerUrl: marker })
        tryCompare(s, "mode", "firstSignIn")
        tryCompare(findChild(s, "status"), "firstSignInText", data.text)
        s.destroy()
    }

    // the same words and the same place as the sign-in screen's Welcome:
    // nothing jumps from the greeter to the splash
    function test_uses_the_shared_status_screen() {
        var s = make({ markerUrl: "file:///nonexistent/first-login.png" })
        var st = findChild(s, "status")
        verify(st)
        compare(st.showBackdrop, true)
        compare(st.width, s.width)
        compare(st.height, s.height)
        s.destroy()
    }

    // The marker lookup goes through a module that may be missing. It may
    // only cost the first sign-in frame, never the splash.
    function test_markerless_path_when_lookup_fails() {
        var s = make({ markerUrl: "" })
        compare(s.mode, "welcome")
        s.destroy()
    }
}
