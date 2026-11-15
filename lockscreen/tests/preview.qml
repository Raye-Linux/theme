// The Raye lock screen with stand-ins for the greeter and the session, to
// look at every state without locking anything.
//
//   qml6 -I <repo>/signin preview.qml -- --view <name> [--lang fr]
//        [--contrast] [--larger]
//
// Views: lock (media playing, battery), signin (caps lock, fingerprint
// line, switch user, power), wrong, power (flyout open), passwordless,
// layouts (two keyboard layouts), retry (finger not recognised),
// updates (power flyout with Raye Update's rows).
import QtQuick
import QtQuick.Window
import org.raye.signin
import ".."

Window {
    id: window

    function arg(name, fallback) {
        var a = Qt.application.arguments
        var i = a.indexOf(name)
        return i >= 0 && i + 1 < a.length ? a[i + 1] : fallback
    }
    function flag(name) {
        return Qt.application.arguments.indexOf(name) >= 0
    }

    readonly property string view: arg("--view", "lock")
    property bool ready: false

    visible: true
    visibility: Window.FullScreen
    width: Screen.width
    height: Screen.height
    color: "#000000"
    title: "Raye lock screen preview"

    Component.onCompleted: {
        RayePalette.assets = Qt.resolvedUrl("../../assets/signin/")
        RayePalette.wallpaperDir = Qt.resolvedUrl("../../wallpaper/Raye/contents/images/")
        RayePalette.eye = Qt.resolvedUrl("../../assets/logo/raye-eye.png")
        Strings.lang = arg("--lang", "")
        RayePalette.highContrast = flag("--contrast")
        RayePalette.largerText = flag("--larger")
        Motion.reducedMotion = true
        ready = true
        stage.start()
    }

    QtObject {
        id: auth
        property bool busy: false
        property bool hadPrompt: true
        property string prompt: ""
        property string promptForSecret: ""
        property string infoMessage: ""
        property string errorMessage: ""
        property int authenticatorTypes: 0
        signal succeeded()
        signal failed(int kind, var authenticator)
        signal noninteractiveInfo(int kind, var authenticator)
        signal noninteractiveError(int kind, var authenticator)
        function startAuthenticating() {}
        function respond(p) {}
    }

    ListModel {
        id: players
        ListElement {
            track: "Midnight Signals"
            artist: "The Violet Hour"
            identity: "Music"
            artUrl: ""
            playbackStatus: 2
            canControl: true
            canGoPrevious: true
            canGoNext: true
        }
    }

    QtObject {
        id: session
        property bool canSwitchUser: true
        property bool canShutDown: true
        property bool canRestart: true
        property bool capsLock: window.view === "signin"
        property bool hasMultipleLayouts: window.view === "layouts"
        property string layoutShortName: "fr"
        property bool hasBattery: true
        property real batteryPercent: 72
        property bool pluggedIn: false
        property string networkIconName: "network-wireless-80"
        property var mediaModel: players
        property bool sessionKeyboardAvailable: false
        property bool sessionKeyboardVisible: false
        property bool reducedMotion: true
        property bool updatesReady: window.view === "updates"
        signal aboutToSuspend()
        signal updateArmFailed(string reason)
        function armUpdates(then) {}
        function switchUser() {}
        function shutDown() {}
        function restart() {}
        function nextLayout() {}
        function showSessionKeyboard(on) {}
    }

    LockUi {
        id: ui
        anchors.fill: parent
        focus: true
        visible: window.ready
        authenticator: auth
        adapter: session
        userName: "Anna Roy"
        canSuspend: true
        canHibernate: false
        Component.onCompleted: lockView.clockRunning = false
    }

    // puts the screen in the asked state once everything is laid out
    Timer {
        id: stage
        interval: 300
        onTriggered: {
            ui.lockView.now = new Date(2026, 9, 29, 9, 41)
            var v = window.view
            if (v === "lock")
                return
            ui.openSignIn("")
            if (v === "signin" || v === "retry" || v === "power" || v === "updates") {
                auth.authenticatorTypes = 1
                auth.authenticatorTypesChanged()
                auth.noninteractiveInfo(1, null)
                ui.column.passwordField.insert("secret")
            }
            if (v === "retry")
                auth.noninteractiveError(1, null)
            if (v === "wrong") {
                auth.promptForSecretChanged()
                ui.submit("x")
                auth.failed(0, null)
            }
            if (v === "passwordless") {
                auth.hadPrompt = false
                auth.succeeded()
            }
            if (v === "power" || v === "updates")
                ui.cornerBar.powerFlyout.open()
        }
    }
}
