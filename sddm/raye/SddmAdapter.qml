// Turns SDDM's objects (sddm, userModel, keyboard, sessionModel, config)
// into plain properties and calls for Main.qml, so the screen itself
// never talks to SDDM directly and can be tested with stand-ins.
//
// In:  greeter (sddm), users (userModel), keyboardModel (keyboard),
//      sessions (sessionModel), themeConfig (config); any can be null
// Out: accounts      [{ name, realName, icon, needsPassword }]
//      count, currentIndex (the account shown), userName, userLogin,
//      userPicture, needsPassword
//      capsLock, layoutCode, showLayout
//      canSleep, canHibernate, canShutDown, canRestart
//      picture (the lock picture from theme.conf, "" = the Raye one)
//      busy (a sign-in runs), signedIn, message (from PAM)
//      sessionChoices [{ key, index, file }] the Raye desktops ("plasma",
//      "hyprland"), showSessions (more than one), sessionKey (the one the
//      account signs in to, "" = SDDM's last session)
//      failed(), succeeded()
// Do:  login(password), loginAs(name, password), sleep(), hibernate(),
//      shutDown(), restart(), nextLayout(), chooseSession(key)
//
// store (QtCore Settings or a stand-in) keeps the desktop each account
// last signed in to; SDDM itself only keeps the last one of all.
//
// The password only passes through login(); nothing here keeps it.
import QtQuick
import "status.js" as Status

Item {
    id: adapter

    property var greeter: null
    property var users: null
    property var keyboardModel: null
    property var sessions: null
    property var themeConfig: null
    property var store: null

    property var accounts: []
    readonly property int count: accounts.length
    property int currentIndex: -1
    readonly property var current: currentIndex >= 0 && currentIndex < accounts.length
                                   ? accounts[currentIndex] : null
    readonly property string userName: Status.displayName(current)
    readonly property string userLogin: current ? current.name : ""
    readonly property string userPicture: current ? current.icon : ""
    readonly property bool needsPassword: current ? current.needsPassword : true

    readonly property bool capsLock: !!keyboardModel && !!keyboardModel.capsLock
    readonly property var layouts: keyboardModel && keyboardModel.layouts ? keyboardModel.layouts : []
    readonly property string layoutCode: keyboardModel
                                         ? Status.layoutCode(layouts, keyboardModel.currentLayout) : ""
    readonly property bool showLayout: Status.showLayout(layouts)

    readonly property bool canSleep: !!greeter && !!greeter.canSuspend
    readonly property bool canHibernate: !!greeter && !!greeter.canHibernate
    readonly property bool canShutDown: !!greeter && !!greeter.canPowerOff
    readonly property bool canRestart: !!greeter && !!greeter.canReboot

    readonly property url picture: themeConfig ? Status.picture(themeConfig.background) : ""

    // true while the account list is still being read
    readonly property bool pending: rebuildLater.running || reader.count !== accounts.length

    property bool busy: false
    property bool signedIn: false
    property string message: ""

    signal failed()
    signal succeeded()

    visible: false

    onCurrentIndexChanged: {
        message = ""
        pickedKey = ""
    }

    // --- sessions ------------------------------------------------------
    property var sessionChoices: []
    readonly property bool showSessions: sessionChoices.length > 1
    // a desktop picked on screen for the account shown
    property string pickedKey: ""
    readonly property string sessionKey: {
        var c = sessionFor(userLogin)
        return c ? c.key : ""
    }

    function remembered(user) {
        if (!store || !user)
            return ""
        return String(store.value("sessions/" + user, "") || "")
    }

    function sessionFor(user) {
        var i
        for (i = 0; pickedKey !== "" && i < sessionChoices.length; ++i)
            if (sessionChoices[i].key === pickedKey)
                return sessionChoices[i]
        return Status.pickSession(sessionChoices, remembered(user), sessions ? sessions.lastIndex : -1)
    }

    function chooseSession(key) {
        for (var i = 0; i < sessionChoices.length; ++i)
            if (sessionChoices[i].key === key)
                pickedKey = key
    }

    Instantiator {
        id: sessionReader
        model: adapter.sessions && typeof adapter.sessions.rowCount === "function" ? adapter.sessions : 0
        delegate: QtObject {
            required property var model
            readonly property string file: model.file ? String(model.file) : ""
            readonly property string name: model.name ? String(model.name) : ""
        }
        onObjectAdded: readSessionsLater.restart()
        onObjectRemoved: readSessionsLater.restart()
    }

    Timer {
        id: readSessionsLater
        interval: 0
        onTriggered: {
            var list = []
            for (var i = 0; i < sessionReader.count; ++i) {
                var o = sessionReader.objectAt(i)
                list.push(o ? { file: o.file, name: o.name } : null)
            }
            adapter.sessionChoices = Status.sessionChoices(list)
        }
    }

    // the user and session of the sign-in under way, remembered once it works
    property string signingUser: ""
    property string signingSession: ""

    function sessionIndex() {
        var count = sessions && typeof sessions.rowCount === "function" ? sessions.rowCount() : -1
        var last = sessions ? sessions.lastIndex : -1
        // a model that does not say how many sessions it has: trust a
        // valid last index
        if (count < 0)
            return typeof last === "number" && last >= 0 ? last : 0
        return Math.max(0, Status.indexInRange(last, count))
    }

    function loginAs(name, password) {
        var user = String(name || "").trim()
        if (!greeter || busy || signedIn || user === "")
            return
        var choice = sessionFor(user)
        busy = true
        message = ""
        signingUser = user
        signingSession = choice ? choice.file : ""
        greeter.login(user, password, choice ? choice.index : sessionIndex())
    }

    function login(password) {
        loginAs(userLogin, password)
    }

    function sleep() {
        if (canSleep)
            greeter.suspend()
    }
    function hibernate() {
        if (canHibernate)
            greeter.hibernate()
    }
    function shutDown() {
        if (canShutDown)
            greeter.powerOff()
    }
    function restart() {
        if (canRestart)
            greeter.reboot()
    }

    function nextLayout() {
        if (!keyboardModel)
            return
        var next = Status.nextLayout(layouts.length, keyboardModel.currentLayout)
        if (next >= 0)
            keyboardModel.currentLayout = next
    }

    // --- accounts ------------------------------------------------------
    function rebuild() {
        var list = []
        for (var i = 0; i < reader.count; ++i) {
            var o = reader.objectAt(i)
            if (o)
                list.push({ name: o.name, realName: o.realName, icon: o.icon,
                            needsPassword: o.needsPassword })
        }
        var keep = current ? current.name : ""
        accounts = list
        // the account on screen stays on screen when the list changes
        var index = -1
        for (var j = 0; keep !== "" && j < list.length; ++j)
            if (list[j].name === keep)
                index = j
        if (index < 0)
            index = Status.firstUser(list, users ? users.lastIndex : -1, users ? users.lastUser : "")
        currentIndex = index
    }

    Instantiator {
        id: reader
        model: adapter.users
        delegate: QtObject {
            required property var model
            readonly property string name: model.name !== undefined ? String(model.name) : ""
            readonly property string realName: model.realName ? String(model.realName) : ""
            readonly property string icon: model.icon ? String(model.icon) : ""
            readonly property bool needsPassword: model.needsPassword !== false
        }
        onObjectAdded: rebuildLater.restart()
        onObjectRemoved: rebuildLater.restart()
    }

    // one rebuild for a whole model filling at once
    Timer {
        id: rebuildLater
        interval: 0
        onTriggered: adapter.rebuild()
    }

    Connections {
        target: adapter.greeter
        ignoreUnknownSignals: true
        function onLoginFailed() {
            adapter.busy = false
            adapter.failed()
        }
        function onLoginSucceeded() {
            if (adapter.store && adapter.signingUser !== "" && adapter.signingSession !== "")
                adapter.store.setValue("sessions/" + adapter.signingUser, adapter.signingSession)
            adapter.busy = false
            adapter.signedIn = true
            adapter.succeeded()
        }
        function onInformationMessage(message) {
            adapter.message = String(message)
        }
    }
}
