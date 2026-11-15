import QtQuick
import org.kde.plasma.plasmoid
QtObject {
    property string engine
    property var connectedSources: []
    property var data: ({})
    Component.onCompleted: Registry.power = this
}
