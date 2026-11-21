import QtQuick
import QtTest

import "../shell/qml/hyprlogic.js" as Logic
// the Plasma clock's own rule, so the two taskbars never disagree
import "../../shell/plasmoids/org.raye.clock/contents/ui/clocklogic.js" as Clock

TestCase {
    name: "HyprLogic"

    function client(address, cls, title, ws, extra) {
        const c = { address: address, class: cls, initialClass: cls, title: title,
                    workspace: ws, mapped: true, hidden: false, pid: 100, urgent: false }
        for (const k in (extra || {}))
            c[k] = extra[k]
        return c
    }
    readonly property var ws1: ({ id: 1, name: "1" })
    readonly property var minWs: ({ id: -98, name: "special:raye-minimized" })
    readonly property var scratch: ({ id: -99, name: "special:magic" })
    readonly property var files: ({ id: "org.kde.dolphin.desktop", name: "Files", icon: "system-file-manager" })
    readonly property var web: ({ id: "firefox.desktop", name: "Firefox", icon: "firefox" })

    function test_one_taskbar_per_output() {
        const a = { name: "eDP-1" }, b = { name: "HDMI-A-1" }
        const got = Logic.uniqueScreens([a, b, { name: "eDP-1" }])
        compare(got.length, 2)
        compare(got[0], a)
        compare(got[1], b)
        compare(Logic.uniqueScreens([]).length, 0)
        compare(Logic.uniqueScreens(null).length, 0)
    }

    function test_pinned_app_without_window_is_a_launcher() {
        const rows = Logic.taskRows([], "", ["org.kde.dolphin.desktop"], {}, true)
        compare(rows.length, 1)
        verify(rows[0].IsLauncher)
        compare(rows[0].LauncherUrlWithoutIcon, "applications:org.kde.dolphin.desktop")
        compare(rows[0].ChildCount, 0)
    }

    function test_window_joins_its_pin() {
        const rows = Logic.taskRows([client("0xa", "org.kde.dolphin", "Home", ws1)], "0xa",
                                    ["org.kde.dolphin.desktop"], { "0xa": files }, true)
        compare(rows.length, 1)
        verify(!rows[0].IsLauncher)
        verify(rows[0].IsActive)
        verify(rows[0].HasLauncher)
        compare(rows[0].display, "Home")
        compare(rows[0].decoration, "system-file-manager")
    }

    function test_two_windows_group_with_child_count() {
        const cs = [client("0xa", "firefox", "One", ws1), client("0xb", "firefox", "Two", ws1)]
        const apps = { "0xa": web, "0xb": web }
        const grouped = Logic.taskRows(cs, "0xb", [], apps, true)
        compare(grouped.length, 1)
        compare(grouped[0].ChildCount, 2)
        compare(grouped[0].display, "Firefox")
        const separate = Logic.taskRows(cs, "0xb", [], apps, false)
        compare(separate.length, 2)
        verify(!separate[0].IsActive)
        verify(separate[1].IsActive)
    }

    function test_unpinned_follow_pins_in_order_seen() {
        const cs = [client("0x2", "firefox", "Web", ws1), client("0x1", "org.kde.konsole", "sh", ws1)]
        const rows = Logic.taskRows(cs, "", ["org.kde.dolphin.desktop"], { "0x2": web }, true,
                                    { "0x1": 1, "0x2": 2 })
        compare(rows.map(r => r.AppId), ["org.kde.dolphin", "org.kde.konsole", "firefox"])
    }

    function test_minimized_window_keeps_button() {
        const rows = Logic.taskRows([client("0xa", "firefox", "Web", minWs)], "", [], { "0xa": web }, true)
        compare(rows.length, 1)
        verify(rows[0].IsMinimized)
    }

    function test_scratchpad_and_unmapped_windows_are_not_shown() {
        const rows = Logic.taskRows([client("0xa", "firefox", "Web", scratch),
                                     client("0xb", "firefox", "Web", ws1, { mapped: false })], "", [], {}, true)
        compare(rows.length, 0)
    }

    function test_urgent_window_asks_for_attention() {
        const rows = Logic.taskRows([client("0xa", "firefox", "Web", ws1, { urgent: true })], "", [], {}, true)
        verify(rows[0].IsDemandingAttention)
    }

    function test_activate_brings_minimized_back_first() {
        compare(Logic.activateCommands({ address: "0xa", minimized: true }, 3),
                ["dispatch movetoworkspacesilent 3,address:0xa",
                 "dispatch focuswindow address:0xa",
                 "dispatch alterzorder top,address:0xa"])
        compare(Logic.activateCommands({ address: "0xa", minimized: false }, 3).length, 2)
    }

    function test_minimize_and_close() {
        compare(Logic.minimizeCommands({ address: "0xa" }),
                ["dispatch movetoworkspacesilent special:raye-minimized,address:0xa"])
        compare(Logic.closeCommands({ address: "0xa" }), ["dispatch closewindow address:0xa"])
    }

    function test_batch() {
        compare(Logic.batch(["dispatch a"]), "dispatch a")
        compare(Logic.batch(["dispatch a", "dispatch b"]), "[[BATCH]]dispatch a;dispatch b")
    }

    function test_show_desktop_round_trip() {
        const cs = [client("0xa", "a", "", ws1), client("0xb", "b", "", { id: 2, name: "2" }),
                    client("0xc", "c", "", minWs)]
        const away = Logic.showDesktop(cs, 1, [])
        compare(away.hidden, ["0xa"])
        compare(away.commands, ["dispatch movetoworkspacesilent special:raye-minimized,address:0xa"])
        const back = Logic.showDesktop(cs, 1, away.hidden)
        compare(back.hidden, [])
        compare(back.commands, ["dispatch movetoworkspacesilent 1,address:0xa"])
    }

    function test_work_area_leaves_out_the_taskbar() {
        const a = Logic.workArea({ x: 0, y: 0, width: 2560, height: 1600, scale: 2, transform: 0,
                                   reserved: [0, 0, 0, 48] })
        compare(a.width, 1280)
        compare(a.height, 752)
    }

    function test_snap_steps_follow_windows() {
        compare(Logic.snapStep("", "left"), { mode: "left", action: "snap" })
        compare(Logic.snapStep("left", "up"), { mode: "top-left", action: "snap" })
        compare(Logic.snapStep("top-left", "down"), { mode: "left", action: "snap" })
        compare(Logic.snapStep("left", "right"), { mode: "", action: "restore" })
        compare(Logic.snapStep("", "up"), { mode: "maximized", action: "maximize" })
        compare(Logic.snapStep("maximized", "down"), { mode: "", action: "restore" })
        compare(Logic.snapStep("", "down"), { mode: "", action: "minimize" })
        compare(Logic.snapStep("maximized", "up"), { mode: "maximized", action: "none" })
    }

    function test_snap_commands_place_the_half() {
        const area = { x: 0, y: 0, width: 1280, height: 752 }
        const cmds = Logic.snapCommands({ mode: "right", action: "snap" }, { address: "0xa" }, area, null, false)
        compare(cmds, ["dispatch setfloating address:0xa",
                       "dispatch resizewindowpixel exact 640 752,address:0xa",
                       "dispatch movewindowpixel exact 640 0,address:0xa"])
        const fromMax = Logic.snapCommands({ mode: "left", action: "snap" }, { address: "0xa" }, area, null, true)
        compare(fromMax[0], "dispatch fullscreenstate 0 -1")
        const restore = Logic.snapCommands({ mode: "", action: "restore" }, { address: "0xa" }, area,
                                           { x: 10, y: 20, width: 800, height: 600 }, false)
        compare(restore[2], "dispatch movewindowpixel exact 10 20,address:0xa")
    }

    function test_events() {
        compare(Logic.parseEvent("openwindow>>a,1,firefox,Web"), { name: "openwindow", data: "a,1,firefox,Web" })
        compare(Logic.parseEvent("junk"), null)
        verify(Logic.changesClients("closewindow"))
        verify(!Logic.changesClients("activelayout"))
    }

    function test_window_of_a_title_bar() {
        const a = client("0xa", "org.kde.dolphin", "Home", ws1, { pid: 200 })
        const b = client("0xb", "org.kde.dolphin", "Pictures", ws1, { pid: 200 })
        const c = client("0xc", "konsole", "Home", ws1, { pid: 300 })
        const all = [a, b, c]
        // the app's window with that title
        compare(Logic.windowOf(all, 200, "Pictures", "0xc").address, "0xb")
        // two with the same title: the one with the focus
        const b2 = client("0xd", "org.kde.dolphin", "Home", ws1, { pid: 200 })
        compare(Logic.windowOf([a, b2, c], 200, "Home", "0xd").address, "0xd")
        // a title that changed on the way: the app's focused window
        compare(Logic.windowOf(all, 200, "Downloads", "0xa").address, "0xa")
        // never another app's window, and nothing for an unknown app
        compare(Logic.windowOf(all, 200, "Downloads", "0xc"), null)
        compare(Logic.windowOf(all, 999, "Home", "0xc"), null)
        // a window already put away is not picked again
        const gone = client("0xe", "org.kde.dolphin", "Music", minWs, { pid: 200 })
        compare(Logic.windowOf([gone], 200, "Music", ""), null)
    }

    function test_do_not_disturb_and_focus() {
        const now = new Date(2026, 10, 19, 10, 0, 0)
        verify(!Logic.dndActive(undefined, now, false))
        verify(!Logic.dndActive(new Date(2026, 10, 19, 9, 0, 0), now, false))
        verify(Logic.dndActive(new Date(2126, 0, 1), now, false))
        verify(Logic.dndActive(undefined, now, true))
        compare(Logic.focusRemaining(new Date(NaN), now), 0)
        compare(Logic.focusRemaining(new Date(2026, 10, 19, 10, 25, 0), now), 1500)
        compare(Logic.focusRemaining(new Date(2026, 10, 19, 9, 0, 0), now), 0)
    }

    // the taskbar clock on Hyprland: 12 or 24 hours as the language writes
    // its own clock, unless Settings forces one ("true"/"false")
    function test_clock_follows_the_locale() {
        verify(!Logic.use24h("auto", "h:mm AP"))
        verify(!Logic.use24h("auto", "h:mm ap"))
        verify(!Logic.use24h(undefined, "h:mm a"))
        verify(Logic.use24h("auto", "HH:mm"))
        verify(Logic.use24h("auto", "H 'h' mm"))
        // a quoted "a" is text, not the AM/PM field
        verify(Logic.use24h("auto", "HH 'am' mm"))
        verify(Logic.use24h("true", "h:mm AP"))
        verify(Logic.use24h(true, "h:mm AP"))
        verify(!Logic.use24h("false", "HH:mm"))
        verify(!Logic.use24h(false, "HH:mm"))
    }

    function test_clock_rule_is_the_plasma_clocks() {
        const settings = ["auto", "", undefined, "true", "false", true, false, "24", "12"]
        const formats = ["h:mm AP", "HH:mm", "h:mm a", "H:mm", "HH 'am' mm", "", Qt.locale("en_US").timeFormat(Locale.ShortFormat),
                         Qt.locale("de_DE").timeFormat(Locale.ShortFormat), Qt.locale("fr_CA").timeFormat(Locale.ShortFormat)]
        for (const s of settings)
            for (const f of formats)
                compare(Logic.use24h(s, f), Clock.use24h(s, f), "setting " + s + ", format " + f)
    }

    // what the shell hands the clock comes from that rule, never a test of its own
    function test_shell_uses_the_rule() {
        const xhr = new XMLHttpRequest()
        xhr.open("GET", Qt.resolvedUrl("../shell/qml/Shell.qml"), false)
        xhr.send()
        verify(/twentyFour:\s*Logic\.use24h\(ShellSettings\.clockUse24h,\s*Qt\.locale\(\)\.timeFormat\(Locale\.ShortFormat\)\)/.test(xhr.responseText),
               "Shell.qml does not take twentyFour from Logic.use24h")
    }
}
