// What ksmserver-logout-greeter shows, on every screen. Two uses, told
// apart by `sdtype`, which the greeter sets from the prompt it was asked for:
//
//   -1 (promptAll)  Ctrl+Alt+Del: the security screen, and no timer of any
//                   kind. Nothing happens until someone chooses.
//   the others      Start's Shut down, Restart, Sign out: Start was the
//                   question, so this asks nothing and shows the words.
//
// The numbers are KWorkSpace::ShutdownType (kworkspace.h).
//
// The greeter's own signals end its process on the spot, so a request
// sent through them takes the words down with it and leaves the desktop
// showing while the session closes. For that reason the session is asked
// from here, and the words stay up until the session takes them away.
// They may not stay for ever: an application that refuses to close asks
// its question behind this screen, so after holdLimit the screen leaves
// and the question can be answered. The signals remain for the update
// variants of a PackageKit update, which only the greeter knows how to
// arm, and for when the session module cannot be loaded. Raye Update's
// own updates are armed here, and Raye Update restarts the session.
import QtQuick
import org.raye.signin
import "."

Item {
    id: root

    // the names the greeter connects to
    signal logoutRequested()
    signal haltRequested()
    signal haltUpdateRequested()
    signal suspendRequested(int spdMethod)
    signal rebootRequested()
    signal rebootRequested2(int opt)
    signal rebootUpdateRequested()
    signal cancelRequested()
    signal lockScreenRequested()
    signal cancelSoftwareUpdateRequested()

    // the greeter's context, read safely
    readonly property int shutdownType: typeof sdtype !== "undefined" ? sdtype : -1
    readonly property bool mayShutDown: typeof maysd !== "undefined" ? maysd : true
    readonly property bool mayLogOut: typeof canLogout !== "undefined" ? canLogout : true
    readonly property bool packageKitWaiting: typeof softwareUpdatePending !== "undefined"
                                              ? softwareUpdatePending : false
    // Raye Update's downloaded set, for people who arm it without a
    // password: a password prompt would open behind this screen
    readonly property bool rayeUpdatesWaiting: updates !== null && updates.ready === true
    readonly property bool updatesWaiting: rayeUpdatesWaiting || packageKitWaiting
    readonly property var powerStates: typeof spdMethods !== "undefined" ? spdMethods : null

    // comes up over the desktop instead of cutting to it
    opacity: Motion.normal === 0 ? 1 : 0
    NumberAnimation on opacity { to: 1; duration: Motion.normal; easing.type: Motion.enter; running: Motion.normal > 0 }

    width: typeof screenGeometry !== "undefined" ? screenGeometry.width : 1280
    height: typeof screenGeometry !== "undefined" ? screenGeometry.height : 800

    // long enough for a normal session to be gone first
    property int holdLimit: 10000
    // give the application we just started the time to leave this process
    readonly property int launchGrace: 400

    // made at run time: a missing module only switches its own part off
    property QtObject sessions: null
    property QtObject launcher: null
    property QtObject user: null
    property QtObject network: null
    property QtObject keyboard: null
    property QtObject updates: null

    property bool taskManagerMissing: false
    readonly property bool canStart: launcher !== null

    readonly property bool ending: LogoutState.phase !== ""
    // how the session gets asked: "session", or a stock signal for updates
    property string route: "session"
    // this window asked Raye Update to arm, so its answer is ours
    property bool arming: false

    function make(source) {
        try {
            return Qt.createQmlObject(source, root, "Logout")
        } catch (e) {
            return null
        }
    }

    function phaseFor(type) {
        switch (type) {
        case 0:
        case 3: return "signingOut"
        case 1: return "restarting"
        case 2: return "shuttingDown"
        }
        return ""
    }

    // the network applet's icon name -> one of the sign-in icons
    function networkIcon(name) {
        if (/disconnect|offline|unavailable|limited|error/.test(name))
            return "network-none"
        if (/wireless|wifi|mobile/.test(name)) {
            var m = name.match(/-(\d{1,3})(?:-|$)/)
            var p = m ? parseInt(m[1], 10) : 0
            return "network-wireless-" + (p <= 0 ? 0 : p <= 20 ? 1 : p <= 40 ? 2 : p <= 60 ? 3 : 4)
        }
        return "network-wired"
    }

    function end(phase, how) {
        if (ending)
            return
        route = how || "session"
        LogoutState.phase = phase
    }

    // every window runs its own timers; only the first one asks
    function ask() {
        if (!LogoutState.claim())
            return
        switch (LogoutState.phase) {
        case "signingOut":
            if (sessions) sessions.logoutNow(); else logoutRequested()
            break
        case "restarting":
            if (route === "update" && rayeUpdatesWaiting) armUpdates("restart")
            else if (route === "update") rebootUpdateRequested()
            else powerNow("restarting")
            break
        case "shuttingDown":
            if (route === "update" && rayeUpdatesWaiting) armUpdates("shutdown")
            else if (route === "update") haltUpdateRequested()
            else powerNow("shuttingDown")
            break
        }
    }

    function powerNow(phase) {
        if (phase === "restarting") {
            if (sessions) sessions.restartNow(); else rebootRequested()
        } else {
            if (sessions) sessions.shutDownNow(); else haltRequested()
        }
    }

    function armUpdates(then) {
        arming = true
        updates.arm(then)
    }

    function updatesFailed(reason) {
        if (!arming)
            return
        arming = false
        if (reason === "cancelled") {
            // back to the choices: nothing was asked of the session yet
            askTimer.stop()
            holdTimer.stop()
            LogoutState.reset()
            security.forceActiveFocus()
            return
        }
        // nothing left to install, or it could not be armed: do what was
        // asked anyway, the updates wait for the next restart
        powerNow(LogoutState.phase)
    }

    function openTaskManager() {
        if (!launcher)
            return
        if (launcher.launch("org.raye.taskmanager.desktop")
                || launcher.launch("org.kde.plasma-systemmonitor.desktop")) {
            leaveSoon.start()
        } else {
            taskManagerMissing = true
        }
    }

    onEndingChanged: if (ending) { askTimer.start(); holdTimer.start() }

    Component.onCompleted: {
        sessions = make("import org.kde.plasma.private.sessions\n"
                        + "SessionManagement {\n"
                        + "  function shutDownNow() { requestShutdown(SessionManagement.Skip) }\n"
                        + "  function restartNow() { requestReboot(SessionManagement.Skip) }\n"
                        + "  function logoutNow() { requestLogout(SessionManagement.Skip) }\n"
                        + "}")
        launcher = make("import QtQuick\nimport org.raye.shell\n"
                        + "QtObject {\n"
                        + "  function launch(id) { return Launcher.launch(id) }\n"
                        + "  function openSettings(page) { return Launcher.openSettings(page) }\n"
                        + "}")
        user = make("import org.kde.coreaddons as K\nK.KUser {}")
        network = make("import org.kde.plasma.networkmanagement\nConnectionIcon {}")
        keyboard = make("import QtQuick\nimport org.kde.plasma.workspace.keyboardlayout as K\n"
                        + "QtObject {\n"
                        + "  readonly property bool available: K.KWinVirtualKeyboard.available\n"
                        + "  readonly property bool visible: K.KWinVirtualKeyboard.visible\n"
                        + "  function show(on) {\n"
                        + "    if (on) K.KWinVirtualKeyboard.enabled = true\n"
                        + "    K.KWinVirtualKeyboard.active = on\n"
                        + "  }\n"
                        + "}")
        updates = make("import QtQuick\nimport org.raye.shell\n"
                       + "QtObject {\n"
                       + "  readonly property bool ready: UpdateState.restartPending\n"
                       + "                                && UpdateState.armsWithoutPassword\n"
                       + "  signal failed(string reason)\n"
                       + "  Component.onCompleted: UpdateState.armFailed.connect(failed)\n"
                       + "  function arm(then) {\n"
                       + "    if (then === 'shutdown') UpdateState.updateAndShutdown()\n"
                       + "    else UpdateState.updateAndRestart()\n"
                       + "  }\n"
                       + "}")
        var units = make("import QtQuick\nimport org.kde.kirigami as K\n"
                         + "QtObject { readonly property bool reduced: K.Units.longDuration <= 1 }")
        if (units)
            Motion.reducedMotion = units.reduced

        var phase = phaseFor(shutdownType)
        if (phase !== "")
            end(phase)
        if (ending) {
            // a second window: the first one has already started the clock
            askTimer.start()
            holdTimer.start()
        }
        security.current = Math.max(0, security.firstEnabled(0, 1))
        security.forceActiveFocus()
    }

    Connections {
        target: root.updates
        ignoreUnknownSignals: true
        function onFailed(reason) { root.updatesFailed(reason) }
    }

    Timer {
        id: askTimer
        interval: Motion.progressDelay
        onTriggered: root.ask()
    }

    Timer {
        id: holdTimer
        interval: root.holdLimit
        onTriggered: root.cancelRequested()
    }

    Timer {
        id: leaveSoon
        interval: root.launchGrace
        onTriggered: root.cancelRequested()
    }

    SecurityScreen {
        id: security
        objectName: "security"
        anchors.fill: parent
        visible: !root.ending
        enabled: !emergency.visible

        userName: root.user ? (root.user.fullName || root.user.loginName || "") : ""
        userPicture: root.user && root.user.faceIconUrl ? root.user.faceIconUrl : ""

        canLock: root.sessions ? root.sessions.canLock === true : true
        canSwitchUser: root.sessions ? root.sessions.canSwitchUser === true : false
        canSignOut: root.mayLogOut
        canChangePassword: root.canStart
        canTaskManager: root.canStart && !root.taskManagerMissing

        networkIcon: root.network && root.network.connectionIcon
                     ? root.networkIcon(root.network.connectionIcon) : ""
        networkText: root.network && root.network.connected ? Strings.tr.connected : Strings.tr.notConnected

        canSleep: root.powerStates ? root.powerStates.SuspendState === true : false
        canHibernate: root.powerStates ? root.powerStates.HibernateState === true : false
        canShutDown: root.mayShutDown
        canRestart: root.mayShutDown
        updatePending: root.updatesWaiting
        keyboardOn: root.keyboard ? root.keyboard.visible : false
        keyboardAvailable: root.keyboard ? root.keyboard.available : false

        onLockRequested: root.lockScreenRequested()
        onSwitchUserRequested: {
            if (root.sessions)
                root.sessions.switchUser()
            root.cancelRequested()
        }
        onSignOutRequested: root.end("signingOut")
        onChangePasswordRequested: {
            if (root.launcher && root.launcher.openSettings("accounts/sign-in"))
                leaveSoon.start()
        }
        onTaskManagerRequested: root.openTaskManager()
        onCancelled: root.cancelRequested()

        onSleepRequested: root.suspendRequested(2)
        onHibernateRequested: root.suspendRequested(4)
        onShutDownRequested: root.end("shuttingDown")
        onRestartRequested: root.end("restarting")
        onUpdateAndShutDownRequested: root.end("shuttingDown", "update")
        onUpdateAndRestartRequested: root.end("restarting", "update")
        onEmergencyRestartRequested: emergency.open()
        onKeyboardToggled: on => { if (root.keyboard) root.keyboard.show(on) }
    }

    StatusScreen {
        id: status
        objectName: "status"
        anchors.fill: parent
        visible: root.ending
        mode: root.ending ? LogoutState.phase : "shuttingDown"
    }

    EmergencyCard {
        id: emergency
        objectName: "emergency"
        anchors.fill: parent
        onConfirmed: root.rebootRequested()
        onClosed: security.forceActiveFocus()
    }
}
