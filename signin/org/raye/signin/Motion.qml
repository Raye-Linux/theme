// Timings of the sign-in screens.
//
// Things come in with OutCubic and leave with InCubic. Each adapter sets
// reducedMotion (lock screen and splash from the user's animation speed,
// SDDM never): every duration then becomes 0, so nothing moves and every
// state is still reached.
pragma Singleton

import QtQuick

QtObject {
    property bool reducedMotion: false

    readonly property int fast: reducedMotion ? 0 : 120
    readonly property int normal: reducedMotion ? 0 : 200
    readonly property int slow: reducedMotion ? 0 : 300

    readonly property int enter: Easing.OutCubic
    readonly property int leave: Easing.InCubic

    // wrong password: the field shakes 3 times, 6 px, in 300 ms
    readonly property int shakeDuration: 300
    readonly property int shakeDistance: 6
    readonly property int shakeCount: 3

    // one turn of the spinner
    readonly property int spinnerTurn: 1100

    // lock view: the "Press a key" line after 4 s without input
    readonly property int hintDelay: 4000
    // sign-in view: back to the lock view after 60 s, black after 10 min
    readonly property int idleToLock: 60000
    readonly property int idleToBlack: 600000

    // Start menu's Shut down / Restart / Sign out: the words show this
    // long before the request goes out
    readonly property int progressDelay: 500

    // first sign-in: words change every 4 s, colours turn in 20 s, and
    // the screen stays at least 3 s
    readonly property int firstSignInStep: 4000
    readonly property int firstSignInCycle: 20000
    readonly property int minimumShown: 3000
}
