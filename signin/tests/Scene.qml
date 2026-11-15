// One screen of the gallery, composed the way the greeters compose it.
import QtQuick
import org.raye.signin

Item {
    id: scene

    property string view: "signin"
    property bool openFlyout: false

    readonly property var users: [
        { name: "alex", realName: "Alex Martin", icon: "" },
        { name: "camille", realName: "Camille Roy", icon: "" },
        { name: "sam", realName: "Sam Lee", icon: "" }
    ]
    readonly property bool isStatus: ["welcome", "signingOut", "shuttingDown", "restarting",
                                      "updating", "firstSignIn"].indexOf(view) >= 0
    readonly property bool isSignIn: !isStatus && view !== "lock" && view !== "security"
    readonly property bool lockScreen: view === "lockscreen"

    LockView {
        anchors.fill: parent
        visible: scene.view === "lock"
        shown: scene.view === "lock"
        now: new Date(2026, 9, 29, 9, 41)
        clockRunning: false
        networkIcon: "network-wireless-3"
        batteryIcon: "battery-080"
    }

    Backdrop {
        anchors.fill: parent
        visible: scene.isSignIn
        blurred: true
    }

    // what Ctrl+Alt+Del shows. "Change a password" and "Task Manager"
    // are refused here, which is their shipped state: the first has no
    // safe path in this repo, the second is a separate application.
    SecurityScreen {
        anchors.fill: parent
        visible: scene.view === "security"
        shown: scene.view === "security"
        userName: "Alex Martin"
        networkIcon: "network-wireless-3"
        layoutCode: Strings.language === "fr" ? "FR" : "US"
        showLayout: true
    }

    SignInColumn {
        id: column
        anchors.fill: parent
        visible: scene.isSignIn
        userName: scene.lockScreen ? "Alex Martin" : scene.users[list.currentIndex].realName
        needsPassword: scene.view !== "passwordless"
        capsLock: scene.view === "caps"
        methodHint: scene.lockScreen ? Strings.tr.fingerprint : ""
        showSwitchUser: scene.lockScreen
        busy: scene.view === "busy"
        bottomInset: keyboard.occupied
        Component.onCompleted: {
            if (scene.view === "error")
                fail()
            if (scene.view === "typed" || scene.view === "busy")
                passwordField.text = "secret"
        }
    }

    UserList {
        id: list
        x: RayePalette.listEdge
        anchors.bottom: parent.bottom
        anchors.bottomMargin: RayePalette.listEdge
        visible: scene.isSignIn && !scene.lockScreen && count > 1
        model: scene.users
        currentIndex: 0
    }

    OnScreenKeyboard {
        id: keyboard
        active: scene.view === "keyboard"
    }

    CornerBar {
        id: bar
        visible: scene.isSignIn
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: RayePalette.edge
        anchors.bottomMargin: RayePalette.edge + keyboard.occupied
        networkIcon: "network-wired"
        layoutCode: Strings.language === "fr" ? "FR" : "US"
        showLayout: true
        updatePending: scene.view === "power"
        keyboardOn: keyboard.active
        keyboardAvailable: keyboard.available
    }

    StatusScreen {
        anchors.fill: parent
        visible: scene.isStatus
        mode: scene.isStatus ? scene.view : "welcome"
        progress: 37
    }

    Timer {
        interval: 600
        running: scene.openFlyout
        onTriggered: {
            if (scene.view === "power")
                bar.powerFlyout.open()
            else if (scene.view === "access")
                bar.accessFlyout.open()
            else if (scene.view === "network")
                bar.networkFlyout.open()
        }
    }
}
