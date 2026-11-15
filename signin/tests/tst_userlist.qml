import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

TestCase {
    id: test
    name: "UserList"
    width: 400
    height: 600
    when: windowShown
    // a TestCase is hidden by default, and so is everything made in it
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
    }

    readonly property var three: [
        { name: "alex", realName: "Alex Martin", icon: "" },
        { name: "camille", realName: "", icon: "" },
        { name: "sam", realName: "Sam Lee", icon: "" }
    ]

    Component {
        id: listComponent
        UserList {
            x: 48
            y: 48
        }
    }

    ListModel {
        id: rolesModel
        ListElement { name: "a"; realName: "Ann A"; icon: "" }
        ListElement { name: "b"; realName: "Bo B"; icon: "" }
    }

    SignalSpy { id: selectedSpy; signalName: "userSelected" }

    function make(model) {
        var l = createTemporaryObject(listComponent, test, { model: model })
        verify(l)
        selectedSpy.clear()
        selectedSpy.target = l
        return l
    }

    function test_hidden_with_one_user() {
        var l = make([three[0]])
        verify(!l.visible)
        l.model = three
        verify(l.visible)
        compare(l.implicitHeight, 3 * 48 + 2 * 4)
    }

    function test_click_selects() {
        var l = make(three)
        l.currentIndex = 0
        waitForRendering(l)
        mouseClick(l, 100, 48 + 4 + 24)
        compare(selectedSpy.count, 1)
        compare(selectedSpy.signalArguments[0][0], 1)
        compare(l.currentIndex, 1)
    }

    function test_keys_move() {
        var l = make(three)
        l.currentIndex = 0
        l.forceActiveFocus()
        keyClick(Qt.Key_Down)
        compare(l.currentIndex, 1)
        keyClick(Qt.Key_Down)
        keyClick(Qt.Key_Down)
        compare(l.currentIndex, 2, "stops at the last one")
        keyClick(Qt.Key_Up)
        compare(l.currentIndex, 1)
        compare(selectedSpy.count, 4)
    }

    function test_six_then_scroll() {
        var many = []
        for (var i = 0; i < 9; ++i)
            many.push({ name: "u" + i, realName: "User " + i, icon: "" })
        var l = make(many)
        compare(l.implicitHeight, 6 * 48 + 5 * 4)
    }

    function test_role_model_and_names() {
        var l = make(rolesModel)
        verify(l.visible)
        compare(l.count, 2)
        waitForRendering(l)
        var roleTexts = []
        function collect(item) {
            if (item.text !== undefined && typeof item.text === "string")
                roleTexts.push(item.text)
            for (var i = 0; i < item.children.length; ++i)
                collect(item.children[i])
        }
        collect(l)
        verify(roleTexts.indexOf("Ann A") >= 0, "roles of an item model: " + roleTexts.join(","))
        // a JS array without realName falls back to the login name
        l.model = three
        waitForRendering(l)
        var found = false
        var texts = []
        function walk(item) {
            if (item.text !== undefined && typeof item.text === "string")
                texts.push(item.text)
            for (var i = 0; i < item.children.length; ++i)
                walk(item.children[i])
        }
        walk(l)
        verify(texts.indexOf("camille") >= 0, texts.join(","))
        verify(texts.indexOf("Alex Martin") >= 0)
    }
}
