// The plain rules of the sign-in screen: status icons, keyboard layout
// code, which account and session come first, and the lock picture.
import QtQuick
import QtTest
import "../raye/status.js" as Status

TestCase {
    name: "Status"

    function test_networkWired_data() {
        return [
            { tag: "activated", input: "network-wired-activated" },
            { tag: "plain", input: "network-wired" },
            { tag: "vpn", input: "network-vpn" }
        ]
    }
    function test_networkWired(data) {
        compare(Status.networkIcon(data.input), "network-wired")
    }

    function test_networkWireless_data() {
        return [
            { tag: "connected-00", input: "network-wireless-connected-00", bars: 0 },
            { tag: "0-locked", input: "network-wireless-0-locked", bars: 0 },
            { tag: "connected-20", input: "network-wireless-connected-20", bars: 1 },
            { tag: "20", input: "network-wireless-20", bars: 1 },
            { tag: "40-locked", input: "network-wireless-40-locked", bars: 2 },
            { tag: "60", input: "network-wireless-60", bars: 3 },
            { tag: "80-limited", input: "network-wireless-80-limited", bars: 4 },
            { tag: "100-locked", input: "network-wireless-100-locked", bars: 4 },
            { tag: "connected-100", input: "network-wireless-connected-100", bars: 4 },
            { tag: "symbolic", input: "network-wireless-60-symbolic", bars: 3 },
            { tag: "mobile", input: "network-mobile-40-lte", bars: 2 },
            { tag: "mobile-plain", input: "network-mobile-100", bars: 4 }
        ]
    }
    function test_networkWireless(data) {
        compare(Status.networkIcon(data.input), "network-wireless-" + data.bars)
    }

    function test_networkNone_data() {
        return [
            { tag: "disconnect", input: "network-disconnect" },
            { tag: "wireless-off", input: "network-wireless-disconnected" },
            { tag: "wired-off", input: "network-wired-disconnected" },
            { tag: "unavailable", input: "network-unavailable" },
            { tag: "flight", input: "network-flightmode-on" },
            { tag: "connecting", input: "network-connect" },
            { tag: "mobile-available", input: "network-mobile-available" },
            { tag: "wireless-bare", input: "network-wireless" }
        ]
    }
    function test_networkNone(data) {
        compare(Status.networkIcon(data.input), "network-none")
    }

    function test_networkHidden_data() {
        return [
            { tag: "empty", input: "" },
            { tag: "undefined", input: undefined },
            { tag: "null", input: null },
            { tag: "not a network icon", input: "dialog-warning" },
            { tag: "bluetooth", input: "network-wireless-bluetooth" },
            { tag: "path", input: "../../etc/passwd" }
        ]
    }
    function test_networkHidden(data) {
        compare(Status.networkIcon(data.input), "")
    }

    function test_battery_data() {
        return [
            { tag: "no battery", has: false, percent: 50, plugged: false, icon: "" },
            { tag: "full", has: true, percent: 100, plugged: false, icon: "battery-100" },
            { tag: "charging", has: true, percent: 100, plugged: true, icon: "battery-100-charging" },
            { tag: "84 rounds down", has: true, percent: 84, plugged: false, icon: "battery-080" },
            { tag: "85 rounds up", has: true, percent: 85, plugged: false, icon: "battery-090" },
            { tag: "4 low", has: true, percent: 4, plugged: false, icon: "battery-000" },
            { tag: "5 is 10", has: true, percent: 5, plugged: true, icon: "battery-010-charging" },
            { tag: "over 100", has: true, percent: 130, plugged: false, icon: "battery-100" },
            { tag: "under 0", has: true, percent: -3, plugged: false, icon: "battery-000" },
            { tag: "not a number", has: true, percent: NaN, plugged: false, icon: "" }
        ]
    }
    function test_battery(data) {
        compare(Status.batteryIcon(data.has, data.percent, data.plugged), data.icon)
    }

    readonly property var twoLayouts: [{ shortName: "fr", longName: "French" },
                                       { shortName: "us", longName: "English (US)" }]

    function test_layoutCode() {
        compare(Status.layoutCode(twoLayouts, 0), "FR")
        compare(Status.layoutCode(twoLayouts, 1), "US")
        compare(Status.layoutCode(twoLayouts, 2), "")
        compare(Status.layoutCode(twoLayouts, -1), "")
        compare(Status.layoutCode([], 0), "")
        compare(Status.layoutCode(undefined, 0), "")
        // a variant ("us(intl)") keeps only the layout, three letters at most
        compare(Status.layoutCode([{ shortName: "us(intl)" }], 0), "US")
        compare(Status.layoutCode([{ shortName: "" , longName: "Dvorak" }], 0), "")
    }

    function test_showLayout() {
        verify(Status.showLayout(twoLayouts))
        verify(!Status.showLayout([twoLayouts[0]]))
        verify(!Status.showLayout([]))
        verify(!Status.showLayout(null))
    }

    function test_nextLayout() {
        compare(Status.nextLayout(2, 0), 1)
        compare(Status.nextLayout(2, 1), 0)
        compare(Status.nextLayout(3, 7), 0)
        compare(Status.nextLayout(0, 0), -1)
        compare(Status.nextLayout(1, 0), 0)
    }

    function test_indexInRange() {
        compare(Status.indexInRange(1, 3), 1)
        compare(Status.indexInRange(-1, 3), 0)
        compare(Status.indexInRange(3, 3), 0)
        compare(Status.indexInRange(0, 0), -1)
        compare(Status.indexInRange(undefined, 2), 0)
    }

    function test_firstUser() {
        var users = [{ name: "alex" }, { name: "camille" }, { name: "sam" }]
        // the last user's name wins over a stale index
        compare(Status.firstUser(users, 0, "sam"), 2)
        compare(Status.firstUser(users, 1, ""), 1)
        compare(Status.firstUser(users, 9, "nobody"), 0)
        compare(Status.firstUser([], 0, "sam"), -1)
    }

    function test_displayName() {
        compare(Status.displayName({ name: "alex", realName: "Alex Martin" }), "Alex Martin")
        compare(Status.displayName({ name: "alex", realName: "" }), "alex")
        compare(Status.displayName({ name: "alex", realName: "  " }), "alex")
        compare(Status.displayName(null), "")
    }

    function test_picture() {
        compare(Status.picture(""), "")
        compare(Status.picture("   "), "")
        compare(Status.picture(undefined), "")
        compare(Status.picture("/srv/pictures/lake.jpg"), "file:///srv/pictures/lake.jpg")
        compare(Status.picture("file:///srv/pictures/lake.jpg"), "file:///srv/pictures/lake.jpg")
        // only local files: the greeter never fetches anything
        compare(Status.picture("https://example.com/a.png"), "")
        compare(Status.picture("relative/a.png"), "")
        // the stock picture of the other theme is not Raye's
        compare(Status.picture("/usr/share/wallpapers/Next/contents/images/5120x2880.png"), "")
    }

    // SDDM's session list: [{ file, name }] in model order
    readonly property var both: [
        { file: "/usr/share/wayland-sessions/sway.desktop", name: "Sway" },
        { file: "/usr/share/wayland-sessions/raye-hyprland.desktop", name: "Raye (Hyprland)" },
        { file: "/usr/share/wayland-sessions/plasma.desktop", name: "Plasma (Wayland)" },
        { file: "/usr/share/xsessions/plasmax11.desktop", name: "Plasma (X11)" }
    ]

    function test_sessionKey() {
        compare(Status.sessionKey("/usr/share/wayland-sessions/plasma.desktop"), "plasma")
        compare(Status.sessionKey("raye-hyprland.desktop"), "hyprland")
        compare(Status.sessionKey("/usr/local/share/wayland-sessions/raye-hyprland.desktop"), "hyprland")
        compare(Status.sessionKey("/usr/share/wayland-sessions/hyprland.desktop"), "")
        compare(Status.sessionKey("/usr/share/xsessions/plasmax11.desktop"), "")
        compare(Status.sessionKey(""), "")
        compare(Status.sessionKey(undefined), "")
    }

    function test_sessionChoicesAreRayesOnly() {
        var c = Status.sessionChoices(both)
        compare(c.length, 2)
        // Plasma first, whatever the order SDDM read the files in
        compare(c[0].key, "plasma")
        compare(c[0].index, 2)
        compare(c[1].key, "hyprland")
        compare(c[1].index, 1)
        compare(c[1].file, "raye-hyprland.desktop")
    }

    function test_sessionChoicesOne() {
        var c = Status.sessionChoices([both[2], both[3]])
        compare(c.length, 1)
        compare(c[0].key, "plasma")
        compare(Status.sessionChoices([]).length, 0)
        compare(Status.sessionChoices(null).length, 0)
    }

    function test_pickSession() {
        var c = Status.sessionChoices(both)
        // what this account used last time
        compare(Status.pickSession(c, "raye-hyprland.desktop", 2).key, "hyprland")
        // never used: SDDM's last session, when it is one of Raye's
        compare(Status.pickSession(c, "", 1).key, "hyprland")
        compare(Status.pickSession(c, "", 2).key, "plasma")
        // a remembered session that is gone
        compare(Status.pickSession(c, "gnome.desktop", 1).key, "hyprland")
        // SDDM's last is another desktop: leave it to SDDM
        compare(Status.pickSession(c, "", 0), null)
        compare(Status.pickSession(c, "", -1).key, "plasma")
        compare(Status.pickSession([], "raye-hyprland.desktop", 1), null)
    }
}
