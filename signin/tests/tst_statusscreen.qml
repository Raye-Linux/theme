import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

TestCase {
    id: test
    name: "StatusScreen"
    width: 1280
    height: 800
    when: windowShown
    // a TestCase is hidden by default, and so is everything made in it
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
    }

    function cleanup() {
        Strings.lang = "en"
        Motion.reducedMotion = false
    }

    Component {
        id: screenComponent
        StatusScreen {
            width: 1280
            height: 800
        }
    }

    function test_titles_data() {
        return [
            { tag: "welcome en", lang: "en", mode: "welcome", title: "Welcome" },
            { tag: "signingOut en", lang: "en", mode: "signingOut", title: "Signing out" },
            { tag: "shuttingDown en", lang: "en", mode: "shuttingDown", title: "Shutting down" },
            { tag: "restarting en", lang: "en", mode: "restarting", title: "Restarting" },
            { tag: "welcome fr", lang: "fr", mode: "welcome", title: "Bienvenue" },
            { tag: "shuttingDown fr", lang: "fr", mode: "shuttingDown", title: "Arrêt en cours" }
        ]
    }

    function test_titles(data) {
        Strings.lang = data.lang
        var s = createTemporaryObject(screenComponent, test, { mode: data.mode })
        compare(s.title, data.title)
        compare(s.subtitle, "")
    }

    function test_updating() {
        Strings.lang = "en"
        var s = createTemporaryObject(screenComponent, test, { mode: "updating", progress: 37 })
        compare(s.title, "Working on updates  37%")
        compare(s.subtitle, "Don't turn off your PC")
        Strings.lang = "fr"
        compare(s.title, "Mises à jour en cours  37 %")
        s.progress = -1
        compare(s.title, "Mises à jour en cours")
    }

    function test_mode_change() {
        Strings.lang = "en"
        var s = createTemporaryObject(screenComponent, test, { mode: "welcome" })
        s.mode = "restarting"
        tryCompare(s, "title", "Restarting", 1000)
        Motion.reducedMotion = true
        s.mode = "signingOut"
        compare(s.title, "Signing out", "no animation: at once")
    }

    function test_first_sign_in() {
        Strings.lang = "en"
        var s = createTemporaryObject(screenComponent, test, { mode: "firstSignIn" })
        compare(s.firstSignInText, "Hi")
        verify(!s.minimumShown)
        tryCompare(s, "minimumShown", true, 4000)
        tryCompare(s, "firstSignInText", "We're getting things ready", 6000)
    }
}
