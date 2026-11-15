// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import org.kde.plasma.plasmoid
import "../org.raye.animated/contents/ui/rest.js" as Rest

Item {
    id: top
    width: 320
    height: 180

    // Plasma puts a wallpaper in its containment, which knows its screen
    Loader {
        id: loader
        anchors.fill: parent
        property rect screenGeometry: Qt.rect(1280, 0, 1920, 1080)
    }

    function load(file, extra, releaseAfter) {
        loader.source = ""
        Registry.tasks = null
        Registry.power = null
        const conf = { Source: Qt.resolvedUrl(file), Still: "", FillMode: 2,
                       PauseCovered: true, PauseOnBattery: true, MaxFps: 30 }
        for (const k in extra) conf[k] = extra[k]
        loader.setSource("../org.raye.animated/contents/ui/main.qml",
                         { configuration: conf, releaseAfter: releaseAfter || 30000 })
        return loader.item
    }

    TestCase {
        name: "Rest"
        function test_gif_by_name() {
            verify(Rest.isGif("file:///a/b/Cat.GIF"))
            verify(!Rest.isGif("file:///a/b/gif.webm"))
        }
        function test_minimized_window_does_not_cover() {
            verify(!Rest.covered([{ maximized: true, minimized: true }]))
            verify(Rest.covered([{ maximized: false, fullScreen: true }]))
            verify(!Rest.covered([{ maximized: false, fullScreen: false }]))
        }
        function test_full_screen_rests_even_with_the_option_off() {
            verify(Rest.shouldRest({ visible: true, fullScreen: true, pauseCovered: false }))
            verify(!Rest.shouldRest({ visible: true, covered: true, pauseCovered: false }))
            verify(Rest.shouldRest({ visible: true, onBattery: true, pauseOnBattery: true }))
            verify(!Rest.shouldRest({ visible: true, onBattery: true, pauseOnBattery: false }))
            verify(Rest.shouldRest({ visible: false }))
        }
        function test_fast_gif_skips_frames_at_the_cap() {
            // 50 fps drawn at 25: two frames a tick
            let f = 0, c = 0
            for (let i = 0; i < 4; ++i) {
                const n = Rest.nextFrame(f, 30, c, 50, 25)
                f = n.frame; c = n.carry
            }
            compare(f, 8)
            compare(Rest.interval(50, 25), 40)
            compare(Rest.interval(10, 30), 100)
        }
    }

    TestCase {
        name: "Gif"
        when: windowShown
        function test_capped_and_resting() {
            const w = top.load("data/fast.gif", {})
            tryVerify(() => w.moving, 4000)
            verify(w.frameInterval >= 33, "interval " + w.frameInterval)
            Registry.tasks.windows = [{ maximized: true }]
            Registry.tasks.dataChanged()
            tryVerify(() => !w.moving, 2000)
            Registry.tasks.windows = [{ maximized: true, minimized: true }]
            Registry.tasks.dataChanged()
            tryVerify(() => w.moving, 2000)
            Registry.power.data = { "Battery": { "Has Battery": true }, "AC Adapter": { "Plugged in": false } }
            tryVerify(() => !w.moving, 2000)
        }
    }

    TestCase {
        name: "Video"
        when: windowShown
        function test_plays_and_rests() {
            const w = top.load("../motion/raye-strata.webm", {})
            tryVerify(() => w.moving, 8000)
            Registry.tasks.windows = [{ fullScreen: true }]
            Registry.tasks.dataChanged()
            tryVerify(() => !w.moving, 2000)
            Registry.tasks.windows = []
            Registry.tasks.dataChanged()
            tryVerify(() => w.moving, 4000)
        }
        function test_long_rest_lets_the_video_go() {
            const w = top.load("../motion/raye-strata.webm",
                               { Still: Qt.resolvedUrl("../motion/raye-strata.jpg") }, 400)
            tryVerify(() => w.moving, 8000)
            Registry.tasks.windows = [{ maximized: true }]
            Registry.tasks.dataChanged()
            tryVerify(() => !w.moving, 2000)
            verify(!w.released, "let go before the delay")
            tryVerify(() => w.released, 3000)
            verify(w.stillShown)
            compare(String(w.playerSource), "")
            Registry.tasks.windows = []
            Registry.tasks.dataChanged()
            tryVerify(() => w.moving, 8000)
            verify(!w.released)
        }
        // no still picture: the last frame is grabbed first, and the
        // desktop can come back before the grab answers
        function test_late_grab_keeps_a_shown_desktop_moving() {
            const w = top.load("../motion/raye-strata.webm", {}, 400)
            tryVerify(() => w.moving, 8000)
            let answer = null
            w.grabFrame = done => { answer = done }
            Registry.tasks.windows = [{ maximized: true }]
            Registry.tasks.dataChanged()
            tryVerify(() => answer !== null, 3000)
            Registry.tasks.windows = []
            Registry.tasks.dataChanged()
            tryVerify(() => w.moving, 4000)
            answer("")
            wait(300)
            verify(!w.released)
            verify(w.moving)
            verify(String(w.playerSource) !== "")
        }
        function test_grab_then_let_go() {
            const w = top.load("../motion/raye-strata.webm", {}, 400)
            tryVerify(() => w.moving, 8000)
            let answer = null
            w.grabFrame = done => { answer = done }
            Registry.tasks.windows = [{ maximized: true }]
            Registry.tasks.dataChanged()
            tryVerify(() => answer !== null, 3000)
            answer("")
            verify(w.released)
            compare(String(w.playerSource), "")
        }
        function test_short_rest_keeps_the_video() {
            const w = top.load("../motion/raye-strata.webm", {}, 30000)
            tryVerify(() => w.moving, 8000)
            Registry.tasks.windows = [{ maximized: true }]
            Registry.tasks.dataChanged()
            tryVerify(() => !w.moving, 2000)
            wait(500)
            verify(!w.released)
            verify(String(w.playerSource) !== "")
        }
    }

    TestCase {
        name: "GifRelease"
        when: windowShown
        function test_long_rest_lets_the_gif_go() {
            const w = top.load("data/fast.gif", {}, 400)
            tryVerify(() => w.moving, 4000)
            Registry.tasks.windows = [{ maximized: true }]
            Registry.tasks.dataChanged()
            tryVerify(() => w.released, 3000)
            compare(String(w.gifSource), "")
            Registry.tasks.windows = []
            Registry.tasks.dataChanged()
            tryVerify(() => w.moving, 4000)
        }
    }

    TestCase {
        name: "Screen"
        function test_watches_its_own_screen() {
            const w = top.load("data/fast.gif", {})
            compare(Registry.tasks.screenGeometry, Qt.rect(1280, 0, 1920, 1080))
        }
    }
}
