// Stand-in for NetworkManager's handler: it only writes down what it was asked.
import QtQuick

QtObject {
    property bool hotspotSupported: false
    property bool hotspotActive: false
    property list<string> calls
    function enableWireless(on) {}
    function enableAirplaneMode(on) {}
    function createHotspot() {}
    function stopHotspot() {}
    function requestScan() { calls = Array.from(calls).concat(["scan"]) }
    function activateConnection(path, device, specific) { calls = Array.from(calls).concat(["activate:" + path]) }
    function deactivateConnection(path, device) { calls = Array.from(calls).concat(["deactivate:" + path]) }
    function addAndActivateConnection(device, specific, password) {
        calls = Array.from(calls).concat(["add:" + specific + (password ? ":with-password" : "")])
    }
}
