// Stand-in for the screen brightness control in the Backend test. The list
// of screens comes from the test (screenList, a C++ model: a ListModel
// crashes when asked for a role it does not have, a real model does not).
// What was asked for lands in calls.
import QtQuick

QtObject {
    property bool isBrightnessAvailable: true
    property var displays: screenList
    property var calls: []
    function setBrightness(name, value) {
        calls = calls.concat([name + "=" + value])
    }
}
