// The sign-in screen with stand-ins for SDDM, for looking at every state
// without a login manager (test mode cannot shut down, and never answers
// a sign-in).
//
//   qml6 -I signin sddm/tests/preview.qml -- <state> [en|fr]
//
// states: lock, signin, typed, caps, error, message, welcome, power,
// access, network, keyboard, passwordless, noaccounts, secondary,
// highcontrast
import QtQuick
import QtQuick.Window
import org.raye.signin

Window {
    id: window

    readonly property var args: {
        var a = Qt.application.arguments
        var i = a.indexOf("--")
        return i >= 0 ? a.slice(i + 1) : []
    }
    readonly property string state: args.length > 0 ? args[0] : "signin"

    width: Screen.width
    height: Screen.height
    visible: true
    visibility: Window.FullScreen
    color: "black"

    Component.onCompleted: {
        if (args.length > 1)
            Strings.lang = args[1]
        RayePalette.assets = Qt.resolvedUrl("../../assets/signin/")
        RayePalette.wallpaperDir = Qt.resolvedUrl("../../wallpaper/Raye/contents/images/")
        RayePalette.eye = Qt.resolvedUrl("../../assets/logo/raye-eye.png")
        loader.setSource(Qt.resolvedUrl("../raye/Main.qml"), {
            greeter: greeter,
            users: state === "noaccounts" ? null : (state === "passwordless" ? kiosk : users),
            keyboardModel: keyboardModel,
            sessions: { lastIndex: 0 },
            themeConfig: { background: "" },
            primary: state !== "secondary",
            statusProbes: true
        })
    }

    QtObject {
        id: greeter
        property bool canPowerOff: true
        property bool canReboot: true
        property bool canSuspend: true
        property bool canHibernate: false
        signal loginFailed()
        signal loginSucceeded()
        signal informationMessage(string message)
        function login(user, password, session) {}
        function powerOff() {}
        function reboot() {}
        function suspend() {}
        function hibernate() {}
    }

    ListModel {
        id: users
        property int lastIndex: 0
        property string lastUser: "alex"
        ListElement { name: "alex"; realName: "Alex Martin"; icon: ""; needsPassword: true }
        ListElement { name: "camille"; realName: "Camille Roy"; icon: ""; needsPassword: true }
        ListElement { name: "sam"; realName: "Sam Lee"; icon: ""; needsPassword: true }
    }

    ListModel {
        id: kiosk
        property int lastIndex: 0
        property string lastUser: "kiosk"
        ListElement { name: "kiosk"; realName: "Kiosk"; icon: ""; needsPassword: false }
    }

    QtObject {
        id: keyboardModel
        property bool capsLock: window.state === "caps"
        property var layouts: [{ shortName: "fr", longName: "French" },
                               { shortName: "us", longName: "English (US)" }]
        property int currentLayout: Strings.language === "fr" ? 0 : 1
    }

    Loader {
        id: loader
        anchors.fill: parent
        focus: true
        onLoaded: {
            item.forceActiveFocus()
            if (window.state === "highcontrast")
                RayePalette.highContrast = true
            if (window.state !== "lock" && window.state !== "secondary")
                Qt.callLater(window.go)
        }
    }

    function go() {
        var main = loader.item
        main.showSignIn("")
        var bar = findByName(main, "cornerBar")
        var column = findByName(main, "signInColumn")
        switch (state) {
        case "typed":
            column.typeText("secret")
            break
        case "error":
            column.typeText("secret")
            main.submit("secret")
            greeter.loginFailed()
            break
        case "message":
            greeter.informationMessage(Strings.language === "fr"
                                       ? "Votre mot de passe a expiré."
                                       : "Your password has expired.")
            break
        case "welcome":
            main.submit("secret")
            greeter.loginSucceeded()
            break
        case "power":
            later.action = function () { bar.powerFlyout.open() }
            later.start()
            break
        case "access":
            later.action = function () { bar.accessFlyout.open() }
            later.start()
            break
        case "network":
            later.action = function () { bar.networkFlyout.open() }
            later.start()
            break
        case "keyboard":
            bar.keyboardToggled(true)
            break
        }
    }

    function findByName(item, name) {
        if (item.objectName === name)
            return item
        for (var i = 0; i < item.children.length; ++i) {
            var f = findByName(item.children[i], name)
            if (f)
                return f
        }
        return null
    }

    Timer {
        id: later
        property var action: null
        interval: 600
        onTriggered: action()
    }
}
