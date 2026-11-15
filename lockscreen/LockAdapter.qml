// What the Raye lock screen takes from the running Plasma session: switch
// user and power through the session manager, Caps Lock, keyboard
// layouts, battery and network state, the playing media, the session's
// on-screen keyboard, the animation speed and Raye Update's ready set.
//
// Each piece comes from its own Plasma QML module and is made at run time,
// so a module that is missing or fails only switches its own feature off;
// the lock screen itself never fails to load because of it (a lock screen
// that fails to load falls back to the greeter's plain one, still locked).
//
// `active: false` makes nothing (tests).
import QtQuick

Item {
    id: adapter

    property bool active: true

    // --- out ---------------------------------------------------------
    readonly property bool canSwitchUser: sessions ? sessions.canSwitchUser === true : false
    readonly property bool canShutDown: sessions ? sessions.canShutdown === true : false
    readonly property bool canRestart: sessions ? sessions.canReboot === true : false

    readonly property bool capsLock: caps ? caps.locked === true : false

    readonly property var layouts: layout && layout.layoutsList ? layout.layoutsList : []
    readonly property bool hasMultipleLayouts: layouts.length > 1
    readonly property string layoutShortName: {
        if (!layout || layouts.length === 0)
            return ""
        var l = layouts[layout.layout]
        return l && l.shortName ? l.shortName : ""
    }

    readonly property bool hasBattery: battery ? battery.hasInternalBatteries === true : false
    readonly property real batteryPercent: battery ? battery.percent : 0
    readonly property bool pluggedIn: battery ? battery.pluggedIn === true : false

    // the network applet's icon name ("network-wireless-60", ...)
    readonly property string networkIconName: network && network.connectionIcon
                                              ? network.connectionIcon : ""

    // MPRIS: one row while something can play
    readonly property var mediaModel: media

    // the session's own on-screen keyboard (Wayland)
    readonly property bool sessionKeyboardAvailable: kwinKeyboard ? kwinKeyboard.available === true : false
    readonly property bool sessionKeyboardVisible: kwinKeyboard ? kwinKeyboard.visible === true : false

    // the user's animation speed is "instant"
    readonly property bool reducedMotion: units ? units.reduced === true : false

    // Raye Update has a set to install at the next restart, and this user
    // arms it without a password (a prompt cannot show over the lock)
    readonly property bool updatesReady: updates ? updates.ready === true : false

    signal aboutToSuspend()
    // arming did not happen: "cancelled", "nothing-ready", "busy", "failed"
    signal updateArmFailed(string reason)

    function switchUser() {
        if (sessions && canSwitchUser)
            sessions.switchUser()
    }
    // no question asked, like Windows' sign-in screen: the logout prompt
    // cannot show over a locked session, so asking would do nothing
    function shutDown() {
        if (sessions && canShutDown)
            sessions.shutDownNow()
    }
    function restart() {
        if (sessions && canRestart)
            sessions.restartNow()
    }
    // then: "restart" or "shutdown"; on success Raye Update restarts
    function armUpdates(then) {
        if (updates)
            updates.arm(then)
        else
            updateArmFailed("failed")
    }
    function nextLayout() {
        if (layout)
            layout.switchToNextLayout()
    }
    function showSessionKeyboard(on) {
        if (kwinKeyboard)
            kwinKeyboard.show(on)
    }

    // --- the pieces --------------------------------------------------
    // Power without a prompt. The confirmation mode is passed by name: its
    // numbers are not the obvious ones (Skip is 0, ForcePrompt is 1).
    readonly property string sessionsSource:
        "import org.kde.plasma.private.sessions\n"
        + "SessionManagement {\n"
        + "  function shutDownNow() { requestShutdown(SessionManagement.Skip) }\n"
        + "  function restartNow() { requestReboot(SessionManagement.Skip) }\n"
        + "}"

    readonly property string updatesSource:
        "import QtQuick\nimport org.raye.shell\n"
        + "QtObject {\n"
        + "  readonly property bool ready: UpdateState.restartPending\n"
        + "                                && UpdateState.armsWithoutPassword\n"
        + "  signal failed(string reason)\n"
        + "  Component.onCompleted: UpdateState.armFailed.connect(failed)\n"
        + "  function arm(then) {\n"
        + "    if (then === 'shutdown') UpdateState.updateAndShutdown()\n"
        + "    else UpdateState.updateAndRestart()\n"
        + "  }\n"
        + "}"

    property QtObject sessions: null
    property QtObject updates: null
    property QtObject caps: null
    property QtObject layout: null
    property QtObject battery: null
    property QtObject network: null
    property QtObject media: null
    property QtObject kwinKeyboard: null
    property QtObject units: null

    function make(source) {
        try {
            return Qt.createQmlObject(source, adapter, "LockAdapter")
        } catch (e) {
            return null
        }
    }

    Connections {
        target: adapter.sessions
        ignoreUnknownSignals: true
        function onAboutToSuspend() {
            adapter.aboutToSuspend()
        }
    }

    Connections {
        target: adapter.updates
        ignoreUnknownSignals: true
        function onFailed(reason) {
            adapter.updateArmFailed(reason)
        }
    }

    Component.onCompleted: {
        if (!active)
            return
        sessions = make(sessionsSource)
        updates = make(updatesSource)
        caps = make("import QtQuick\nimport org.kde.plasma.private.keyboardindicator\n"
                    + "KeyState { key: Qt.Key_CapsLock }")
        layout = make("import org.kde.plasma.workspace.keyboardlayout\nKeyboardLayout {}")
        battery = make("import org.kde.plasma.private.battery\nBatteryControlModel {}")
        network = make("import org.kde.plasma.networkmanagement\nConnectionIcon {}")
        media = make("import org.kde.plasma.private.mpris\nMultiplexerModel {}")
        kwinKeyboard = make("import QtQuick\nimport org.kde.plasma.workspace.keyboardlayout as K\n"
                            + "QtObject {\n"
                            + "  readonly property bool available: K.KWinVirtualKeyboard.available\n"
                            + "  readonly property bool visible: K.KWinVirtualKeyboard.visible\n"
                            + "  function show(on) {\n"
                            + "    if (on) K.KWinVirtualKeyboard.enabled = true\n"
                            + "    K.KWinVirtualKeyboard.active = on\n"
                            + "  }\n"
                            + "}")
        units = make("import QtQuick\nimport org.kde.kirigami as K\n"
                     + "QtObject { readonly property bool reduced: K.Units.longDuration <= 1 }")
    }
}
