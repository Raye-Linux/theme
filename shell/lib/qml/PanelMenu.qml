// A menu of the taskbar (an app's right-click menu, the taskbar's own,
// the clock's): the rows of a RayeMenu on the Raye acrylic, in a window
// of its own placed above the bar, centred over a point, the flyout gap
// above the taskbar, as Windows draws them. (A RayeMenu opened from the
// panel's window lands on the bar itself.)
//
//   PanelMenu {
//       id: menu
//       RayeMenuItem { text: i18nd("raye_shell_x", "Task Manager"); onTriggered: ... }
//       RayeMenuSeparator {}
//   }
//   ... menu.open(button, button.width / 2)
//
// Rows made later go in with addItem(); clear() takes them all away.
// Up, Down, Home, End, Return and Escape work as in a menu.
import QtQuick
import QtQuick.Templates as T
import org.kde.plasma.core as PlasmaCore
import org.raye.ui
import org.raye.shell

Item {
    id: panelMenu

    default property alias rows: column.data
    readonly property bool opened: window.visible
    // the height of the rows on their surface, which the window takes
    readonly property real contentHeight: frame.implicitHeight
    property int currentIndex: -1
    // rows already set up (width, highlight, closing on a click)
    property var known: []

    signal closed()

    function addItem(item) {
        item.parent = column
    }

    // destroy() only happens later: out of the column now, or the rows of
    // the last opening still count when the new ones are measured and the
    // menu opens twice as tall as what it shows
    function clear() {
        for (let i = column.children.length - 1; i >= 0; --i) {
            const r = column.children[i]
            r.parent = null
            r.destroy()
        }
        known = []
        currentIndex = -1
    }

    // where the menu goes: the centre of its bottom edge, and the screen
    property point bottomCentre
    property rect screenRect
    // the point given to open() is the menu's left edge, not its centre
    // (the quick link menu lines up with the Start button)
    property bool fromLeft: false

    function open(item, at) {
        currentIndex = -1
        const p = item.mapToGlobal(at, 0)
        bottomCentre = Qt.point(p.x, p.y - (ShellTokens.taskbarHeight - item.height) / 2 - ShellTokens.flyoutGap)
        const w = item.Window.window
        screenRect = w && w.screen ? Qt.rect(w.screen.virtualX, w.screen.virtualY, w.screen.width, w.screen.height)
                                   : Qt.rect(0, 0, 0, 0)
        // the window keeps the size of a longer menu shown before
        window.width = frame.width
        window.height = frame.height
        place()
        window.visible = true
        window.requestActivate()
        frame.forceActiveFocus()
        entrance.restart()
    }

    // the rows are laid out a moment after they are made: placed again
    // when the size is known
    function place() {
        let x = Math.round(fromLeft ? bottomCentre.x : bottomCentre.x - frame.width / 2)
        if (screenRect.width > 0)
            x = Math.max(screenRect.x + 8, Math.min(x, screenRect.x + screenRect.width - frame.width - 8))
        window.x = x
        window.y = Math.round(bottomCentre.y - frame.height)
    }

    function close() {
        if (window.visible) {
            window.visible = false
            closed()
        }
    }

    function rowsThatTakeFocus() {
        const out = []
        for (let i = 0; i < column.children.length; ++i) {
            const r = column.children[i]
            if (r.visible && r.enabled && r instanceof T.MenuItem)
                out.push(r)
        }
        return out
    }

    function move(step) {
        const rows = rowsThatTakeFocus()
        if (rows.length === 0)
            return
        let at = rows.indexOf(column.children[currentIndex])
        at = at < 0 ? (step > 0 ? 0 : rows.length - 1) : (at + step + rows.length) % rows.length
        currentIndex = Array.prototype.indexOf.call(column.children, rows[at])
    }

    PlasmaCore.Dialog {
        id: window

        location: PlasmaCore.Types.Floating
        type: PlasmaCore.Dialog.PopupMenu
        backgroundHints: PlasmaCore.Dialog.NoBackground
        hideOnWindowDeactivate: true
        visible: false
        onVisibleChanged: if (!visible) panelMenu.closed()

        mainItem: FocusScope {
            id: frame
            readonly property int padding: Tokens.sizeMenuPadding
            implicitWidth: Math.max(200, Math.ceil(column.widest) + 2 * padding)
            implicitHeight: Math.ceil(column.implicitHeight) + 2 * padding
            width: implicitWidth
            height: implicitHeight
            onWidthChanged: if (window.visible) panelMenu.place()
            onHeightChanged: {
                if (window.visible) {
                    window.height = height
                    panelMenu.place()
                }
            }

            Keys.onPressed: event => {
                switch (event.key) {
                case Qt.Key_Down: panelMenu.move(1); break
                case Qt.Key_Up: panelMenu.move(-1); break
                case Qt.Key_Home: panelMenu.currentIndex = -1; panelMenu.move(1); break
                case Qt.Key_End: panelMenu.currentIndex = -1; panelMenu.move(-1); break
                case Qt.Key_Return:
                case Qt.Key_Enter:
                case Qt.Key_Space: {
                    const r = column.children[panelMenu.currentIndex]
                    if (r && r.enabled)
                        r.clicked()
                    break
                }
                case Qt.Key_Escape: panelMenu.close(); break
                default: return
                }
                event.accepted = true
            }

            AcrylicSurface {
                anchors.fill: parent
                radius: ShellTokens.radiusFlyout
                focused: true
            }

            Column {
                id: column
                x: frame.padding
                y: frame.padding
                width: frame.width - 2 * frame.padding
                // the widest row as it would like to be
                property real widest: 0

                onChildrenChanged: {
                    let w = 0
                    for (let i = 0; i < children.length; ++i)
                        w = Math.max(w, children[i].implicitWidth)
                    widest = w
                    for (let i = 0; i < children.length; ++i) {
                        const r = children[i]
                        if (panelMenu.known.indexOf(r) >= 0)
                            continue
                        panelMenu.known.push(r)
                        r.width = Qt.binding(() => column.width)
                        if (r instanceof T.MenuItem) {
                            r.highlighted = Qt.binding(() => column.children[panelMenu.currentIndex] === r)
                            r.triggered.connect(panelMenu.close)
                        }
                    }
                }
            }

            ParallelAnimation {
                id: entrance
                NumberAnimation { target: frame; property: "opacity"; from: 0; to: 1; duration: ShellTokens.durationFaster }
                NumberAnimation { target: column; property: "y"; from: frame.padding + ShellTokens.menuSlide; to: frame.padding; duration: ShellTokens.durationFast; easing: Motion.entrance }
            }
        }
    }
}
