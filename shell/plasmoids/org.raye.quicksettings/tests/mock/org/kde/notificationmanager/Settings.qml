// Stand-in for the notification settings in the Backend test.
import QtQuick

QtObject {
    property var notificationsInhibitedUntil
    property bool notificationsInhibitedByApplication: false
    function revokeApplicationInhibitions() {}
    function save() {}
}
