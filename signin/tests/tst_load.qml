import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

// Every type of the module loads and builds under the scene graph in use
// (run it once with QT_QUICK_BACKEND=software).
TestCase {
    id: test
    name: "Load"
    width: 1280
    height: 800
    when: windowShown
    // a TestCase is hidden by default, and so is everything made in it
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
    }

    function test_types_data() {
        return ["Backdrop", "LockView", "SecurityScreen", "SignInColumn", "UserPicture", "PasswordField",
                "UserList", "CornerBar", "CornerButton", "Flyout", "PowerFlyout",
                "AccessFlyout", "Spinner", "StatusScreen", "MethodHint", "OnScreenKeyboard",
                "ShadowText", "TextLink"].map(function (t) { return { tag: t, type: t } })
    }

    function test_types(data) {
        var c = Qt.createQmlObject("import QtQuick; import org.raye.signin; Item { width: 1280; height: 800; "
                                   + data.type + " {} }", test, data.type)
        verify(c, data.type)
        c.destroy()
    }

    function test_singletons() {
        verify(RayePalette.text !== undefined)
        verify(Strings.tr.password !== undefined)
        compare(Motion.normal, 200)
    }

    Component {
        id: sceneComponent
        Scene {
            width: 1280
            height: 800
        }
    }

    function test_gallery_scenes_data() {
        return ["lock", "signin", "security", "error", "caps", "passwordless", "lockscreen",
                "keyboard", "welcome", "updating", "firstSignIn"].map(function (v) { return { tag: v, view: v } })
    }

    function test_gallery_scenes(data) {
        var s = createTemporaryObject(sceneComponent, test, { view: data.view })
        verify(s)
        waitForRendering(s)
    }

    Component {
        id: keyboardComponent
        Item {
            width: 1280
            height: 800
            OnScreenKeyboard { id: k; objectName: "keyboard" }
        }
    }

    function test_keyboard_panel_loads() {
        var host = createTemporaryObject(keyboardComponent, test)
        var k = findChild(host, "keyboard")
        verify(k.available)
        k.active = true
        tryVerify(function () { return k.panelHeight > 0 }, 3000, "the VirtualKeyboard panel loads")
        verify(k.available)
        k.active = false
    }
}
