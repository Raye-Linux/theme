import QtQuick

QtObject {
    property var profiles: []
    property string activeProfile: ""
    function setProfile(name) { activeProfile = name }
}
