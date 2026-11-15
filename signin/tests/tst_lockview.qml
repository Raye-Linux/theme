import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

TestCase {
    id: test
    name: "LockView"
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
        Strings.lang = "en"
    }

    Component {
        id: viewComponent
        LockView {
            width: 800
            height: 600
            clockRunning: false
            now: new Date(2026, 9, 29, 21, 5)
            locale: Qt.locale("en_US")
        }
    }

    SignalSpy { id: dismissSpy; signalName: "dismissRequested" }
    SignalSpy { id: activitySpy; signalName: "activity" }

    function make(props) {
        var v = createTemporaryObject(viewComponent, test, props || {})
        verify(v)
        dismissSpy.clear()
        dismissSpy.target = v
        activitySpy.clear()
        activitySpy.target = v
        v.forceActiveFocus()
        return v
    }

    // A locale whose marker hangs below the digits, whatever the regions
    // ship today: the date goes under the ink, not under the baseline.
    readonly property var hangingLocale: ({
        timeFormat: () => "h:mm",
        dateFormat: () => "dddd",
        toString: (date, format) => format === "dddd" ? "Saturday" : "2:13 pgyj",
        amText: "am", pmText: "pm"
    })

    FontMetrics { id: clockInk; font.family: RayePalette.displayFamily; font.weight: Font.DemiBold; font.pixelSize: 128 }
    FontMetrics { id: dateInk; font.family: RayePalette.displayFamily; font.weight: Font.DemiBold; font.pixelSize: 20 }

    function inkGap(v) {
        var clock = findChild(v, "clock"), date = findChild(v, "date")
        var clockBottom = clock.y + clock.baselineOffset + clockInk.tightBoundingRect(clock.text).bottom
        var dateTop = date.y + date.baselineOffset + dateInk.tightBoundingRect(date.text).top
        return dateTop - clockBottom
    }

    function test_date_clears_the_clock_data() {
        return [
            { tag: "en_US", locale: Qt.locale("en_US") },
            { tag: "en_CA", locale: Qt.locale("en_CA") },
            { tag: "fr_CA", locale: Qt.locale("fr_CA") },
            { tag: "hanging", locale: hangingLocale }
        ]
    }

    // the "p" of "p.m." used to bite into the date
    function test_date_clears_the_clock(data) {
        var v = createTemporaryObject(viewComponent, test, { locale: data.locale, now: new Date(2026, 10, 7, 14, 13) })
        verify(inkGap(v) >= 12, data.tag + ": date is " + inkGap(v) + " px under the clock")
    }

    function test_clock_and_date() {
        var v = make()
        compare(findChild(v, "clock").text, "9:05 PM")
        compare(findChild(v, "date").text, "Thursday, October 29")
        v.locale = Qt.locale("fr_FR")
        compare(findChild(v, "clock").text, "21:05")
        compare(findChild(v, "date").text, "Jeudi 29 octobre")
    }

    // What is on screen, not what the function behind it returns: the
    // clock item's own text has to tell the morning from the afternoon.
    function test_clock_marker_is_on_screen() {
        var v = make({ now: new Date(2026, 9, 29, 3, 5) })
        var clock = findChild(v, "clock")
        verify(clock)
        compare(clock.text, "3:05 AM")
        compare(clock.Accessible.name, "3:05 AM")
        v.now = new Date(2026, 9, 29, 15, 5)
        compare(clock.text, "3:05 PM")
        // and a 24-hour region gains no marker
        v.locale = Qt.locale("fr_FR")
        compare(clock.text, "15:05")
    }

    function test_clock_position() {
        var v = make()
        var clock = findChild(v, "clock")
        verify(clock.y < 600 * 0.11, "the line box starts above the digits")
        compare(clock.font.pixelSize, 128)
    }

    function test_printable_key_goes_through() {
        var v = make()
        keyClick(Qt.Key_A, Qt.NoModifier)
        compare(dismissSpy.count, 1)
        compare(dismissSpy.signalArguments[0][0], "a")
    }

    function test_other_keys() {
        var v = make()
        keyClick(Qt.Key_Escape)
        compare(dismissSpy.count, 0, "Esc stays on the lock view")
        keyClick(Qt.Key_Shift)
        compare(dismissSpy.count, 1)
        compare(dismissSpy.signalArguments[0][0], "")
        keyClick(Qt.Key_C, Qt.ControlModifier)
        compare(dismissSpy.signalArguments[1][0], "", "a shortcut types nothing")
    }

    function test_mouse() {
        var v = make()
        mouseMove(v, 300, 300)
        mouseMove(v, 320, 310)
        verify(activitySpy.count > 0)
        compare(dismissSpy.count, 0, "moving the mouse only wakes")
        mouseClick(v, 400, 300)
        compare(dismissSpy.count, 1)
        mouseWheel(v, 400, 300, 0, -120)
        compare(dismissSpy.count, 2)
    }

    function test_swipe_up() {
        var v = make()
        mousePress(v, 400, 500)
        mouseMove(v, 400, 460)
        mouseMove(v, 400, 400)
        compare(dismissSpy.count, 1)
        mouseRelease(v, 400, 400)
        compare(dismissSpy.count, 1, "one swipe, one dismiss")
    }

    function test_hidden() {
        var v = make()
        v.shown = false
        verify(!v.enabled)
        tryVerify(function () { return !v.visible }, 1000, "gone once faded out")
        keyClick(Qt.Key_A)
        compare(dismissSpy.count, 0)
        v.shown = true
        tryCompare(v, "opacity", 1, 1000)
    }

    function test_hint_after_idle() {
        var v = make({ hintDelay: 150 })
        verify(!v.hintVisible)
        tryCompare(v, "hintVisible", true, 2000)
        v.showHint = false
        tryCompare(v, "hintVisible", false, 2000)
    }

    function test_status_icons() {
        var v = make({ networkIcon: "network-wired", batteryIcon: "battery-050" })
        var found = []
        function walk(item) {
            if (item.source !== undefined && item.source.toString().indexOf("icons/") >= 0 && item.visible)
                found.push(item.source.toString().replace(/.*icons\//, ""))
            for (var i = 0; i < item.children.length; ++i)
                walk(item.children[i])
        }
        walk(v)
        verify(found.indexOf("network-wired.svg") >= 0, found)
        verify(found.indexOf("battery-050.svg") >= 0, found)
    }
}
