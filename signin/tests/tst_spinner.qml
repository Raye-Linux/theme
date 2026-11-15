import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

// The ring moves like the boot screen's frames (art/plymouth.py): the
// expected numbers below are ring_arc() of that script.
TestCase {
    id: test
    name: "Spinner"
    width: 400
    height: 300
    when: windowShown
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
    }

    function cleanup() {
        Motion.reducedMotion = false
    }

    Component {
        id: spinnerComponent
        Spinner { size: 32 }
    }

    function test_same_arc_as_the_boot_screen_data() {
        return [
            { tag: "start", phase: 0, tail: 0, head: 12 },
            { tag: "head runs", phase: 0.15, tail: 67.5, head: 103.166 },
            { tag: "long", phase: 0.3, tail: 135, head: 309.724 },
            { tag: "both move", phase: 0.5, tail: 233.935, head: 500.785 },
            { tag: "tail catches up", phase: 0.7, tail: 477.724, head: 597 },
            { tag: "end", phase: 1, tail: 720, head: 732 }
        ]
    }

    function test_same_arc_as_the_boot_screen(data) {
        var s = createTemporaryObject(spinnerComponent, test, { running: false })
        s.phase = data.phase
        fuzzyCompare(s._tail, data.tail, 0.05)
        fuzzyCompare(s._head, data.head, 0.05)
    }

    function test_never_shorter_than_a_dot_nor_a_full_circle() {
        var s = createTemporaryObject(spinnerComponent, test, { running: false })
        var tail = -1, head = -1, longest = 0
        for (var i = 0; i <= 120; i++) {
            s.phase = i / 120
            verify(s._len >= 12 - 1e-6 && s._len < 360, "phase " + s.phase)
            verify(s._tail >= tail && s._head > head, "both ends go forward at " + s.phase)
            tail = s._tail
            head = s._head
            longest = Math.max(longest, s._len)
        }
        // the colour runs from the head round to the tail and the round
        // ends: the gradient stops have to stay in order
        verify(2 * s._cap + longest < 360)
    }

    function test_turns_while_shown() {
        var s = createTemporaryObject(spinnerComponent, test)
        var first = s.phase
        wait(150)
        verify(s.phase !== first)
    }

    function test_stands_still_with_reduced_motion() {
        Motion.reducedMotion = true
        var s = createTemporaryObject(spinnerComponent, test)
        var first = s.phase
        wait(150)
        compare(s.phase, first)
        verify(s._len > 180)
    }

    function test_stops_when_hidden() {
        var s = createTemporaryObject(spinnerComponent, test, { visible: false })
        var first = s.phase
        wait(150)
        compare(s.phase, first)
    }

    function test_line_width_follows_the_size() {
        var small = createTemporaryObject(spinnerComponent, test, { size: 18, running: false })
        var big = createTemporaryObject(spinnerComponent, test, { size: 96, running: false })
        compare(small.lineWidth, 2)
        compare(big.lineWidth, 10)
    }
}
