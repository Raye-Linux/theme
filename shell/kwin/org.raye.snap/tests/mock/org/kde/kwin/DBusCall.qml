import QtQuick

QtObject {
    property string service
    property string path
    property string dbusInterface
    property string method
    property int calls: 0
    function call() { calls++ }
}
