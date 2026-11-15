// The whole sign-in screen (Main.qml) with stand-ins for SDDM's objects:
// lock view -> sign-in view -> Welcome, wrong password, accounts, idle,
// power, keyboard layout, a screen that is not the main one.
import QtQuick
import QtTest
import org.raye.signin

TestCase {
    id: test
    name: "Main"
    width: 1280
    height: 800
    when: windowShown
    visible: true

    Component {
        id: greeterComponent
        QtObject {
            property bool canPowerOff: true
            property bool canReboot: true
            property bool canSuspend: true
            property bool canHibernate: false
            property var calls: []
            signal loginFailed()
            signal loginSucceeded()
            signal informationMessage(string message)
            function login(user, password, session) { calls.push(["login", user, password, session]) }
            function powerOff() { calls.push(["powerOff"]) }
            function reboot() { calls.push(["reboot"]) }
            function suspend() { calls.push(["suspend"]) }
            function hibernate() { calls.push(["hibernate"]) }
        }
    }

    Component {
        id: threeUsers
        ListModel {
            property int lastIndex: 1
            property string lastUser: "camille"
            ListElement { name: "alex"; realName: "Alex Martin"; icon: ""; needsPassword: true }
            ListElement { name: "camille"; realName: "Camille Roy"; icon: ""; needsPassword: true }
            ListElement { name: "kiosk"; realName: "Kiosk"; icon: ""; needsPassword: false }
        }
    }

    Component {
        id: oneUser
        ListModel {
            property int lastIndex: 0
            property string lastUser: "alex"
            ListElement { name: "alex"; realName: "Alex Martin"; icon: ""; needsPassword: true }
        }
    }

    Component {
        id: keyboardComponent
        QtObject {
            property bool capsLock: false
            property var layouts: [{ shortName: "fr", longName: "French" },
                                   { shortName: "us", longName: "English (US)" }]
            property int currentLayout: 0
        }
    }

    Component {
        id: mainComponent
        Loader {
            property var props: ({})
            anchors.fill: parent
            focus: true
            Component.onCompleted: setSource(Qt.resolvedUrl("../raye/Main.qml"), props)
        }
    }

    function make(extra, usersComponent) {
        var props = {
            greeter: createTemporaryObject(greeterComponent, test),
            users: createTemporaryObject(usersComponent || threeUsers, test),
            keyboardModel: createTemporaryObject(keyboardComponent, test),
            sessions: { lastIndex: 0 },
            themeConfig: { background: "" },
            primary: true,
            statusProbes: false
        }
        for (var k in extra)
            props[k] = extra[k]
        var loader = createTemporaryObject(mainComponent, test, { props: props })
        tryVerify(function () { return loader.status === Loader.Ready })
        var main = loader.item
        main.forceActiveFocus()
        tryVerify(function () { return main.ready })
        return main
    }

    function part(main, name) {
        var o = findChild(main, name)
        verify(o, name)
        return o
    }

    // the repo's pictures, as the installed ones
    function initTestCase() {
        RayePalette.assets = Qt.resolvedUrl("../../assets/signin/")
        RayePalette.wallpaperDir = Qt.resolvedUrl("../../wallpaper/Raye/contents/images/")
        RayePalette.eye = Qt.resolvedUrl("../../assets/logo/raye-eye.png")
    }

    function init() {
        RayePalette.highContrast = false
        RayePalette.largerText = false
    }

    function test_startsOnLockView() {
        var main = make({})
        compare(main.view, "lock")
        var lock = part(main, "lockView")
        verify(lock.shown)
        verify(lock.activeFocus)
        verify(!part(main, "signInColumn").enabled)
    }

    // the key that lifts the curtain is not typed; the next ones are
    function test_keyOnCurtainOnlyLiftsIt() {
        var main = make({})
        keyClick(Qt.Key_S, Qt.NoModifier)
        compare(main.view, "signin")
        var column = part(main, "signInColumn")
        compare(column.passwordField.text, "")
        verify(column.passwordField.inputItem.activeFocus)
        keyClick(Qt.Key_E)
        compare(column.passwordField.text, "e")
    }

    function test_spaceOnCurtainAddsNothing() {
        var main = make({})
        keyClick(Qt.Key_Space)
        compare(main.view, "signin")
        compare(part(main, "signInColumn").passwordField.text, "")
        compare(main.greeter.calls.length, 0)
    }

    function test_clickDismisses() {
        var main = make({})
        mouseClick(main, 600, 400)
        compare(main.view, "signin")
        var column = part(main, "signInColumn")
        compare(column.passwordField.text, "")
        verify(column.passwordField.inputItem.activeFocus)
    }

    function test_escapeGoesBack() {
        var main = make({})
        keyClick(Qt.Key_A)
        keyClick(Qt.Key_Escape)
        compare(main.view, "lock")
        compare(part(main, "signInColumn").passwordField.text, "")
        verify(part(main, "lockView").activeFocus)
    }

    function test_wrongPassword() {
        var main = make({})
        var column = part(main, "signInColumn")
        mouseClick(main, 600, 400)
        keyClick(Qt.Key_X)
        keyClick(Qt.Key_Return)
        var g = main.greeter
        compare(g.calls.length, 1)
        compare(JSON.stringify(g.calls[0]), JSON.stringify(["login", "camille", "x", 0]))
        verify(column.busy)
        g.loginFailed()
        verify(!column.busy)
        verify(column.failed)
        compare(column.passwordField.text, "")
        compare(main.view, "signin")
        // it can be tried again
        keyClick(Qt.Key_Y)
        keyClick(Qt.Key_Return)
        compare(g.calls.length, 2)
    }

    function test_welcome() {
        var main = make({})
        mouseClick(main, 600, 400)
        keyClick(Qt.Key_X)
        keyClick(Qt.Key_Return)
        main.greeter.loginSucceeded()
        compare(main.view, "welcome")
        var status = part(main, "statusScreen")
        compare(status.mode, "welcome")
        tryCompare(status, "opacity", 1)
        tryCompare(part(main, "signInColumn"), "opacity", 0)
        verify(!part(main, "cornerBar").visible || part(main, "cornerBar").opacity === 0)
    }

    function test_userList() {
        var main = make({})
        var list = part(main, "userList")
        // not on the lock view
        verify(!list.visible)
        mouseClick(main, 600, 400)
        verify(list.visible)
        compare(list.count, 3)
        compare(list.currentIndex, 1)
        var column = part(main, "signInColumn")
        compare(column.userName, "Camille Roy")
        keyClick(Qt.Key_Q)
        list.userSelected(0)
        compare(column.userName, "Alex Martin")
        // a new account starts with an empty field
        compare(column.passwordField.text, "")
        verify(column.passwordField.inputItem.activeFocus)
    }

    function test_oneUserNoList() {
        var main = make({}, oneUser)
        verify(!part(main, "userList").visible)
        compare(part(main, "signInColumn").userName, "Alex Martin")
    }

    function test_passwordless() {
        var main = make({})
        part(main, "userList").userSelected(2)
        mouseClick(main, 600, 400)
        var column = part(main, "signInColumn")
        verify(!column.needsPassword)
        keyClick(Qt.Key_Return)
        compare(JSON.stringify(main.greeter.calls[0]), JSON.stringify(["login", "kiosk", "", 0]))
    }

    function test_capsLock() {
        var main = make({})
        main.keyboardModel.capsLock = true
        verify(part(main, "signInColumn").capsLock)
    }

    function test_message() {
        var main = make({})
        main.greeter.informationMessage("Your password has expired")
        compare(part(main, "signInColumn").message, "Your password has expired")
    }

    function test_layoutButton() {
        var main = make({})
        var bar = part(main, "cornerBar")
        compare(bar.layoutCode, "FR")
        verify(bar.showLayout)
        bar.layoutClicked()
        compare(main.keyboardModel.currentLayout, 1)
        compare(bar.layoutCode, "US")
    }

    function test_power() {
        var main = make({})
        var bar = part(main, "cornerBar")
        verify(bar.canSleep && bar.canShutDown && bar.canRestart && !bar.canHibernate)
        bar.sleepRequested()
        bar.restartRequested()
        bar.shutDownRequested()
        compare(JSON.stringify(main.greeter.calls), JSON.stringify([["suspend"], ["reboot"], ["powerOff"]]))
    }

    function test_idleBackToLockThenBlack() {
        var main = make({ idleToLock: 150, idleToBlack: 400 })
        keyClick(Qt.Key_A)
        compare(main.view, "signin")
        tryCompare(main, "view", "lock", 2000)
        compare(part(main, "signInColumn").passwordField.text, "")
        tryCompare(main, "view", "black", 2000)
        verify(part(main, "blackScreen").visible)
        // a key wakes the screen to the lock view and types nothing
        keyClick(Qt.Key_B)
        compare(main.view, "lock")
        compare(part(main, "signInColumn").passwordField.text, "")
    }

    function test_mouseWakesBlack() {
        var main = make({ idleToLock: 100, idleToBlack: 200 })
        tryCompare(main, "view", "black", 2000)
        mouseMove(main, 300, 300)
        mouseMove(main, 320, 310)
        compare(main.view, "lock")
    }

    function test_activityKeepsSignInOpen() {
        var main = make({ idleToLock: 300, idleToBlack: 100000 })
        keyClick(Qt.Key_A)
        for (var i = 0; i < 4; ++i) {
            wait(150)
            keyClick(Qt.Key_B)
        }
        compare(main.view, "signin")
    }

    function test_noIdleWhileSigningIn() {
        var main = make({ idleToLock: 300, idleToBlack: 400 })
        keyClick(Qt.Key_A)
        keyClick(Qt.Key_Return)
        wait(500)
        compare(main.view, "signin")
    }

    function test_escapeClosesTheFlyoutOnly() {
        var main = make({})
        mouseClick(main, 600, 400)
        var bar = part(main, "cornerBar")
        bar.accessFlyout.open()
        tryVerify(function () { return bar.accessFlyout.opened })
        keyClick(Qt.Key_Escape)
        tryVerify(function () { return !bar.accessFlyout.visible })
        compare(main.view, "signin")
        // the same with the focus in the field (after a click on a row)
        bar.accessFlyout.open()
        tryVerify(function () { return bar.accessFlyout.opened })
        part(main, "signInColumn").focusField()
        keyClick(Qt.Key_Escape)
        compare(main.view, "signin")
        tryVerify(function () { return !bar.accessFlyout.visible })
        // and the next Esc goes back to the lock view
        part(main, "signInColumn").focusField()
        keyClick(Qt.Key_Escape)
        compare(main.view, "lock")
    }

    function test_noPowerButtonWithoutActions() {
        var main = make({})
        var bar = part(main, "cornerBar")
        verify(bar.showPower)
        main.greeter.canPowerOff = false
        main.greeter.canReboot = false
        main.greeter.canSuspend = false
        verify(!bar.showPower)
    }

    function test_secondaryScreen() {
        var main = make({ primary: false, keysHere: false, pointerHere: false })
        var lock = findChild(main, "lockView")
        verify(!lock || !lock.visible)
        var column = findChild(main, "signInColumn")
        verify(!column || !column.visible)
        verify(part(main, "secondaryBackdrop").visible)
        keyClick(Qt.Key_A)
        compare(main.greeter.calls.length, 0)
    }

    // two screens: the sign-in goes where the pointer is, so the password
    // can be typed on the monitor too
    function test_pointerBringsTheSignIn() {
        var main = make({ primary: false, keysHere: false, pointerHere: false })
        verify(!main.owner)
        main.pointerHere = true
        verify(main.owner)
        compare(main.keyRequests, 1)
        verify(part(main, "lockView").visible)
        verify(!part(main, "secondaryBackdrop").visible)
        main.showSignIn("ab")
        compare(main.view, "signin")
        compare(part(main, "signInColumn").passwordField.text, "ab")
        // the pointer moves on: nothing typed stays behind on this screen
        main.pointerHere = false
        verify(!main.owner)
        compare(main.view, "lock")
        compare(part(main, "signInColumn").passwordField.text, "")
        verify(part(main, "secondaryBackdrop").visible)
    }

    function test_screenWithTheKeysShowsTheSignIn() {
        // the pointer is elsewhere but the keys come here: the field shows
        var main = make({ primary: false, pointerHere: false, keysHere: false })
        main.keysHere = true
        verify(main.owner)
        verify(part(main, "lockView").visible)
    }

    function test_mainScreenLetsGoWhenAnotherTakesTheKeys() {
        var main = make({ pointerHere: false, keysHere: false })
        // before anything happened, the main screen shows the sign-in
        verify(main.owner)
        main.keysHere = true
        main.keysHere = false
        verify(!main.owner)
        verify(part(main, "secondaryBackdrop").visible)
    }

    function test_mainScreenLetsGoOnceThePointerLeft() {
        // the keys went elsewhere first: the main screen keeps its field
        // until the pointer has been here and gone
        var main = make({ pointerHere: false, keysHere: false })
        verify(main.owner)
        main.pointerHere = true
        main.pointerHere = false
        verify(!main.owner)
    }

    function test_signInUnderWayStaysWhenThePointerGoes() {
        var main = make({ pointerHere: true, keysHere: false })
        main.showSignIn("pw")
        part(main, "signInColumn").passwordField.submit()
        verify(main.greeter.calls.length > 0)
        main.pointerHere = false
        // busy: the screen that is signing in keeps its view
        verify(main.owner)
        main.greeter.loginSucceeded()
        compare(main.view, "welcome")
        verify(main.owner)
    }

    function test_noIdleTimersOnAScreenWithoutTheSignIn() {
        var main = make({ primary: false, keysHere: false, pointerHere: false })
        main.showSignIn("x")
        compare(main.view, "lock")
    }

    function test_noAccountsAsksForTheName() {
        var main = make({ users: null })
        mouseClick(main, 600, 400)
        var name = part(main, "nameField")
        verify(name.visible)
        verify(name.activeFocus)
        keyClick(Qt.Key_R)
        keyClick(Qt.Key_O)
        keyClick(Qt.Key_Return)
        var column = part(main, "signInColumn")
        verify(column.passwordField.inputItem.activeFocus)
        keyClick(Qt.Key_P)
        keyClick(Qt.Key_Return)
        compare(JSON.stringify(main.greeter.calls[0]), JSON.stringify(["login", "ro", "p", 0]))
    }

    function test_nameFieldHiddenWithAccounts() {
        var main = make({})
        var name = findChild(main, "nameField")
        verify(!name || !name.visible)
    }

    function test_onScreenKeyboard() {
        var main = make({})
        mouseClick(main, 600, 400)
        var bar = part(main, "cornerBar")
        bar.keyboardToggled(true)
        verify(bar.keyboardOn)
        verify(part(main, "signInColumn").passwordField.inputItem.activeFocus)
        bar.keyboardToggled(false)
        verify(!bar.keyboardOn)
    }

    function test_probesMissingAreHarmless() {
        var main = make({ statusProbes: true })
        // whatever the probes find (or not), the names are Raye icons or empty
        var lock = part(main, "lockView")
        verify(lock.networkIcon === "" || lock.networkIcon.indexOf("network-") === 0)
        verify(lock.batteryIcon === "" || lock.batteryIcon.indexOf("battery-") === 0)
    }

    // The greeter shows the same clock as the lock screen, from the same
    // module: a 12-hour region keeps its AM/PM, a 24-hour one gains none.
    function test_clockKeepsAmPm() {
        var main = make({})
        var lock = part(main, "lockView")
        lock.clockRunning = false
        var clock = findChild(lock, "clock")
        verify(clock)
        lock.locale = Qt.locale("en_US")
        lock.now = new Date(2026, 9, 29, 3, 5)
        var morning = clock.text
        lock.now = new Date(2026, 9, 29, 15, 5)
        verify(morning !== clock.text, "both read \"" + morning + "\"")
        compare(morning, "3:05 AM")
        compare(clock.text, "3:05 PM")
        lock.locale = Qt.locale("fr_FR")
        compare(clock.text, "15:05")
    }

    function test_pictureFromConfig() {
        var file = Qt.resolvedUrl("../../wallpaper/Raye/contents/images/3840x2160.png").toString()
        var main = make({ themeConfig: { background: file } })
        compare(part(main, "lockView").picture.toString(), file)
        wait(300)
        compare(part(main, "lockView").picture.toString(), file)
    }

    function test_missingPictureFallsBack() {
        var main = make({ themeConfig: { background: "/srv/none/lake.jpg" } })
        tryVerify(function () { return RayePalette.isDefaultPicture(part(main, "lockView").picture) })
    }

    function test_defaultPicture() {
        var main = make({})
        verify(RayePalette.isDefaultPicture(part(main, "lockView").picture))
    }
}
