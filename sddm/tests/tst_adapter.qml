// SddmAdapter turns SDDM's objects into plain properties and calls.
import QtQuick
import QtTest
import "../raye"

TestCase {
    id: test
    name: "SddmAdapter"

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
        id: usersComponent
        ListModel {
            property int lastIndex: 0
            property string lastUser: "camille"
            ListElement { name: "alex"; realName: "Alex Martin"; icon: ""; needsPassword: true }
            ListElement { name: "camille"; realName: "Camille Roy"; icon: "/var/lib/AccountsService/icons/camille"; needsPassword: true }
            ListElement { name: "kiosk"; realName: ""; icon: ""; needsPassword: false }
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
        id: adapterComponent
        SddmAdapter {}
    }

    function make(extra) {
        var props = {
            greeter: createTemporaryObject(greeterComponent, test),
            users: createTemporaryObject(usersComponent, test),
            keyboardModel: createTemporaryObject(keyboardComponent, test),
            sessions: { lastIndex: 1 },
            themeConfig: { background: "" }
        }
        for (var k in extra)
            props[k] = extra[k]
        var a = createTemporaryObject(adapterComponent, test, props)
        verify(a)
        return a
    }

    function test_accounts() {
        var a = make({})
        tryCompare(a, "count", 3)
        compare(a.accounts[0].name, "alex")
        compare(a.accounts[1].icon, "/var/lib/AccountsService/icons/camille")
        compare(a.accounts[2].needsPassword, false)
    }

    function test_startsOnLastUser() {
        var a = make({})
        tryCompare(a, "currentIndex", 1)
        compare(a.userName, "Camille Roy")
        compare(a.userLogin, "camille")
        compare(a.userPicture, "/var/lib/AccountsService/icons/camille")
        verify(a.needsPassword)
    }

    function test_loginNameWhenNoRealName() {
        var a = make({})
        tryCompare(a, "count", 3)
        a.currentIndex = 2
        compare(a.userName, "kiosk")
        verify(!a.needsPassword)
    }

    function test_noUsers() {
        var a = make({ users: null })
        compare(a.count, 0)
        compare(a.currentIndex, -1)
        compare(a.userName, "")
        verify(a.needsPassword)
    }

    function test_loginGoesToSddm() {
        var a = make({})
        tryCompare(a, "currentIndex", 1)
        a.login("hunter2")
        verify(a.busy)
        compare(a.greeter.calls.length, 1)
        compare(JSON.stringify(a.greeter.calls[0]), JSON.stringify(["login", "camille", "hunter2", 1]))
        // nothing else goes out while it runs
        a.login("again")
        compare(a.greeter.calls.length, 1)
    }

    function test_loginWithTypedName() {
        var a = make({ users: null })
        a.loginAs("  root\n", "pw")
        compare(JSON.stringify(a.greeter.calls[0]), JSON.stringify(["login", "root", "pw", 1]))
    }

    function test_loginRefusesEmptyName() {
        var a = make({ users: null })
        a.loginAs("   ", "pw")
        compare(a.greeter.calls.length, 0)
        verify(!a.busy)
    }

    function test_sessionFallsBackToFirst() {
        var a = make({ sessions: { lastIndex: -1 } })
        tryCompare(a, "currentIndex", 1)
        a.login("x")
        compare(a.greeter.calls[0][3], 0)
    }

    function test_failed() {
        var a = make({})
        var spy = createTemporaryObject(spyComponent, test, { target: a, signalName: "failed" })
        tryCompare(a, "currentIndex", 1)
        a.login("bad")
        a.greeter.loginFailed()
        compare(spy.count, 1)
        verify(!a.busy)
        // the password is not kept anywhere in the adapter
        for (var k in a)
            if (typeof a[k] === "string")
                verify(a[k] !== "bad", k)
    }

    function test_succeeded() {
        var a = make({})
        var spy = createTemporaryObject(spyComponent, test, { target: a, signalName: "succeeded" })
        a.login("good")
        a.greeter.loginSucceeded()
        compare(spy.count, 1)
        verify(a.signedIn)
    }

    function test_message() {
        var a = make({})
        a.greeter.informationMessage("Your password has expired")
        compare(a.message, "Your password has expired")
        a.currentIndex = 0
        compare(a.message, "")
    }

    function test_capsLock() {
        var a = make({})
        verify(!a.capsLock)
        a.keyboardModel.capsLock = true
        verify(a.capsLock)
    }

    function test_layout() {
        var a = make({})
        compare(a.layoutCode, "FR")
        verify(a.showLayout)
        a.nextLayout()
        compare(a.keyboardModel.currentLayout, 1)
        compare(a.layoutCode, "US")
        a.nextLayout()
        compare(a.keyboardModel.currentLayout, 0)
    }

    function test_oneLayoutHidden() {
        var a = make({})
        a.keyboardModel.layouts = [{ shortName: "us", longName: "English (US)" }]
        verify(!a.showLayout)
    }

    function test_power() {
        var a = make({})
        verify(a.canSleep && a.canShutDown && a.canRestart)
        verify(!a.canHibernate)
        a.sleep(); a.shutDown(); a.restart(); a.hibernate()
        compare(JSON.stringify(a.greeter.calls), JSON.stringify([["suspend"], ["powerOff"], ["reboot"]]))
    }

    function test_powerRefusedWhenNotAllowed() {
        var a = make({})
        a.greeter.canPowerOff = false
        a.greeter.canReboot = false
        a.greeter.canSuspend = false
        verify(!a.canShutDown && !a.canRestart && !a.canSleep)
        a.sleep(); a.shutDown(); a.restart()
        compare(a.greeter.calls.length, 0)
    }

    function test_noGreeter() {
        var a = make({ greeter: null, keyboardModel: null, sessions: null, themeConfig: null })
        verify(!a.canShutDown)
        compare(a.layoutCode, "")
        verify(!a.capsLock)
        a.login("x")
        verify(!a.busy)
        compare(a.picture.toString(), "")
    }

    function test_picture() {
        var a = make({ themeConfig: { background: "/srv/lake.jpg" } })
        compare(a.picture.toString(), "file:///srv/lake.jpg")
    }

    Component {
        id: spyComponent
        SignalSpy {}
    }
}
