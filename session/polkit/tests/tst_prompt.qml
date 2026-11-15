import QtQuick
import QtTest
import org.raye.signin
import "../qml"

TestCase {
    id: test
    name: "Prompt"
    width: 1280
    height: 800
    when: windowShown
    visible: true

    function initTestCase() {
        RayePalette.assets = Qt.resolvedUrl("../../../assets/signin/")
        RayePalette.eye = Qt.resolvedUrl("../../../assets/logo/raye-eye.png")
        Motion.reducedMotion = true
    }

    function cleanup() {
        Strings.lang = ""
    }

    Component {
        id: fakeComponent
        FakePrompt {}
    }

    Component {
        id: promptComponent
        Prompt {
            width: test.width
            height: test.height
        }
    }

    function make(props, lang) {
        Strings.lang = lang || "en"
        var fake = createTemporaryObject(fakeComponent, test, props || {})
        var p = createTemporaryObject(promptComponent, test, { prompt: fake })
        verify(p)
        waitForRendering(p)
        return { prompt: p, fake: fake }
    }

    function texts(item) {
        var out = []
        if (item.text !== undefined && item.visible)
            out.push(String(item.text).replace(/[\u00a0\u202f]/g, " "))
        for (var i = 0; i < item.children.length; ++i)
            out = out.concat(texts(item.children[i]))
        return out
    }

    function hasText(item, s) {
        return texts(item).indexOf(s) >= 0
    }

    function test_verified() {
        var o = make()
        verify(hasText(o.prompt, "Do you want to allow this app to make changes to your device?"))
        verify(hasText(o.prompt, "Settings"))
        verify(hasText(o.prompt, "Verified publisher: Raye Linux"))
        verify(hasText(o.prompt, "Yes"))
        verify(hasText(o.prompt, "No"))
        verify(!hasText(o.prompt, "Publisher: Unknown"))
    }

    function test_unknown() {
        var o = make({ publisher: "", appName: "install.sh", program: "/home/techoraye/install.sh" })
        verify(hasText(o.prompt, "Publisher: Unknown"))
        verify(!hasText(o.prompt, "Verified publisher: Raye Linux"))
    }

    function test_french() {
        var o = make({}, "fr")
        verify(hasText(o.prompt, "Voulez-vous autoriser cette application à apporter des modifications à votre appareil ?"))
        verify(hasText(o.prompt, "Éditeur vérifié : Raye Linux"))
        verify(hasText(o.prompt, "Oui"))
        verify(hasText(o.prompt, "Non"))
    }

    function test_details() {
        var o = make({ command: "/usr/bin/true" })
        var details = findChild(o.prompt, "details")
        verify(!details.visible)
        mouseClick(findChild(o.prompt, "detailsLink"))
        verify(details.visible)
        verify(hasText(o.prompt, "org.freedesktop.timedate1.set-timezone"))
        verify(hasText(o.prompt, "/usr/bin/raye-settings"))
        verify(hasText(o.prompt, "Program location"))
        verify(hasText(o.prompt, "Hide details"))
    }

    function test_enterIsYes() {
        var o = make()
        var field = findChild(o.prompt, "passwordField")
        tryVerify(function () { return field.inputItem.activeFocus })
        keyClick("s")
        keyClick("e")
        keyClick(Qt.Key_Return)
        compare(o.fake.request.accepted, ["se"])
        compare(o.fake.request.rejected, 0)
    }

    function test_yesButton() {
        var o = make()
        findChild(o.prompt, "passwordField").text = "pw"
        mouseClick(findChild(o.prompt, "yesButton"))
        compare(o.fake.request.accepted, ["pw"])
    }

    function test_escIsNo() {
        var o = make()
        keyClick(Qt.Key_Escape)
        compare(o.fake.request.rejected, 1)
        compare(o.fake.request.accepted.length, 0)
    }

    function test_noButton() {
        var o = make()
        mouseClick(findChild(o.prompt, "noButton"))
        compare(o.fake.request.rejected, 1)
    }

    function test_noButtonWithEnter() {
        var o = make()
        findChild(o.prompt, "noButton").forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(o.fake.request.rejected, 1)
        compare(o.fake.request.accepted.length, 0)
    }

    function test_wrongPassword() {
        var o = make()
        var field = findChild(o.prompt, "passwordField")
        field.text = "nope"
        o.fake.request.failOnce()
        compare(field.text, "")
        var problem = findChild(o.prompt, "problem")
        verify(problem.visible)
        compare(problem.text, "The password is incorrect. Try again.")
    }

    function test_busyBlocksYes() {
        var o = make()
        o.fake.request.busy = true
        verify(!findChild(o.prompt, "yesButton").enabled)
        keyClick(Qt.Key_Return)
        compare(o.fake.request.accepted.length, 0)
        // No still works while PAM thinks
        keyClick(Qt.Key_Escape)
        compare(o.fake.request.rejected, 1)
    }

    function test_standardUser() {
        var o = make({ userIsSelf: false, users: [
            { user: "alex", name: "Alex Martin", face: "" },
            { user: "camille", name: "Camille Roy", face: "" }
        ] })
        verify(hasText(o.prompt, "To continue as root, enter the password of a sudo user."))
        verify(!hasText(o.prompt, "Enter your password to continue as root."))
        verify(hasText(o.prompt, "Alex Martin"))
        verify(hasText(o.prompt, "More choices"))
    }

    function test_ownPassword() {
        var o = make()
        verify(hasText(o.prompt, "Root access needed"))
        verify(hasText(o.prompt, "Enter your password to continue as root."))
        verify(!hasText(o.prompt, "To continue as root, enter the password of a sudo user."))
    }

    function test_ownPasswordFrench() {
        var o = make({}, "fr")
        verify(hasText(o.prompt, "Accès root requis"))
        verify(hasText(o.prompt, "Entrez votre mot de passe pour continuer en tant que root."))
    }

    function test_dimOnlyScreen() {
        Strings.lang = "en"
        var fake = createTemporaryObject(fakeComponent, test)
        var p = createTemporaryObject(promptComponent, test, { prompt: fake, showCard: false })
        verify(!findChild(p, "card").visible)
    }
}
