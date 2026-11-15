// The full screens with words and a spinner, all on the same backdrop so
// greeter -> splash -> desktop and desktop -> shut down read as one screen.
//
// mode:
//   "welcome"       spinner + "Welcome"
//   "signingOut"    spinner + "Signing out"
//   "shuttingDown"  spinner + "Shutting down"
//   "restarting"    spinner + "Restarting"
//   "updating"      spinner + "Working on updates  37%" (progress >= 0)
//                   and "Don't turn off your PC"
//   "firstSignIn"   a slowly shifting night gradient, the eye, "Hi" ->
//                   "We're getting things ready" -> "This might take a
//                   minute", and "Don't turn off your PC" at the bottom
// In:  picture (the backdrop's), showBackdrop (false: draw only the words,
//      over whatever is behind, e.g. the SDDM view after sign-in), progress
// Out: minimumShown becomes true 3 s after the screen (or the mode) began;
//      the splash waits for it so the first sign-in is never a flash
import QtQuick

Item {
    id: screen

    property string mode: "welcome"
    property int progress: -1
    property bool showBackdrop: true
    property alias picture: backdrop.source

    readonly property bool minimumShown: !minimumTimer.running
    // the words on screen, for tests and screen readers
    readonly property string title: shownTitle.text
    readonly property string subtitle: shownSubtitle.visible ? shownSubtitle.text : ""
    readonly property string firstSignInText: firstSignInLabel.text

    function titleFor(m) {
        switch (m) {
        case "welcome": return Strings.tr.welcome
        case "signingOut": return Strings.tr.signingOut
        case "shuttingDown": return Strings.tr.shuttingDown
        case "restarting": return Strings.tr.restarting
        case "updating":
            return progress >= 0
                   ? Strings.tr.workingOnUpdates + "  " + Strings.tr.percent.arg(progress)
                   : Strings.tr.workingOnUpdates
        }
        return ""
    }

    property string _shownMode: mode

    onModeChanged: {
        minimumTimer.restart()
        if (Motion.normal === 0 || !visible) {
            _shownMode = mode
            return
        }
        swap.restart()
    }

    Component.onCompleted: minimumTimer.start()

    Timer {
        id: minimumTimer
        interval: Motion.minimumShown
    }

    SequentialAnimation {
        id: swap
        NumberAnimation { target: words; property: "opacity"; to: 0; duration: Motion.fast; easing.type: Motion.leave }
        ScriptAction { script: screen._shownMode = screen.mode }
        NumberAnimation { target: words; property: "opacity"; to: 1; duration: Motion.normal; easing.type: Motion.enter }
    }

    Backdrop {
        id: backdrop
        anchors.fill: parent
        visible: screen.showBackdrop && screen._shownMode !== "firstSignIn"
        blurred: true
    }

    // --- spinner and words ---------------------------------------------
    Column {
        id: words
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -Math.round(screen.height * 0.04)
        spacing: 24
        visible: screen._shownMode !== "firstSignIn"

        Spinner {
            anchors.horizontalCenter: parent.horizontalCenter
            size: 32
            running: words.visible && screen.visible
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8

            ShadowText {
                id: shownTitle
                anchors.horizontalCenter: parent.horizontalCenter
                text: screen.titleFor(screen._shownMode)
                font.family: RayePalette.displayFamily
                font.weight: Font.DemiBold
                font.pixelSize: Math.round(28 * RayePalette.textScale)
                font.features: { "tnum": 1 }
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            ShadowText {
                id: shownSubtitle
                anchors.horizontalCenter: parent.horizontalCenter
                visible: screen._shownMode === "updating"
                text: Strings.tr.dontTurnOff
                font.pixelSize: Math.round(20 * RayePalette.textScale)
                color: RayePalette.text
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }
    }

    // --- first sign-in ---------------------------------------------------
    Item {
        id: firstSignIn
        anchors.fill: parent
        visible: screen._shownMode === "firstSignIn"

        property int phrase: 0
        readonly property var phrases: [Strings.tr.hi, Strings.tr.gettingReady,
                                        Strings.tr.mightTakeMinute]

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { id: topStop; position: 0.0; color: RayePalette.night }
                GradientStop { id: bottomStop; position: 1.0; color: RayePalette.nightHigh }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: RayePalette.highContrast
            color: "#000000"
        }

        // the colours turn through night, graphite and a dark violet in 20 s
        SequentialAnimation {
            running: firstSignIn.visible && !Motion.reducedMotion && !RayePalette.highContrast
            loops: Animation.Infinite
            ParallelAnimation {
                ColorAnimation { target: topStop; property: "color"; to: RayePalette.nightHigh; duration: Motion.firstSignInCycle / 3; easing.type: Easing.InOutSine }
                ColorAnimation { target: bottomStop; property: "color"; to: "#241e35"; duration: Motion.firstSignInCycle / 3; easing.type: Easing.InOutSine }
            }
            ParallelAnimation {
                ColorAnimation { target: topStop; property: "color"; to: "#241e35"; duration: Motion.firstSignInCycle / 3; easing.type: Easing.InOutSine }
                ColorAnimation { target: bottomStop; property: "color"; to: RayePalette.night; duration: Motion.firstSignInCycle / 3; easing.type: Easing.InOutSine }
            }
            ParallelAnimation {
                ColorAnimation { target: topStop; property: "color"; to: RayePalette.night; duration: Motion.firstSignInCycle / 3; easing.type: Easing.InOutSine }
                ColorAnimation { target: bottomStop; property: "color"; to: RayePalette.nightHigh; duration: Motion.firstSignInCycle / 3; easing.type: Easing.InOutSine }
            }
        }

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.round(parent.height * 0.12)
            source: RayePalette.eye
            sourceSize: Qt.size(72 * Screen.devicePixelRatio, 72 * Screen.devicePixelRatio)
            width: 72
            height: 72
            fillMode: Image.PreserveAspectFit
            Accessible.ignored: true
        }

        ShadowText {
            id: firstSignInLabel
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width - 96)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: firstSignIn.phrases[firstSignIn.phrase]
            font.family: RayePalette.displayFamily
            font.weight: Font.DemiBold
            font.pixelSize: Math.round(40 * RayePalette.textScale)
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        // "Hi", then the two others in turn, every 4 s
        Timer {
            interval: Motion.firstSignInStep
            repeat: true
            running: firstSignIn.visible
            onTriggered: {
                var next = firstSignIn.phrase === 2 ? 1 : firstSignIn.phrase + 1
                if (Motion.normal === 0) {
                    firstSignIn.phrase = next
                } else {
                    phraseSwap.next = next
                    phraseSwap.restart()
                }
            }
        }

        SequentialAnimation {
            id: phraseSwap
            property int next: 0
            NumberAnimation { target: firstSignInLabel; property: "opacity"; to: 0; duration: Motion.slow * 2; easing.type: Motion.leave }
            ScriptAction { script: firstSignIn.phrase = phraseSwap.next }
            NumberAnimation { target: firstSignInLabel; property: "opacity"; to: 1; duration: Motion.slow * 2; easing.type: Motion.enter }
        }

        onVisibleChanged: if (visible) phrase = 0

        ShadowText {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 48
            text: Strings.tr.dontTurnOff
            font.pixelSize: Math.round(14 * RayePalette.textScale)
            color: RayePalette.textSecondary
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
    }
}
