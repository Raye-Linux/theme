// Everything the Raye lock screen draws, fed by plain properties so it can
// be tested without a greeter. LockScreen.qml (the greeter's main file)
// passes the greeter's objects in.
//
// Views, as Windows: the lock view (picture, clock, date, status icons,
// media card) -> any key, click, wheel or swipe -> the sign-in view for
// the session's user (blurred picture, picture, name, password field,
// "Switch user", corner bar) -> unlocking fades straight to the desktop,
// no "Welcome".
//
// In:  authenticator   the greeter's authenticators (or a test double):
//                      startAuthenticating(), respond(), properties
//                      busy, hadPrompt, prompt, promptForSecret,
//                      infoMessage, errorMessage, authenticatorTypes;
//                      signals succeeded, failed(kind),
//                      noninteractiveInfo(kind), noninteractiveError(kind)
//      userName  userImagePath  wallpaperImage (the lock picture setting)
//      locked          false during the grace time after locking: nothing
//                      is asked yet
//      canSuspend  canHibernate  showMedia
//      adapter         a LockAdapter (session pieces), or null
//      share           the LockShare of all the screens, or null for a
//                      view of its own (see LockShare.qml)
// Out: suspendRequested()  hibernateRequested()
//      unlocked()      authentication succeeded and the fade is done; the
//                      greeter must quit now
// Do:  clearPassword()
import QtQuick
import org.raye.signin
import "hints.js" as Hints

FocusScope {
    id: ui

    property var authenticator: null
    property string userName: ""
    property string userImagePath: ""
    property string wallpaperImage: ""
    property bool locked: true
    property bool canSuspend: false
    property bool canHibernate: false
    property bool showMedia: true
    property var adapter: null
    // the sign-in state shared with the other screens' views (a LockShare);
    // null: this view keeps its own (one screen, tests)
    property QtObject share: null

    readonly property QtObject shared: share ? share : ownState
    // this view answers the authenticator for all the screens
    readonly property bool driving: shared.driver === ui

    readonly property bool signingIn: shared.signingIn
    readonly property bool busy: shared.busy
    readonly property bool unlocking: shared.unlocking
    readonly property alias lockView: lockView
    readonly property alias column: column
    readonly property alias cornerBar: cornerBar
    readonly property alias mediaCard: mediaCard
    readonly property string methodHint: column.methodHint
    readonly property string message: column.message

    signal suspendRequested()
    signal hibernateRequested()
    signal unlocked()

    readonly property bool _wayland: Qt.platform.pluginName.indexOf("wayland") >= 0
    readonly property int _types: authenticator && authenticator.authenticatorTypes !== undefined
                                  ? authenticator.authenticatorTypes : 0
    readonly property string _lang: Strings.language
    // Raye Update was asked to arm and has not answered yet
    property bool arming: false
    property string _armThen: ""
    // what PAM is told goes through the driver
    readonly property var _driver: shared.driver ? shared.driver : ui

    LockShare {
        id: ownState
    }

    Component.onCompleted: {
        shared.join(ui)
        if (shared.password !== "")
            column.passwordField.text = shared.password
    }
    Component.onDestruction: shared.leave(ui)

    // --- pictures ----------------------------------------------------
    readonly property string _wanted: Hints.lockPicture(wallpaperImage)
    readonly property url picture: _wanted !== "" && probe.status === Image.Ready
                                   ? _wanted
                                   : RayePalette.defaultPicture(height * Screen.devicePixelRatio)

    // tells whether the chosen picture loads, before it is used
    Image {
        id: probe
        visible: false
        source: ui._wanted
        sourceSize: Qt.size(16, 16)
        asynchronous: true
    }

    // --- actions -----------------------------------------------------
    // Any screen may call these; the state they change is the same for
    // every screen, and what reaches PAM goes through the driver.
    function openSignIn(text) {
        if (shared.unlocking)
            return
        poke()
        if (!shared.signingIn) {
            shared.signingIn = true
            if (Window.window)
                Window.window.requestActivate()
            startAuth()
        }
        column.focusField()
        if (text !== undefined && text !== "")
            column.typeText(text)
    }

    function closeSignIn() {
        if (shared.unlocking)
            return
        clearPassword()
        setKeyboard(false)
        shared.signingIn = false
    }

    function clearPassword() {
        shared.resetRequested()
        if (shared.hasPending) {
            // nothing went out yet: the try is dropped with the password
            shared.pending = ""
            shared.hasPending = false
            shared.busy = false
        }
    }

    function startAuth() {
        if (!driving) {
            _driver.startAuth()
            return
        }
        if (authenticator && locked)
            authenticator.startAuthenticating()
    }

    function submit(password) {
        if (!driving) {
            _driver.submit(password)
            return
        }
        if (shared.unlocking || shared.busy)
            return
        if (shared.passwordless) {
            finishUnlock()
            return
        }
        if (!authenticator)
            return
        shared.busy = true
        shared.message = ""
        shared.tryError = ""
        if (shared.secretAsked) {
            respond(password)
        } else {
            shared.pending = password
            shared.hasPending = true
            if (!restart.running)
                startAuth()
        }
    }

    function respond(password) {
        shared.secretAsked = false
        shared.responded = true
        authenticator.respond(password)
    }

    function finishUnlock() {
        if (shared.unlocking)
            return
        shared.busy = false
        clearPassword()
        shared.unlocking = true
    }

    function setKeyboard(on) {
        shared.keyboardWanted = on
        if (_wayland && adapter)
            adapter.showSessionKeyboard(on)
        if (on)
            shared.focusRequested()
    }

    // Raye Update arms its set, then restarts the session itself
    function updateAndPower(then) {
        if (!adapter || arming)
            return
        if (!adapter.updatesReady) {
            powerNow(then)
            return
        }
        arming = true
        _armThen = then
        adapter.armUpdates(then)
    }

    function powerNow(then) {
        if (then === "shutdown")
            adapter.shutDown()
        else
            adapter.restart()
    }

    function poke() {
        if (shared.signingIn)
            shared.poked()
    }

    // --- what every screen does when the shared state moves -------------
    Connections {
        target: ui.shared
        function onSigningInChanged() {
            if (!ui.shared.signingIn) {
                cornerBar.closeFlyouts()
                lockView.forceActiveFocus()
            }
        }
        function onUnlockingChanged() {
            if (!ui.shared.unlocking)
                return
            if (Motion.normal === 0)
                ui.unlocked()
            else
                fadeOut.start()
        }
        function onPasswordChanged() {
            if (column.passwordField.text !== ui.shared.password)
                column.passwordField.text = ui.shared.password
        }
        function onFailRequested(text) {
            column.fail(text)
        }
        function onResetRequested() {
            column.reset()
        }
        function onFocusRequested() {
            column.focusField()
        }
        function onPoked() {
            if (ui.driving)
                idle.restart()
        }
    }

    // --- the authenticator (driver only) --------------------------------
    Connections {
        target: ui.authenticator
        enabled: ui.driving
        ignoreUnknownSignals: true

        function onPromptForSecretChanged() {
            var st = ui.shared
            st.secretAsked = true
            if (st.responded) {
                // PAM asks again in the same conversation (a new password,
                // a second factor): say what it asks
                st.message = ui.authenticator.promptForSecret
                st.busy = false
                st.responded = false
                st.resetRequested()
                st.focusRequested()
            } else if (st.hasPending) {
                var p = st.pending
                st.pending = ""
                st.hasPending = false
                ui.respond(p)
            }
        }
        function onPromptChanged() {
            var st = ui.shared
            st.secretAsked = true
            st.message = ui.authenticator.prompt
            if (st.responded) {
                st.busy = false
                st.responded = false
                st.resetRequested()
            }
        }
        function onInfoMessageChanged() {
            ui.shared.message = ui.authenticator.infoMessage
        }
        function onErrorMessageChanged() {
            ui.shared.message = ui.authenticator.errorMessage
            if (ui.shared.responded)
                ui.shared.tryError = ui.authenticator.errorMessage
        }
        function onFailed(kind) {
            // a reader that is not there, or a finger that did not match:
            // that is the method line's business, not the password's
            if (kind !== 0 && kind !== undefined)
                return
            var st = ui.shared
            st.secretAsked = false
            if (!st.responded) {
                // the conversation ended before the password went out (a
                // start too soon after the last one, a PAM error): not a
                // wrong password; start again a little later, the typed
                // password still waits for the prompt
                restart.restart()
                return
            }
            var text = st.tryError
            st.busy = false
            st.responded = false
            st.tryError = ""
            st.message = ""
            st.pending = ""
            st.hasPending = false
            st.failRequested(text)
            // a new conversation for the next try, once the greeter has
            // closed this one (starting at once fails again, forever)
            restart.restart()
        }
        function onSucceeded() {
            if (ui.authenticator.hadPrompt) {
                ui.finishUnlock()
            } else {
                // nothing was asked: a "Sign in" button, like Windows for
                // an account without a password
                ui.shared.passwordless = true
                ui.shared.busy = false
            }
        }
        function onNoninteractiveInfo(kind) {
            ui.shared.hint = Hints.spoke(ui.shared.hint, kind)
        }
        function onNoninteractiveError(kind) {
            ui.shared.now = Date.now()
            ui.shared.hint = Hints.failed(ui.shared.hint, kind, ui.shared.now)
            retry.restart()
        }
        function onAuthenticatorTypesChanged() {
            ui.shared.hint = Hints.available(ui.shared.hint, ui.authenticator.authenticatorTypes)
        }
    }

    Connections {
        target: ui.adapter
        ignoreUnknownSignals: true
        function onAboutToSuspend() {
            ui.closeSignIn()
        }
        function onUpdateArmFailed(reason) {
            if (!ui.arming)
                return
            ui.arming = false
            // the user said no: stay here, nothing was asked of the session
            if (reason === "cancelled") {
                ui.column.focusField()
                return
            }
            // already armed, or it could not be armed: do what was asked,
            // the updates wait for the next restart
            ui.powerNow(ui._armThen)
        }
    }

    onLockedChanged: {
        if (driving && locked && (shared.signingIn || shared.hasPending))
            startAuth()
    }

    // after a failed conversation, the next one starts after this pause
    Timer {
        id: restart
        objectName: "restartTimer"
        interval: 1500
        onTriggered: ui.startAuth()
    }

    // a typed password that PAM never asks for is not kept forever: the
    // field comes back so the user can try again
    Timer {
        id: waiting
        objectName: "waitingTimer"
        interval: 20000
        running: ui.driving && ui.shared.hasPending
        onTriggered: {
            ui.shared.pending = ""
            ui.shared.hasPending = false
            ui.shared.busy = false
            ui.shared.focusRequested()
        }
    }

    // the "try again" line goes after a while
    Timer {
        id: retry
        interval: Hints.RETRY_SHOWN
        onTriggered: ui.shared.now = Date.now()
    }

    // a minute without input on the sign-in view: back to the picture
    Timer {
        id: idle
        interval: Motion.idleToLock
        running: ui.driving && ui.shared.signingIn && !ui.shared.busy && !ui.shared.unlocking
        onTriggered: ui.closeSignIn()
    }

    NumberAnimation {
        id: fadeOut
        target: ui
        property: "opacity"
        to: 0
        duration: Motion.normal
        easing.type: Motion.leave
        onFinished: ui.unlocked()
    }

    // mouse movement keeps the sign-in view, like typing does
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        onPositionChanged: ui.poke()
    }

    // --- the sign-in view --------------------------------------------
    Backdrop {
        id: signInBackdrop
        objectName: "signInBackdrop"
        anchors.fill: parent
        source: ui.picture
        blurred: true
        visible: lockView.opacity < 1
    }

    SignInColumn {
        id: column
        objectName: "signInColumn"
        anchors.fill: parent
        opacity: ui.shared.signingIn ? 1 : 0
        visible: opacity > 0
        enabled: ui.shared.signingIn && !ui.shared.unlocking
        focus: ui.shared.signingIn
        Behavior on opacity {
            NumberAnimation { duration: Motion.normal; easing.type: Motion.enter }
        }

        userName: ui.userName
        userPicture: Hints.userPictureUrl(ui.userImagePath)
        needsPassword: !ui.shared.passwordless
        capsLock: ui.adapter ? ui.adapter.capsLock : false
        busy: ui.shared.busy
        methodHint: Hints.hintText(Hints.hint(ui.shared.hint, ui._types, ui.shared.now), ui._lang)
        methodHintIcon: Hints.hintIcon(Hints.hint(ui.shared.hint, ui._types, ui.shared.now))
        message: ui.shared.message
        showSwitchUser: ui.adapter ? ui.adapter.canSwitchUser : false
        bottomInset: osk.occupied

        onSubmitted: password => ui.submit(password)
        onBackRequested: ui.closeSignIn()
        onSwitchUserRequested: {
            if (ui.adapter)
                ui.adapter.switchUser()
        }
    }

    Connections {
        target: column.passwordField
        function onTextChanged() {
            // what is typed on this screen shows on the others
            if (ui.shared.password !== column.passwordField.text)
                ui.shared.password = column.passwordField.text
            ui.poke()
        }
    }

    CornerBar {
        id: cornerBar
        objectName: "cornerBar"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: RayePalette.edge
        anchors.bottomMargin: RayePalette.edge + osk.occupied
        opacity: column.opacity
        visible: opacity > 0
        enabled: column.enabled

        networkIcon: Hints.networkIcon(ui.adapter ? ui.adapter.networkIconName : "")
        layoutCode: Hints.layoutCode(ui.adapter ? ui.adapter.layoutShortName : "")
        showLayout: ui.adapter ? ui.adapter.hasMultipleLayouts : false
        canSleep: ui.canSuspend
        canHibernate: ui.canHibernate
        canShutDown: ui.adapter ? ui.adapter.canShutDown : false
        canRestart: ui.adapter ? ui.adapter.canRestart : false
        showPower: canSleep || canHibernate || canShutDown || canRestart
        updatePending: ui.adapter ? ui.adapter.updatesReady === true : false
        keyboardAvailable: ui._wayland ? (ui.adapter ? ui.adapter.sessionKeyboardAvailable : false)
                                       : osk.available
        keyboardOn: ui._wayland ? (ui.adapter ? ui.adapter.sessionKeyboardVisible : false)
                                : ui.shared.keyboardWanted

        onSleepRequested: { ui.closeSignIn(); ui.suspendRequested() }
        onHibernateRequested: { ui.closeSignIn(); ui.hibernateRequested() }
        onShutDownRequested: if (ui.adapter) ui.adapter.shutDown()
        onRestartRequested: if (ui.adapter) ui.adapter.restart()
        onUpdateAndShutDownRequested: ui.updateAndPower("shutdown")
        onUpdateAndRestartRequested: ui.updateAndPower("restart")
        onKeyboardToggled: on => ui.setKeyboard(on)
        onLayoutClicked: {
            if (ui.adapter)
                ui.adapter.nextLayout()
            column.focusField()
        }
    }

    // the on-screen keyboard drawn by the module (X11 greeter: it sets the
    // virtual keyboard input method itself); on Wayland the session's own
    // keyboard is used instead
    OnScreenKeyboard {
        id: osk
        active: !ui._wayland && ui.shared.keyboardWanted && ui.shared.signingIn
    }

    // --- the lock view -----------------------------------------------
    LockView {
        id: lockView
        objectName: "lockView"
        anchors.fill: parent
        shown: !ui.shared.signingIn
        fadeIn: true
        focus: !ui.shared.signingIn
        picture: ui.picture
        networkIcon: Hints.networkIcon(ui.adapter ? ui.adapter.networkIconName : "")
        batteryIcon: ui.adapter ? Hints.batteryIcon(ui.adapter.hasBattery, ui.adapter.batteryPercent,
                                                    ui.adapter.pluggedIn) : ""
        // the key that lifts the curtain only lifts it, like Windows: it
        // is not the first letter of the password
        onDismissRequested: ui.openSignIn("")

        // app badges of unread notifications, when that piece is installed
        statusSlot: Item {
            id: badges
            objectName: "badges"
            width: childrenRect.width
            height: childrenRect.height
            Component.onCompleted: {
                var c = Qt.createComponent(Qt.resolvedUrl("badges/LockBadges.qml"))
                if (c.status === Component.Ready)
                    c.createObject(badges)
            }
        }
    }

    // --- the media card, on both views -------------------------------
    MediaCard {
        id: mediaCard
        objectName: "mediaCard"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: ui.shared.signingIn ? RayePalette.edge + 8 : 80
        model: ui.showMedia && ui.adapter ? ui.adapter.mediaModel : null
        visible: opacity > 0 && osk.occupied <= 0 && !ui.shared.unlocking
        onUsed: ui.poke()
        Behavior on anchors.bottomMargin {
            NumberAnimation { duration: Motion.slow; easing.type: Motion.enter }
        }
    }
}
