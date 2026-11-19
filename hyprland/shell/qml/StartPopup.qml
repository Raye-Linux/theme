// Start and Search: the Raye Start applet's own frame and backend, in a
// flyout above the taskbar. The backend is made a while after login and
// kept, so Start opens with everything on it.
import QtQuick
import org.raye.hypr
import org.raye.shell

Flyout {
    id: popup

    layerName: "raye-start"
    content: frame.item
    readonly property bool open: visible && !!frame.item && (frame.item.shown || frame.item.presenting)
    readonly property string mode: frame.item ? frame.item.mode : "start"
    property Item anchorButton: null

    onOutsideClicked: close()

    Loader {
        id: backend
        active: false
        asynchronous: true
        source: Views.url("org.raye.start", "Backend.qml")
        onLoaded: {
            item.applet = null
            item.done.connect(popup.close)
            item.appsWanted = true
            item.categoriesWanted = true
        }
    }
    Timer {
        interval: 10000
        running: true
        onTriggered: {
            backend.active = true
            if (frame.item)
                frame.item.prepare()
        }
    }

    Loader {
        id: frame
        focus: true
        source: Views.url("org.raye.start", "StartFrame.qml")
        onLoaded: {
            item.backend = Qt.binding(() => backend.item)
            item.requestClose.connect(popup.close)
            item.closed.connect(() => popup.visible = false)
        }
        x: {
            const w = frame.item ? frame.item.width : 0
            if (ShellSettings.windows10 || ShellSettings.alignment === "left") {
                const b = popup.anchorButton
                return b ? Math.max(0, b.mapToGlobal(0, 0).x - (popup.screen ? popup.screen.virtualX : 0)) : ShellTokens.edgeMargin
            }
            return Math.round((popup.width - w) / 2)
        }
        y: popup.height - (frame.item ? frame.item.height : 0) - (ShellSettings.windows10 ? 0 : ShellTokens.flyoutGap)
    }

    function show(mode, button) {
        backend.active = true
        anchorButton = button || null
        const f = frame.item
        if (!f)
            return
        f.screenWidth = screen ? screen.width : 1280
        f.screenHeight = screen ? screen.height : 800
        f.panelHeight = ShellTokens.taskbarHeight
        f.reset(mode, "")
        f.present()
        visible = true
        requestActivate()
    }
    function close() {
        const f = frame.item
        if (!visible || !f)
            return
        if (f.presenting) {
            f.presenting = false
            visible = false
        }
        f.shown = false
    }
    function toggle(mode, button) {
        if (open && this.mode === mode)
            close()
        else if (open)
            frame.item.reset(mode, "")
        else
            show(mode, button)
    }
}
