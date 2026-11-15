// The sign-in state every screen of the lock screen shows the same.
//
// The greeter draws one view per screen, all in one QML engine. KWin
// sends the keys of a locked session to one of those views only (the
// topmost one), whichever screen the pointer is on. So the screens share
// this state: the sign-in view opens on all of them at once, and what is
// typed shows in the field of every screen, as one field. Whichever view
// gets the keys, the password appears where the user is looking.
//
// One view, the driver, answers the authenticator and runs the timers of
// a try; the others only show the state. The driver changes when its
// screen goes away.
//
// The typed password lives here while it is typed, like in the field,
// and is cleared with it.
import QtQuick
import "hints.js" as Hints

QtObject {
    id: share

    // the sign-in view is open
    property bool signingIn: false
    // a password went out, or waits for PAM to ask
    property bool busy: false
    // PAM has asked for the password and has not had it yet
    property bool secretAsked: false
    // the password went out in this conversation
    property bool responded: false
    // typed before PAM asked: sent as soon as it does
    property bool hasPending: false
    property string pending: ""
    // an error message of this try, shown instead of the stock words
    property string tryError: ""
    property string message: ""
    // PAM let the session in without asking anything
    property bool passwordless: false
    property bool unlocking: false
    property var hint: Hints.initial()
    property double now: 0
    property bool keyboardWanted: false
    // what is typed, the same in every screen's field
    property string password: ""

    // the view that answers the authenticator
    property QtObject driver: null
    readonly property int views: _views.length

    // to every view's column
    signal failRequested(string text)
    signal resetRequested()
    signal focusRequested()
    // input on any screen: the driver's idle timer starts again
    signal poked()

    property var _views: []

    function join(view) {
        if (_views.indexOf(view) < 0)
            _views = _views.concat([view])
        if (!driver)
            driver = view
    }

    function leave(view) {
        _views = _views.filter(function (v) { return v !== view })
        if (driver === view)
            driver = _views.length > 0 ? _views[0] : null
        if (_views.length === 0)
            clear()
    }

    // back to a fresh lock screen: nothing open, nothing typed
    function clear() {
        signingIn = false
        busy = false
        secretAsked = false
        responded = false
        hasPending = false
        pending = ""
        tryError = ""
        message = ""
        passwordless = false
        unlocking = false
        hint = Hints.initial()
        now = 0
        keyboardWanted = false
        password = ""
    }
}
