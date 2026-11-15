// The lock view: the picture, the clock and date at the top, status icons
// at the bottom right, a media slot at the bottom centre, and after 4 s
// without input a faint "Press a key or click to sign in" line.
//
// In (properties):
//   picture       the lock picture (default: the Raye picture)
//   now           the time shown; updated every second by itself unless
//                 clockRunning is false
//   locale        date and time formats (default: Strings.locale)
//   shown         false slides the view up and fades it out (300 ms),
//                 true brings it back; a hidden view takes no input
//   fadeIn        fade in from black when created (Win+L, 200 ms)
//   showHint      allow the "Press a key" line
//   networkIcon   batteryIcon   status icon names, empty = not shown
//   statusSlot    more status items, left of those two
//   mediaSlot     the media card
// Out (signals):
//   dismissRequested(text)  a key, a click, the wheel or a swipe up;
//                           `text` is the printable character typed, if
//                           any, for the password field
//   activity()              any input, mouse movement included (which
//                           wakes the screen but does not dismiss)
import QtQuick

FocusScope {
    id: view

    property alias picture: backdrop.source
    property date now: new Date()
    // false: `now` is left as given (tests, the gallery)
    property bool clockRunning: true
    property var locale: Strings.locale
    property bool shown: true
    property bool fadeIn: false
    property bool showHint: true
    property int hintDelay: Motion.hintDelay
    property string networkIcon: ""
    property string batteryIcon: ""
    property alias statusSlot: extraStatus.data
    property alias mediaSlot: media.data

    readonly property bool hintVisible: hint.opacity > 0

    signal dismissRequested(string text)
    signal activity()

    function poke() {
        hintTimer.restart()
        hint.waiting = true
        activity()
    }

    function dismiss(text) {
        if (!shown)
            return
        poke()
        dismissRequested(text)
    }

    enabled: shown
    // back on the lock view: the hint waits its delay again
    onShownChanged: {
        if (shown) {
            hint.waiting = true
            hintTimer.restart()
        }
    }
    opacity: 1
    visible: opacity > 0
    focus: true

    states: State {
        name: "hidden"
        when: !view.shown
        PropertyChanges { view.opacity: 0; slide.y: -Math.round(view.height * 0.3) }
    }
    transitions: [
        Transition {
            to: "hidden"
            NumberAnimation { properties: "opacity,y"; duration: Motion.slow; easing.type: Motion.enter }
        },
        Transition {
            to: ""
            NumberAnimation { properties: "opacity,y"; duration: Motion.normal; easing.type: Motion.enter }
        }
    ]

    transform: Translate { id: slide }

    Component.onCompleted: {
        if (fadeIn && Motion.normal > 0) {
            opacity = 0
            appear.start()
        }
        hintTimer.start()
    }

    NumberAnimation {
        id: appear
        target: view
        property: "opacity"
        to: 1
        duration: Motion.normal
        easing.type: Motion.enter
    }

    Timer {
        interval: 1000
        repeat: true
        running: view.visible && view.clockRunning
        onTriggered: view.now = new Date()
    }

    Timer {
        id: hintTimer
        interval: view.hintDelay
        onTriggered: hint.waiting = false
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Escape)
            return
        var t = event.text
        var printable = t.length > 0 && t.charCodeAt(0) >= 32 && t.charCodeAt(0) !== 127
                        && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
        view.dismiss(printable ? t : "")
        event.accepted = true
    }

    Backdrop {
        id: backdrop
        anchors.fill: parent
        blurred: false
    }

    // a faint dark gradient behind the clock
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.round(parent.height * 0.45)
        visible: !RayePalette.highContrast
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0x0e / 255, 0x0a / 255, 0x17 / 255, 0.30) }
            GradientStop { position: 1.0; color: Qt.rgba(0x0e / 255, 0x0a / 255, 0x17 / 255, 0.0) }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        property real pressY: 0
        property bool swiped: false
        onPositionChanged: mouse => {
            view.poke()
            if (pressed && !swiped && pressY - mouse.y > 60) {
                swiped = true
                view.dismiss("")
            }
        }
        onPressed: mouse => {
            pressY = mouse.y
            swiped = false
            view.forceActiveFocus()
        }
        onReleased: if (!swiped) view.dismiss("")
        onWheel: view.dismiss("")
    }

    FontMetrics {
        id: clockMetrics
        font: clock.font
    }
    FontMetrics {
        id: dateMetrics
        font: date.font
    }

    // the top of the digits at 11 % of the height
    ShadowText {
        id: clock
        objectName: "clock"
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(view.height * 0.11 - clockMetrics.ascent
                      - clockMetrics.tightBoundingRect("0").y)
        text: Strings.timeText(view.now, view.locale)
        font.family: RayePalette.displayFamily
        font.weight: Font.DemiBold
        font.pixelSize: 128
        font.features: { "tnum": 1 }
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    // the date 18 px under the digits, or under whatever hangs below them
    ShadowText {
        id: date
        objectName: "date"
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(clock.y + clock.baselineOffset
                      + Math.max(4, clockMetrics.tightBoundingRect(clock.text).bottom) + 18
                      - dateMetrics.ascent - dateMetrics.tightBoundingRect("W").y)
        text: {
            var s = Strings.dateText(view.now, view.locale)
            return s.charAt(0).toUpperCase() + s.slice(1)
        }
        font.family: RayePalette.displayFamily
        font.weight: Font.DemiBold
        font.pixelSize: 20
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    Item {
        id: media
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 80
        width: childrenRect.width
        height: childrenRect.height
    }

    ShadowText {
        id: hint
        property bool waiting: true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 36
        text: Strings.tr.pressKey
        font.pixelSize: Math.round(14 * RayePalette.textScale)
        color: RayePalette.textSecondary
        opacity: view.showHint && !waiting && view.shown ? 0.85 : 0
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation { duration: Motion.slow * 2; easing.type: Motion.enter }
        }
    }

    // status icons at the bottom right, on the corner bar's grid
    Row {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: RayePalette.edge
        anchors.bottomMargin: RayePalette.edge
        spacing: 4
        layoutDirection: Qt.RightToLeft

        Repeater {
            model: [view.batteryIcon, view.networkIcon]
            delegate: Item {
                required property string modelData
                visible: modelData !== ""
                width: 40
                height: 40
                Image {
                    anchors.centerIn: parent
                    source: parent.modelData !== "" ? RayePalette.icon(parent.modelData) : ""
                    sourceSize: Qt.size(20, 20)
                    width: 20
                    height: 20
                }
            }
        }

        Row {
            id: extraStatus
            spacing: 4
            layoutDirection: Qt.RightToLeft
        }
    }
}
