// The Raye sign-in screen.
//
// SDDM starts one of these per screen. The screen the user is on shows
// the lock view (picture, clock, date, status icons); any key, click,
// wheel or swipe slides it away to the sign-in view (blurred picture,
// account picture, name, password field, other accounts bottom left, the
// corner bar bottom right); a correct password turns the column into
// "Welcome". The other screens only show the picture.
//
// "The screen the user is on": SDDM gives the keys to the main screen's
// view only. The sign-in follows the pointer instead: the screen under it
// asks for the keys and shows the sign-in, so the password can be typed
// on any screen. A screen that holds the keys shows it too, so typing
// always lands in a field that is on screen. Until the pointer has been
// anywhere, that is the main screen.
//
// The words, colours, sizes and motion come from org.raye.signin; this
// file only puts the pieces together and wires them to SDDM through
// SddmAdapter.
import QtQuick
import org.raye.signin
import "status.js" as Status

FocusScope {
    id: root

    // SDDM's objects (stand-ins in the tests)
    property var greeter: typeof sddm !== "undefined" ? sddm : null
    property var users: typeof userModel !== "undefined" ? userModel : null
    property var keyboardModel: typeof keyboard !== "undefined" ? keyboard : null
    property var sessions: typeof sessionModel !== "undefined" ? sessionModel : null
    property var themeConfig: typeof config !== "undefined" ? config : null
    property bool primary: typeof primaryScreen !== "undefined" ? !!primaryScreen : true
    // where the desktop of each account is kept; only under SDDM, the
    // tests and the preview pass their own or none
    property var sessionStore: typeof sddm !== "undefined" && sessionMemory.status === Loader.Ready
                               ? sessionMemory.item : null
    // the pointer is on this screen, this screen's view has the keys
    property bool pointerHere: pointer.hovered
    property bool keysHere: Window.active
    // this screen shows the sign-in; a sign-in under way stays where it
    // is, whatever the pointer does
    readonly property bool owner: pointerHere || keysHere || view === "welcome" || adapter.busy
                                  || (primary && !keysSeen && !pointerLeft)
    // this view had the keys once, or the pointer came and went: the main
    // screen lets go of the sign-in then. Until one of them, it keeps it,
    // so there is always a field on some screen.
    property bool keysSeen: false
    property bool pointerLeft: false
    // how many times the view asked for the keys (tests)
    property int keyRequests: 0

    // load the network and battery icons from the Plasma modules
    property bool statusProbes: true
    // sign-in view back to the lock view, then a black screen
    property int idleToLock: Motion.idleToLock
    property int idleToBlack: Motion.idleToBlack

    // "lock", "signin", "welcome" or "black"
    property string view: "lock"
    readonly property bool ready: !adapter.pending
    readonly property bool askName: adapter.count === 0

    readonly property real pixelHeight: height * Screen.devicePixelRatio
    // the picture of theme.conf, or the Raye one (also when that file
    // cannot be read)
    readonly property url picture: adapter.picture.toString() !== "" && pictureProbe.status !== Image.Error
                                   ? adapter.picture : RayePalette.defaultPicture(pixelHeight)

    readonly property string networkIcon: networkProbe.status === Loader.Ready
                                          ? Status.networkIcon(networkProbe.item.icon) : ""
    readonly property string batteryIcon: batteryProbe.status === Loader.Ready
                                          ? Status.batteryIcon(batteryProbe.item.hasBattery,
                                                               batteryProbe.item.percent,
                                                               batteryProbe.item.pluggedIn) : ""

    property bool keyboardOn: false

    focus: true
    // the greeter runs as a user without any Plasma setting: never slow
    // the screens down
    Component.onCompleted: {
        Motion.reducedMotion = false
        if (keysHere)
            keysSeen = true
        activity()
    }

    onKeysHereChanged: {
        if (keysHere)
            keysSeen = true
    }
    onPointerHereChanged: {
        if (pointerHere)
            claimKeys()
        else
            pointerLeft = true
    }
    onOwnerChanged: {
        if (owner) {
            activity()
        } else {
            // the user went to another screen: nothing typed stays here
            showLock()
        }
    }

    // ask KWin for the keys, so they reach the screen the pointer is on
    function claimKeys() {
        keyRequests += 1
        if (Window.window && !Window.window.active)
            Window.window.requestActivate()
    }

    function focusInput() {
        if (askName && nameField.text.trim() === "")
            nameField.forceActiveFocus()
        else
            column.focusField()
    }

    function showSignIn(text) {
        if (!owner || view === "welcome")
            return
        view = "signin"
        activity()
        if (askName && nameField.text.trim() === "") {
            nameField.forceActiveFocus()
            if (text !== "")
                nameField.insert(nameField.cursorPosition, text)
        } else {
            column.focusField()
            if (text !== "")
                column.typeText(text)
        }
    }

    function showLock() {
        if (view === "welcome")
            return
        bar.closeFlyouts()
        keyboardOn = false
        column.reset()
        nameField.clear()
        view = "lock"
        lock.forceActiveFocus()
        activity()
    }

    // Esc: an open flyout closes first (the field may have the focus
    // while it is open), then back to the lock view
    function back() {
        if (bar.flyoutOpen)
            bar.closeFlyouts()
        else
            showLock()
    }

    // any input: the idle timers start again (only those that apply to
    // the view on screen)
    function activity() {
        var idle = owner && !adapter.busy
        if (idle && view === "signin")
            toLock.restart()
        else
            toLock.stop()
        if (idle && (view === "lock" || view === "signin"))
            toBlack.restart()
        else
            toBlack.stop()
    }

    function submit(password) {
        if (askName)
            adapter.loginAs(nameField.text, password)
        else
            adapter.login(password)
    }

    SddmAdapter {
        id: adapter
        greeter: root.greeter
        users: root.users
        keyboardModel: root.keyboardModel
        sessions: root.sessions
        themeConfig: root.themeConfig
        store: root.sessionStore
        onFailed: {
            column.fail()
            root.activity()
        }
        onBusyChanged: root.activity()
        onSucceeded: {
            bar.closeFlyouts()
            root.keyboardOn = false
            root.view = "welcome"
        }
    }

    // --- the other screens: the picture only ---------------------------
    Backdrop {
        id: secondaryBackdrop
        objectName: "secondaryBackdrop"
        anchors.fill: parent
        visible: !root.owner
        source: root.picture
        blurred: false
    }

    // --- sign-in view, built under the lock view so the slide is instant
    Backdrop {
        id: signInBackdrop
        anchors.fill: parent
        visible: root.owner && (root.view !== "lock" || lock.opacity < 1)
        source: root.picture
        blurred: true
    }

    SignInColumn {
        id: column
        objectName: "signInColumn"
        anchors.fill: parent
        visible: root.owner && opacity > 0
        enabled: root.view === "signin"
        opacity: root.view === "welcome" ? 0 : 1
        Behavior on opacity {
            NumberAnimation { duration: Motion.normal; easing.type: Motion.leave }
        }
        // a blank name keeps the name line's height for the name field
        userName: root.askName ? " " : adapter.userName
        userPicture: adapter.userPicture
        needsPassword: adapter.needsPassword
        capsLock: adapter.capsLock
        busy: adapter.busy
        message: adapter.message
        bottomInset: keyboard.occupied
        onSubmitted: password => root.submit(password)
        onBackRequested: root.back()
        desktop: adapter.showSessions ? adapter.sessionKey : ""
        onDesktopClicked: bar.toggle(bar.sessionFlyout)
    }

    // only when SDDM lists no account: where the name would be
    NameField {
        id: nameField
        objectName: "nameField"
        visible: root.askName && column.visible
        enabled: column.enabled
        opacity: column.opacity
        anchors.horizontalCenter: parent.horizontalCenter
        // 20 px above the password field, over the (empty) name line;
        // follows the column when the on-screen keyboard moves it
        y: column.y + column.passwordField.parent.y + column.passwordField.y - 20 - height
        KeyNavigation.tab: column.passwordField.inputItem
        Keys.onEscapePressed: root.back()
        onAccepted: if (text.trim() !== "") column.focusField()
        onTextChanged: root.activity()
    }

    UserList {
        id: list
        objectName: "userList"
        x: RayePalette.listEdge
        anchors.bottom: parent.bottom
        anchors.bottomMargin: RayePalette.listEdge + keyboard.occupied
        visible: root.owner && count > 1 && root.view !== "lock" && opacity > 0
        enabled: root.view === "signin"
        opacity: root.view === "welcome" ? 0 : 1
        Behavior on opacity {
            NumberAnimation { duration: Motion.normal; easing.type: Motion.leave }
        }
        model: adapter.accounts
        currentIndex: adapter.currentIndex
        onUserSelected: index => {
            adapter.currentIndex = index
            column.reset()
            column.focusField()
            root.activity()
        }
    }

    OnScreenKeyboard {
        id: keyboard
        active: root.keyboardOn && root.view === "signin"
    }

    CornerBar {
        id: bar
        objectName: "cornerBar"
        visible: root.owner && root.view !== "lock" && opacity > 0
        enabled: root.view === "signin"
        opacity: root.view === "welcome" ? 0 : 1
        Behavior on opacity {
            NumberAnimation { duration: Motion.normal; easing.type: Motion.leave }
        }
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: RayePalette.edge
        anchors.bottomMargin: RayePalette.edge + keyboard.occupied
        networkIcon: root.networkIcon
        layoutCode: adapter.layoutCode
        showLayout: adapter.showLayout
        showPower: adapter.canSleep || adapter.canHibernate || adapter.canShutDown || adapter.canRestart
        canSleep: adapter.canSleep
        canHibernate: adapter.canHibernate
        canShutDown: adapter.canShutDown
        canRestart: adapter.canRestart
        keyboardOn: root.keyboardOn
        keyboardAvailable: keyboard.available
        sessions: adapter.showSessions ? adapter.sessionChoices : []
        session: adapter.sessionKey
        onSessionPicked: key => {
            adapter.chooseSession(key)
            root.focusInput()
        }
        onSleepRequested: adapter.sleep()
        onHibernateRequested: adapter.hibernate()
        onShutDownRequested: adapter.shutDown()
        onRestartRequested: adapter.restart()
        onLayoutClicked: {
            adapter.nextLayout()
            root.focusInput()
        }
        onKeyboardToggled: on => {
            root.keyboardOn = on
            // the keys of the on-screen keyboard go to the focused field
            root.focusInput()
        }
    }

    // spinner + "Welcome" over the same blurred picture
    StatusScreen {
        id: status
        objectName: "statusScreen"
        anchors.fill: parent
        mode: "welcome"
        showBackdrop: false
        visible: opacity > 0
        opacity: root.view === "welcome" ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: Motion.normal; easing.type: Motion.enter }
        }
    }

    // --- lock view, on top until dismissed -----------------------------
    LockView {
        id: lock
        objectName: "lockView"
        anchors.fill: parent
        visible: root.owner && opacity > 0
        shown: root.owner && (root.view === "lock" || root.view === "black")
        focus: true
        picture: root.picture
        networkIcon: root.networkIcon
        batteryIcon: root.batteryIcon
        // the key that lifts the curtain only lifts it, like Windows
        onDismissRequested: root.showSignIn("")
        onActivity: root.activity()
    }

    // --- 10 minutes without input: black -------------------------------
    Rectangle {
        id: blackScreen
        objectName: "blackScreen"
        anchors.fill: parent
        color: "#000000"
        visible: root.view === "black"

        MouseArea {
            anchors.fill: parent
            enabled: blackScreen.visible
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            property point last: Qt.point(-1, -1)
            // a small move only: the first event after the screen went
            // black may just be the pointer's own position
            onPositionChanged: mouse => {
                if (last.x >= 0 && (Math.abs(mouse.x - last.x) > 2 || Math.abs(mouse.y - last.y) > 2))
                    root.showLock()
                last = Qt.point(mouse.x, mouse.y)
            }
            onPressed: root.showLock()
            onWheel: root.showLock()
        }
    }

    onViewChanged: {
        activity()
        if (view === "black") {
            bar.closeFlyouts()
            blackScreen.forceActiveFocus()
        }
    }

    // --- idle ----------------------------------------------------------
    Timer {
        id: toLock
        interval: root.idleToLock
        onTriggered: root.showLock()
    }

    Timer {
        id: toBlack
        interval: root.idleToBlack
        onTriggered: root.view = "black"
    }

    // any key no item took (on the black screen: every key)
    Keys.onPressed: event => {
        if (view === "black") {
            showLock()
            event.accepted = true
            return
        }
        activity()
    }

    Connections {
        target: column.passwordField
        function onTextChanged() { root.activity() }
    }
    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() { root.activity() }
    }
    HoverHandler {
        id: pointer
        onPointChanged: root.activity()
    }
    PointHandler {
        acceptedButtons: Qt.AllButtons
        onActiveChanged: root.activity()
    }

    // Meta+Space: the next keyboard layout, like on the desktop
    Shortcut {
        sequence: "Meta+Space"
        enabled: root.owner && root.view === "signin" && adapter.showLayout
        onActivated: adapter.nextLayout()
    }

    // only to learn whether the picture of theme.conf loads
    Image {
        id: pictureProbe
        visible: false
        source: adapter.picture
        sourceSize: Qt.size(16, 16)
        asynchronous: true
    }

    // --- status icons: hidden when a module is missing -----------------
    Loader {
        id: networkProbe
        active: root.statusProbes
        source: "NetworkProbe.qml"
    }
    Loader {
        id: batteryProbe
        active: root.statusProbes
        source: "BatteryProbe.qml"
    }
    Loader {
        id: sessionMemory
        active: typeof sddm !== "undefined"
        source: "SessionMemory.qml"
    }
}
