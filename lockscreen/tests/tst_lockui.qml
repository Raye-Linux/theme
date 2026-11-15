import QtQuick
import QtTest
import org.raye.signin
import "../../signin/tests/Setup.js" as Setup
import ".."

TestCase {
    id: test
    name: "LockUi"
    width: 1280
    height: 800
    when: windowShown
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
        Strings.lang = "en"
        Motion.reducedMotion = true
    }

    function cleanupTestCase() {
        Motion.reducedMotion = false
    }

    // a test that turns the animations on gets them off again, even when
    // it fails
    function cleanup() {
        Motion.reducedMotion = true
    }

    // A stand-in for the greeter's authenticators: records what the lock
    // screen asks, and lets the test play PAM's part.
    Component {
        id: fakeComponent
        QtObject {
            property bool busy: false
            property bool hadPrompt: false
            property string prompt: ""
            property string promptForSecret: ""
            property string infoMessage: ""
            property string errorMessage: ""
            property int authenticatorTypes: 0
            property int starts: 0
            property var responses: []

            signal succeeded()
            signal failed(int kind, var authenticator)
            signal noninteractiveInfo(int kind, var authenticator)
            signal noninteractiveError(int kind, var authenticator)

            function startAuthenticating() { starts += 1 }
            function respond(p) { responses = responses.concat([p]) }

            function askPassword() {
                hadPrompt = true
                promptForSecret = "Password: "
                promptForSecretChanged()
            }
        }
    }

    // the session pieces, without the session
    Component {
        id: adapterComponent
        QtObject {
            property bool canSwitchUser: true
            property bool canShutDown: true
            property bool canRestart: true
            property bool capsLock: false
            property bool hasMultipleLayouts: false
            property string layoutShortName: "us"
            property bool hasBattery: true
            property real batteryPercent: 64
            property bool pluggedIn: false
            property string networkIconName: "network-wireless-80"
            property var mediaModel: null
            property bool sessionKeyboardAvailable: false
            property bool sessionKeyboardVisible: false
            property bool reducedMotion: true
            property bool updatesReady: false
            property var arms: []
            property int switches: 0
            property int shutdowns: 0
            property int restarts: 0
            property int layoutSwitches: 0
            signal aboutToSuspend()
            signal updateArmFailed(string reason)
            function armUpdates(then) { arms = arms.concat([then]) }
            function switchUser() { switches += 1 }
            function shutDown() { shutdowns += 1 }
            function restart() { restarts += 1 }
            function nextLayout() { layoutSwitches += 1 }
            function showSessionKeyboard(on) {}
        }
    }

    Component {
        id: uiComponent
        LockUi {
            width: 1280
            height: 800
            userName: "Anna Roy"
        }
    }

    SignalSpy { id: unlockedSpy; signalName: "unlocked" }
    SignalSpy { id: suspendSpy; signalName: "suspendRequested" }

    property var auth: null
    property var adapter: null

    function make(props) {
        auth = createTemporaryObject(fakeComponent, test)
        adapter = createTemporaryObject(adapterComponent, test)
        var p = props || {}
        if (p.authenticator === undefined)
            p.authenticator = auth
        if (p.adapter === undefined)
            p.adapter = adapter
        var ui = createTemporaryObject(uiComponent, test, p)
        verify(ui)
        unlockedSpy.target = ui
        unlockedSpy.clear()
        suspendSpy.target = ui
        suspendSpy.clear()
        ui.lockView.clockRunning = false
        ui.forceActiveFocus()
        return ui
    }

    function signIn(ui, text) {
        ui.openSignIn(text || "")
        compare(ui.signingIn, true)
    }

    // --- views --------------------------------------------------------

    function test_startsOnTheLockView() {
        var ui = make()
        compare(ui.signingIn, false)
        verify(ui.lockView.shown)
        compare(ui.column.visible, false)
        compare(ui.cornerBar.visible, false)
        // nothing is asked before the user wants to sign in
        compare(auth.starts, 0)
    }

    // the key that lifts the curtain is not typed; the next ones are
    function test_keyOnCurtainOnlyLiftsIt() {
        var ui = make()
        keyClick(Qt.Key_A)
        compare(ui.signingIn, true)
        compare(ui.column.passwordField.text, "")
        compare(auth.starts, 1)
        verify(ui.column.visible)
        verify(ui.cornerBar.visible)
        verify(ui.column.passwordField.inputItem.activeFocus)
        keyClick(Qt.Key_B)
        compare(ui.column.passwordField.text, "b")
    }

    function test_spaceEnterOnCurtainAddNothing_data() {
        return [{ tag: "space", key: Qt.Key_Space },
                { tag: "enter", key: Qt.Key_Return },
                { tag: "digit", key: Qt.Key_1 }]
    }

    function test_spaceEnterOnCurtainAddNothing(data) {
        var ui = make()
        keyClick(data.key)
        compare(ui.signingIn, true)
        compare(ui.column.passwordField.text, "")
        // nothing went to PAM either
        compare(auth.responses.length, 0)
    }

    function test_clickOpensSignIn() {
        var ui = make()
        mouseClick(ui, 400, 300)
        compare(ui.signingIn, true)
        compare(ui.column.passwordField.text, "")
    }

    function test_escapeGoesBackAndClears() {
        var ui = make()
        signIn(ui, "abc")
        compare(ui.column.passwordField.text, "abc")
        keyClick(Qt.Key_Escape)
        compare(ui.signingIn, false)
        compare(ui.column.passwordField.text, "")
        // back on the curtain, the next key lifts it again
        verify(ui.lockView.activeFocus)
        keyClick(Qt.Key_Space)
        compare(ui.signingIn, true)
        compare(ui.column.passwordField.text, "")
    }

    // with the animations on, the curtain fades back in: it must still
    // take the keys once it is there
    function test_escapeThenKeyWithAnimations() {
        Motion.reducedMotion = false
        var ui = make()
        // the curtain's own fade in from black (Win+L) ends first
        tryVerify(function () { return ui.lockView.opacity > 0.99 })
        signIn(ui, "abc")
        tryVerify(function () { return !ui.lockView.visible })
        keyClick(Qt.Key_Escape)
        compare(ui.signingIn, false)
        tryVerify(function () { return ui.lockView.opacity > 0.99 })
        verify(ui.lockView.activeFocus)
        keyClick(Qt.Key_Space)
        compare(ui.signingIn, true)
        compare(ui.column.passwordField.text, "")
    }

    function test_userAndPicture() {
        var ui = make({ userImagePath: "/var/lib/AccountsService/icons/anna" })
        compare(ui.column.userName, "Anna Roy")
        compare(ui.column.userPicture.toString(), "file:///var/lib/AccountsService/icons/anna")
    }

    function test_defaultPictureWhenNoneSet() {
        var ui = make()
        verify(RayePalette.isDefaultPicture(ui.picture))
        var other = make({ wallpaperImage: "/no/such/picture.jpg" })
        // a picture that does not load falls back to Raye's own
        tryVerify(function () { return RayePalette.isDefaultPicture(other.picture) })
    }

    function test_customPictureWhenItLoads() {
        var file = Qt.resolvedUrl("../../assets/signin/face-256.png").toString().slice(7)
        var ui = make({ wallpaperImage: decodeURIComponent(file) })
        tryCompare(ui, "picture", Qt.resolvedUrl("../../assets/signin/face-256.png"))
    }

    // --- password -----------------------------------------------------

    function test_rightPasswordUnlocks() {
        var ui = make()
        signIn(ui)
        auth.askPassword()
        ui.column.passwordField.insert("secret")
        keyClick(Qt.Key_Return)
        compare(auth.responses, ["secret"])
        compare(ui.busy, true)
        compare(ui.column.busy, true)
        auth.succeeded()
        compare(ui.unlocking, true)
        compare(unlockedSpy.count, 1)
        // the field is emptied at once
        compare(ui.column.passwordField.text, "")
    }

    function test_passwordTypedBeforePamAsksIsSentWhenItDoes() {
        var ui = make()
        signIn(ui, "x")
        ui.submit("early")
        compare(auth.responses, [])
        compare(ui.busy, true)
        auth.askPassword()
        compare(auth.responses, ["early"])
    }

    function test_submitWhenNotAuthenticatingStartsIt() {
        var ui = make()
        signIn(ui)
        auth.starts = 0
        ui.submit("pw")
        compare(auth.starts, 1)
    }

    function test_wrongPassword() {
        var ui = make()
        signIn(ui)
        auth.askPassword()
        ui.submit("nope")
        auth.failed(0, null)
        compare(ui.busy, false)
        compare(ui.column.failed, true)
        compare(ui.column.errorText, Strings.tr.wrongPassword)
        compare(ui.column.passwordField.text, "")
        // a new conversation starts for the next try
        var before = auth.starts
        tryVerify(function () { return auth.starts > before })
        // and the next password goes to the next prompt, not this one
        auth.askPassword()
        ui.submit("again")
        compare(auth.responses, ["nope", "again"])
        compare(unlockedSpy.count, 0)
    }

    function test_pamErrorReplacesStockWords() {
        var ui = make()
        signIn(ui)
        auth.askPassword()
        ui.submit("nope")
        auth.errorMessage = "The account is locked for 2 minutes."
        auth.errorMessageChanged()
        auth.failed(0, null)
        compare(ui.column.errorText, "The account is locked for 2 minutes.")
    }

    function test_failureBeforeThePasswordIsNotAWrongPassword() {
        var ui = make()
        signIn(ui)
        ui.submit("early")            // PAM has not asked yet
        auth.failed(0, null)          // e.g. a start that came too soon
        compare(ui.column.failed, false)
        compare(ui.busy, true)
        var before = auth.starts
        // no tight loop: nothing starts again right away
        wait(100)
        compare(auth.starts, before)
        tryVerify(function () { return auth.starts > before })
        auth.askPassword()
        compare(auth.responses, ["early"])
    }

    function test_pendingPasswordIsNotKeptForever() {
        var ui = make()
        signIn(ui)
        ui.submit("early")
        compare(ui.busy, true)
        var timer = findChild(ui, "waitingTimer")
        verify(timer.running)
        timer.triggered()
        compare(ui.busy, false)
        // PAM asking later gets nothing from the old try
        auth.askPassword()
        compare(auth.responses, [])
    }

    function test_escapeDropsAWaitingTry() {
        var ui = make()
        signIn(ui)
        ui.submit("early")
        compare(ui.busy, true)
        keyClick(Qt.Key_Escape)
        compare(ui.signingIn, false)
        compare(ui.busy, false)
        auth.askPassword()
        compare(auth.responses, [])
        signIn(ui)
        ui.submit("now")
        compare(auth.responses, ["now"])
    }

    function test_failedRestartIsDelayed() {
        var ui = make()
        signIn(ui)
        auth.askPassword()
        ui.submit("nope")
        var before = auth.starts
        auth.failed(0, null)
        wait(100)
        compare(auth.starts, before)
        // typing the next password during the pause waits for it too
        ui.submit("next")
        compare(auth.starts, before)
        tryVerify(function () { return auth.starts > before })
        auth.askPassword()
        compare(auth.responses, ["nope", "next"])
    }

    function test_readerFailureIsNotAWrongPassword() {
        var ui = make()
        signIn(ui)
        auth.askPassword()
        ui.submit("pw")
        auth.failed(1, null)
        compare(ui.column.failed, false)
        compare(ui.busy, true)
    }

    function test_infoMessagesShow() {
        var ui = make()
        signIn(ui)
        auth.infoMessage = "Your password expires in 3 days"
        auth.infoMessageChanged()
        compare(ui.message, "Your password expires in 3 days")
    }

    function test_secondPromptInSameConversation() {
        var ui = make()
        signIn(ui)
        auth.askPassword()
        ui.submit("old")
        auth.promptForSecret = "New password: "
        auth.promptForSecretChanged()
        compare(ui.busy, false)
        compare(ui.message, "New password: ")
        ui.submit("new")
        compare(auth.responses, ["old", "new"])
    }

    function test_noPromptSuccessShowsSignInButton() {
        var ui = make()
        signIn(ui)
        auth.succeeded()          // hadPrompt is false
        compare(unlockedSpy.count, 0)
        compare(ui.column.needsPassword, false)
        ui.submit("")
        compare(unlockedSpy.count, 1)
    }

    function test_graceTimeAsksNothing() {
        var ui = make({ locked: false })
        signIn(ui)
        compare(auth.starts, 0)
        ui.locked = true
        compare(auth.starts, 1)
    }

    function test_noAuthenticatorIsHarmless() {
        var ui = make({ authenticator: null })
        signIn(ui, "a")
        ui.submit("a")
        compare(ui.busy, false)
        compare(unlockedSpy.count, 0)
    }

    // --- method line ----------------------------------------------------

    function test_noFingerprintLineWithoutReader() {
        var ui = make()
        signIn(ui)
        // listed as soon as asking starts, but no reader ever speaks
        auth.authenticatorTypes = 1
        auth.authenticatorTypesChanged()
        compare(ui.methodHint, "")
        auth.authenticatorTypes = 0
        auth.authenticatorTypesChanged()
        compare(ui.methodHint, "")
    }

    function test_fingerprintLineAfterReaderSpeaks() {
        var ui = make()
        signIn(ui)
        auth.authenticatorTypes = 1
        auth.authenticatorTypesChanged()
        auth.noninteractiveInfo(1, null)
        compare(ui.methodHint, Strings.tr.fingerprint)
        auth.noninteractiveError(1, null)
        compare(ui.methodHint, "That finger wasn't recognized. Try again.")
        auth.authenticatorTypes = 0
        auth.authenticatorTypesChanged()
        compare(ui.methodHint, "")
    }

    function test_smartcardLineNever() {
        var ui = make()
        signIn(ui)
        auth.authenticatorTypes = 2
        auth.authenticatorTypesChanged()
        auth.noninteractiveInfo(2, null)
        auth.noninteractiveError(2, null)
        compare(ui.methodHint, "")
    }

    function test_frenchWords() {
        Strings.lang = "fr"
        var ui = make()
        signIn(ui)
        auth.authenticatorTypes = 1
        auth.authenticatorTypesChanged()
        auth.noninteractiveInfo(1, null)
        compare(ui.methodHint, "Posez votre doigt sur le lecteur d'empreintes")
        Strings.lang = "en"
    }

    // --- session pieces ------------------------------------------------

    function test_switchUser() {
        var ui = make()
        signIn(ui)
        compare(ui.column.showSwitchUser, true)
        ui.column.switchUserRequested()
        compare(adapter.switches, 1)
        adapter.canSwitchUser = false
        compare(ui.column.showSwitchUser, false)
    }

    function test_capsLockLine() {
        var ui = make()
        signIn(ui)
        compare(ui.column.capsLock, false)
        adapter.capsLock = true
        compare(ui.column.capsLock, true)
    }

    function test_statusIcons() {
        var ui = make()
        compare(ui.lockView.batteryIcon, "battery-060")
        compare(ui.lockView.networkIcon, "network-wireless-4")
        adapter.hasBattery = false
        compare(ui.lockView.batteryIcon, "")
    }

    function test_powerRows() {
        var ui = make({ canSuspend: true, canHibernate: false })
        signIn(ui)
        var ids = ui.cornerBar.powerFlyout.rows.map(function (r) { return r.id })
        compare(ids, ["sleep", "shutDown", "restart"])
        ui.cornerBar.sleepRequested()
        compare(suspendSpy.count, 1)
        compare(ui.signingIn, false)
        ui.cornerBar.shutDownRequested()
        compare(adapter.shutdowns, 1)
        ui.cornerBar.restartRequested()
        compare(adapter.restarts, 1)
    }

    function rowIds(ui) {
        return ui.cornerBar.powerFlyout.rows.map(function (r) { return r.id })
    }

    function test_updateRowsOnlyWhenReady() {
        var ui = make({ canSuspend: false })
        signIn(ui)
        compare(rowIds(ui), ["shutDown", "restart"])
        adapter.updatesReady = true
        compare(rowIds(ui), ["updateShutDown", "shutDown", "updateRestart", "restart"])
    }

    function test_updateAndRestartArms() {
        var ui = make()
        adapter.updatesReady = true
        signIn(ui)
        ui.cornerBar.updateAndRestartRequested()
        compare(adapter.arms, ["restart"])
        compare(ui.arming, true)
        compare(adapter.restarts, 0)
        // a second click while Raye Update works does nothing
        ui.cornerBar.updateAndRestartRequested()
        compare(adapter.arms, ["restart"])
    }

    function test_updateAndShutDownArms() {
        var ui = make()
        adapter.updatesReady = true
        signIn(ui)
        ui.cornerBar.updateAndShutDownRequested()
        compare(adapter.arms, ["shutdown"])
        compare(adapter.shutdowns, 0)
    }

    function test_cancelledArmStaysLocked() {
        var ui = make()
        adapter.updatesReady = true
        signIn(ui, "abc")
        ui.cornerBar.updateAndRestartRequested()
        adapter.updateArmFailed("cancelled")
        compare(ui.arming, false)
        compare(adapter.restarts, 0)
        compare(adapter.shutdowns, 0)
        compare(ui.signingIn, true)
        compare(unlockedSpy.count, 0)
    }

    function test_failedArmStillPowersOff() {
        var ui = make()
        adapter.updatesReady = true
        signIn(ui)
        ui.cornerBar.updateAndShutDownRequested()
        adapter.updateArmFailed("nothing-ready")
        compare(adapter.shutdowns, 1)
        ui.cornerBar.updateAndRestartRequested()
        adapter.updateArmFailed("failed")
        compare(adapter.restarts, 1)
    }

    function test_strayArmFailureIgnored() {
        var ui = make()
        adapter.updatesReady = true
        adapter.updateArmFailed("failed")
        compare(adapter.restarts, 0)
        compare(adapter.shutdowns, 0)
    }

    function test_noPowerButtonWithoutPower() {
        var ui = make({ canSuspend: false })
        adapter.canShutDown = false
        adapter.canRestart = false
        compare(ui.cornerBar.showPower, false)
    }

    function test_layoutButton() {
        var ui = make()
        signIn(ui)
        compare(ui.cornerBar.showLayout, false)
        adapter.hasMultipleLayouts = true
        adapter.layoutShortName = "fr"
        compare(ui.cornerBar.showLayout, true)
        compare(ui.cornerBar.layoutCode, "FR")
        ui.cornerBar.layoutClicked()
        compare(adapter.layoutSwitches, 1)
    }

    function test_suspendGoesBackToPicture() {
        var ui = make()
        signIn(ui, "abc")
        adapter.aboutToSuspend()
        compare(ui.signingIn, false)
        compare(ui.column.passwordField.text, "")
    }

    function test_noAdapterIsHarmless() {
        var ui = make({ adapter: null })
        signIn(ui)
        compare(ui.column.showSwitchUser, false)
        compare(ui.lockView.batteryIcon, "")
        compare(ui.mediaCard.shown, false)
    }

    // --- media card ----------------------------------------------------

    ListModel {
        id: players
    }

    // records the MPRIS calls the card makes through its invoke hook (a
    // QML object cannot declare methods starting with a capital, and a
    // ListModel cannot hold an object, so the model hands out a name)
    QtObject {
        id: player
        property int calls: 0
        property string last: ""
    }

    function addPlayer(status) {
        players.clear()
        players.append({ track: "Night Drive", artist: "Raye Band", identity: "Player",
                         artUrl: "", playbackStatus: status, canControl: true,
                         canGoPrevious: true, canGoNext: false, container: "player-1" })
    }

    function test_mediaCardFollowsPlayback() {
        var ui = make()
        adapter.mediaModel = players
        players.clear()
        compare(ui.mediaCard.shown, false)
        addPlayer(2)
        tryCompare(ui.mediaCard, "shown", true)
        var title = findChild(ui.mediaCard, "mediaTitle")
        compare(title.text, "Night Drive")
        compare(findChild(ui.mediaCard, "mediaArtist").text, "Raye Band")
        players.setProperty(0, "playbackStatus", 1)   // stopped
        tryCompare(ui.mediaCard, "shown", false)
        players.setProperty(0, "playbackStatus", 3)   // paused
        tryCompare(ui.mediaCard, "shown", true)
        players.clear()
        tryCompare(ui.mediaCard, "shown", false)
    }

    function test_mediaButtons() {
        var ui = make()
        adapter.mediaModel = players
        addPlayer(2)
        tryCompare(ui.mediaCard, "shown", true)
        player.calls = 0
        ui.mediaCard.invoke = function (c, name) {
            compare(c, "player-1")
            player.calls += 1
            player.last = name
        }
        var play = findChild(ui.mediaCard, "playButton")
        compare(play.Accessible.name, "Pause")
        mouseClick(play)
        compare(player.last, "PlayPause")
        var prev = findChild(ui.mediaCard, "previousButton")
        mouseClick(prev)
        compare(player.last, "Previous")
        var next = findChild(ui.mediaCard, "nextButton")
        compare(next.enabled, false)
        // a click on the card does not open the sign-in view
        compare(ui.signingIn, false)
    }

    function test_mediaHiddenWhenTurnedOff() {
        var ui = make({ showMedia: false })
        adapter.mediaModel = players
        addPlayer(2)
        compare(ui.mediaCard.shown, false)
    }

    // --- the clock ------------------------------------------------------

    // The lock screen draws the sign-in module's clock. A 12-hour region
    // must keep its AM/PM: without it, 3 in the morning and 3 in the
    // afternoon are the very same screen.
    function test_clockKeepsAmPm() {
        var ui = make()
        var clock = findChild(ui.lockView, "clock")
        verify(clock)
        ui.lockView.locale = Qt.locale("en_US")
        ui.lockView.now = new Date(2026, 9, 29, 3, 5)
        var morning = clock.text
        ui.lockView.now = new Date(2026, 9, 29, 15, 5)
        var afternoon = clock.text
        verify(morning !== afternoon, "both read \"" + morning + "\"")
        compare(morning, "3:05 AM")
        compare(afternoon, "3:05 PM")
        // a 24-hour region gains nothing
        ui.lockView.locale = Qt.locale("fr_FR")
        compare(clock.text, "15:05")
    }

    // --- idle -----------------------------------------------------------

    function test_idleGoesBackToPicture() {
        var ui = make()
        signIn(ui, "abc")
        var timer = null
        for (var i = 0; i < ui.data.length; ++i)
            if (ui.data[i].interval === Motion.idleToLock)
                timer = ui.data[i]
        verify(timer)
        verify(timer.running)
        timer.triggered()
        compare(ui.signingIn, false)
        compare(ui.column.passwordField.text, "")
    }
}
