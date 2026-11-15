import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

TestCase {
    id: test
    name: "Flyout"
    width: 800
    height: 600
    when: windowShown
    // a TestCase is hidden by default, and so is everything made in it
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
        Strings.lang = "en"
    }

    function cleanup() {
        RayePalette.highContrast = false
        RayePalette.largerText = false
    }

    Component {
        id: barComponent
        CornerBar {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 24
            networkIcon: "network-wired"
        }
    }

    Item {
        id: anchorBox
        x: 600
        y: 500
        width: 40
        height: 40
    }

    Component {
        id: flyoutComponent
        Flyout {
            anchorItem: anchorBox
            rows: [
                { id: "a", text: "Alpha" },
                { id: "b", text: "Bravo", enabled: false },
                { id: "c", text: "Charlie" },
                { id: "d", text: "Delta", checkable: true, checked: false }
            ]
        }
    }

    SignalSpy { id: triggeredSpy; signalName: "triggered" }

    function ids(rows) {
        return rows.map(function (r) { return r.id })
    }

    function test_power_rows() {
        var bar = createTemporaryObject(barComponent, test)
        var p = bar.powerFlyout
        compare(ids(p.rows), ["sleep", "shutDown", "restart"])
        bar.updatePending = true
        compare(ids(p.rows), ["sleep", "updateShutDown", "shutDown", "updateRestart", "restart"])
        bar.canHibernate = true
        bar.canSleep = false
        compare(ids(p.rows), ["hibernate", "updateShutDown", "shutDown", "updateRestart", "restart"])
        compare(p.rows[1].text, "Update and shut down")
    }

    function test_keyboard_navigation() {
        var f = createTemporaryObject(flyoutComponent, test)
        triggeredSpy.clear()
        triggeredSpy.target = f
        f.open()
        tryCompare(f, "opened", true)
        compare(f.currentRow, 0)
        keyClick(Qt.Key_Down)
        compare(f.currentRow, 2, "the disabled row is skipped")
        keyClick(Qt.Key_Down)
        compare(f.currentRow, 3)
        keyClick(Qt.Key_Down)
        compare(f.currentRow, 0, "wraps to the top")
        keyClick(Qt.Key_Up)
        compare(f.currentRow, 3, "wraps to the bottom")
        keyClick(Qt.Key_Space)
        compare(triggeredSpy.count, 1)
        compare(triggeredSpy.signalArguments[0][0], "d")
        verify(f.visible, "a toggle keeps it open")
        keyClick(Qt.Key_Up)
        keyClick(Qt.Key_Return)
        compare(triggeredSpy.signalArguments[1][0], "c")
        tryCompare(f, "visible", false)
    }

    function test_escape_closes() {
        var f = createTemporaryObject(flyoutComponent, test)
        f.open()
        tryCompare(f, "opened", true)
        keyClick(Qt.Key_Escape)
        tryCompare(f, "visible", false)
    }

    function test_opens_upward() {
        var f = createTemporaryObject(flyoutComponent, test)
        f.open()
        tryCompare(f, "opened", true)
        verify(f.y + f.height <= 0, "above its button")
        compare(Math.round(f.x + f.width), anchorBox.width, "right edges line up")
        compare(f.rowCount, 4)
    }

    function test_access_toggles() {
        var bar = createTemporaryObject(barComponent, test)
        var a = bar.accessFlyout
        a.open()
        tryCompare(a, "opened", true)
        a.pick(2)
        verify(RayePalette.highContrast)
        verify(a.visible)
        compare(a.rows[2].checked, true)
        a.pick(1)
        verify(RayePalette.largerText)
        var toggled = []
        bar.keyboardToggled.connect(function (on) { toggled.push(on) })
        a.pick(0)
        compare(toggled, [true])
        bar.keyboardAvailable = false
        compare(ids(a.rows), ["largerText", "highContrast"])
    }

    function test_button_opens_its_flyout() {
        var bar = createTemporaryObject(barComponent, test)
        waitForRendering(bar)
        mouseClick(findChild(bar, "powerButton"))
        tryCompare(bar.powerFlyout, "opened", true)
        mouseClick(findChild(bar, "accessButton"))
        tryCompare(bar.accessFlyout, "opened", true)
        tryCompare(bar.powerFlyout, "visible", false)
    }

    function test_emergency_restart() {
        var bar = createTemporaryObject(barComponent, test, { emergencyRestart: true })
        var count = 0
        bar.emergencyRestartRequested.connect(function () { count++ })
        waitForRendering(bar)
        var power = findChild(bar, "powerButton")
        mouseClick(power, power.width / 2, power.height / 2, Qt.LeftButton, Qt.ControlModifier)
        compare(count, 1)
        verify(!bar.powerFlyout.visible)
        mouseClick(power)
        tryCompare(bar.powerFlyout, "opened", true)
        compare(count, 1, "a plain click opens the flyout")
    }

    function test_layout_code() {
        var bar = createTemporaryObject(barComponent, test)
        var layout = findChild(bar, "layoutButton")
        verify(!layout.visible)
        bar.layoutCode = "FR"
        bar.showLayout = true
        verify(layout.visible)
        compare(layout.text, "FR")
    }
}
