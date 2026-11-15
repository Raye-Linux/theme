// Stand-in for Plasma's TasksModel: a list of windows the test sets.
import QtQuick
import org.kde.plasma.plasmoid
QtObject {
    enum GroupMode { GroupDisabled }
    property int groupMode
    property bool filterByScreen
    property rect screenGeometry
    property bool filterByVirtualDesktop
    property var virtualDesktop
    property bool filterByActivity
    property string activity
    property var windows: []
    readonly property int count: windows.length
    signal dataChanged()
    signal rowsInserted()
    signal rowsRemoved()
    function index(row, column) { return row }
    readonly property var keys: ["maximized", "fullScreen", "minimized"]
    function data(row, role) { return windows[row][keys[role]] === true }
    Component.onCompleted: Registry.tasks = this
}
