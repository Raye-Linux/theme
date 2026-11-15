// Stand-in for KWin's workspace in the snap tests: screens, windows and
// the work area (the screen less a 48 px taskbar at the bottom).
pragma Singleton
import QtQuick

QtObject {
    property var screens: []
    property var windows: []
    property var stackingOrder: windows
    property var activeWindow: null
    property int currentDesktop: 1
    property point cursorPos: Qt.point(0, 0)
    signal windowAdded(var window)
    signal windowRemoved(var window)

    function clientArea(option, output, desktop) {
        const g = output.geometry;
        return Qt.rect(g.x, g.y, g.width, g.height - 48);
    }
    function screenAt(p) {
        for (const s of screens) {
            const g = s.geometry;
            if (p.x >= g.x && p.x < g.x + g.width && p.y >= g.y && p.y < g.y + g.height)
                return s;
        }
        return null;
    }
    function windowAt(p, count) {
        for (const w of windows.slice().reverse()) {
            const f = w.frameGeometry;
            if (p.x >= f.x && p.x < f.x + f.width && p.y >= f.y && p.y < f.y + f.height)
                return [w];
        }
        return [];
    }
}
