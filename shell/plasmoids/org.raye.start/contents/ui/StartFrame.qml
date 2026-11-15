// The inside of the Start popup: Start (pins, recommended, all apps) or
// Search, in the size of 3.3 of the shell design, on the Raye acrylic. The
// acrylic slides and fades in with what is on it, so the panel never shows
// empty. It switches to Search as soon as something is typed. The two
// views are separate files:
//   start/StartView.qml    StartView { signal requestClose; property string pendingText;
//                          property var backend; signal requestSearch(string text) }
//   search/SearchView.qml  SearchView { property string query; property var backend;
//                          signal requestClose; signal requestStart }
// `backend` is Backend.qml (models, actions), made in the background
// after login (main.qml).
import QtQuick
import org.raye.ui
import org.raye.shell

import "startlogic.js" as Logic

FocusScope {
    id: frame

    property int screenWidth: 1280
    property int screenHeight: 800
    property int panelHeight: ShellTokens.taskbarHeight
    // "start" or "search"
    property string mode: "start"
    // the popup is showing: the content slides in, or away
    property bool shown: false
    // waiting for the window's first frames before the slide starts
    property bool presenting: false
    // the views are being made in the background (prepare()), a little
    // at a time; opening finishes them at once
    property bool preparing: false
    // the content was drawn once, nearly clear, while presenting
    property bool primed: false
    // Backend.qml, or a stand-in in tests
    property var backend: null
    // the views are made the first time they are wanted
    property bool startWanted: false

    readonly property var size: Logic.popupSize(screenWidth, screenHeight, panelHeight)
    readonly property Item view: mode === "search" ? searchLoader.item : startLoader.item
    // the view of the mode is made: the panel can show
    readonly property bool ready: mode === "search" ? searchLoader.status === Loader.Ready
                                                    : startLoader.status === Loader.Ready

    signal requestClose()
    // the closing slide is over: the window can go
    signal closed()

    // open in a mode, with text already typed (Win+S, typing on the desktop)
    function reset(newMode, text) {
        preparing = false
        startWanted = true
        mode = newMode === "search" ? "search" : "start"
        if (startLoader.item) {
            startLoader.item.pendingText = ""
            if (startLoader.item.reset)
                startLoader.item.reset()
        }
        if (mode === "search")
            searchLoader.setQuery(text || "")
        frame.forceActiveFocus()
        if (mode === "start" && startLoader.item)
            startLoader.item.forceActiveFocus()
        else if (mode === "search")
            focusSearch()
    }

    // Make both views ahead of time, a little at a time, so the first
    // Start and the first key typed in it have nothing left to build.
    function prepare() {
        if (startLoader.status === Loader.Ready && searchLoader.status === Loader.Ready)
            return
        preparing = true
        startWanted = true
        searchLoader.wanted = true
    }

    // Show the content. The slide waits for the window's first frames: one
    // to map it, then one where the content is drawn at 1 % opacity. That
    // first real paint of the views is the slow one (icons, text); done
    // before the slide, it cannot eat the start of the motion, and the
    // panel rises from its first frame instead of jumping into place.
    function present() {
        if (shown)
            return
        primed = false
        presenting = true
        presentFallback.restart()
    }
    function frameDrawn() {
        if (!presenting || !ready)
            return
        if (!primed) {
            primed = true
            if (frame.Window.window)
                frame.Window.window.update()
            return
        }
        startSlide()
    }
    function startSlide() {
        presentFallback.stop()
        if (!presenting)
            return
        presenting = false
        shown = true
    }
    onReadyChanged: if (presenting && ready && frame.Window.window) frame.Window.window.update()
    Connections {
        target: frame.presenting ? frame.Window.window : null
        function onFrameSwapped() { frame.frameDrawn() }
    }
    // a window that never paints still opens
    Timer {
        id: presentFallback
        interval: 400
        onTriggered: frame.startSlide()
    }

    // drop the views (Start makes them again when it opens)
    function release() {
        preparing = false
        startWanted = false
        searchLoader.wanted = false
        mode = "start"
    }

    function switchToStart() {
        mode = "start"
        if (startLoader.item) {
            startLoader.item.pendingText = ""
            startLoader.item.forceActiveFocus()
        }
    }

    function switchToSearch(text) {
        mode = "search"
        searchLoader.setQuery(text)
        focusSearch()
    }

    // the keys go on to the field at once, so nothing typed fast is lost
    function focusSearch() {
        if (searchLoader.item && searchLoader.item.focusField) {
            searchLoader.forceActiveFocus()
            searchLoader.item.focusField()
        }
    }

    width: size.width
    height: size.height
    focus: true

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Escape) {
            frame.requestClose()
            event.accepted = true
            return
        }
        if (frame.mode === "start"
            && Logic.isTypedText(event.text, event.modifiers & Qt.ControlModifier,
                                 event.modifiers & Qt.AltModifier, event.modifiers & Qt.MetaModifier)) {
            frame.switchToSearch(event.text)
            event.accepted = true
        }
    }

    // Each move picks its own curve from the state it goes to; a Behavior
    // reading `shown` could still see the old value and open on the closing
    // curve, slow at first then jumping into place.
    states: [
        State {
            name: "shown"
            when: frame.shown
            PropertyChanges { content.y: 0; content.opacity: 1 }
        },
        State {
            name: "presenting"
            when: frame.presenting
            PropertyChanges { content.opacity: 0.01 }
        }
    ]
    transitions: [
        Transition {
            to: "shown"
            NumberAnimation {
                target: content
                properties: "y,opacity"
                duration: ShellTokens.durationNormal
                easing: Motion.entrance
            }
        },
        Transition {
            from: "shown"
            to: ""
            SequentialAnimation {
                NumberAnimation {
                    target: content
                    properties: "y,opacity"
                    duration: ShellTokens.durationFast
                    easing: Motion.exit
                }
                ScriptAction { script: frame.closed() }
            }
        }
    ]

    Item {
        id: content
        objectName: "startContent"
        width: frame.width
        height: frame.height
        y: ShellTokens.flyoutSlide
        opacity: 0

        // the panel itself: its shadow and acrylic go with what is on them
        Repeater {
            model: 4
            Rectangle {
                required property int index
                anchors.fill: parent
                anchors.margins: -(index + 1) * 3
                anchors.topMargin: -(index + 1) * 3 + 6
                radius: ShellTokens.radiusFlyout + (index + 1) * 3
                color: Qt.rgba(0, 0, 0, Tokens.night ? 0.07 : 0.03)
            }
        }
        AcrylicSurface {
            id: surface
            objectName: "startSurface"
            anchors.fill: parent
            focused: frame.activeFocus
            // no frost behind the nearly clear first paint
            blur: !frame.presenting && content.opacity > 0
        }

        Loader {
            id: startLoader
            objectName: "startLoader"
            anchors.fill: parent
            active: frame.startWanted
            asynchronous: frame.preparing
            source: "start/StartView.qml"
            onLoaded: item.backend = Qt.binding(() => frame.backend)
            focus: frame.mode === "start"
            opacity: frame.mode === "start" ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint } }

            Connections {
                target: startLoader.item
                ignoreUnknownSignals: true
                function onRequestClose() { frame.requestClose() }
                function onRequestSearch(text) { frame.switchToSearch(text) }
            }
        }

        Loader {
            id: searchLoader
            objectName: "searchLoader"
            anchors.fill: parent
            active: wanted
            asynchronous: frame.preparing
            source: "search/SearchView.qml"
            focus: frame.mode === "search"
            opacity: frame.mode === "search" ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint } }

            // loaded the first time Search is wanted, kept until release()
            property bool wanted: false
            property string pendingQuery: ""
            function setQuery(text) {
                pendingQuery = text
                wanted = true
                if (item)
                    item.query = text
            }
            onLoaded: {
                item.backend = Qt.binding(() => frame.backend)
                item.query = pendingQuery
            }

            Connections {
                target: searchLoader.item
                ignoreUnknownSignals: true
                function onRequestClose() { frame.requestClose() }
                function onRequestStart() { frame.switchToStart() }
            }
        }
    }
}
