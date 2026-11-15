// The live picture of one window, through the compositor's screencast of
// that window (the same stream Plasma's own task manager shows). In a
// file of its own: without PipeWire it fails to load and the card keeps
// the app's icon.
import QtQuick
import org.kde.pipewire as PipeWire
import org.kde.taskmanager as TaskManager

Item {
    id: picture

    property string winId: ""
    readonly property bool shown: source.nodeId > 0 && source.ready

    PipeWire.PipeWireSourceItem {
        id: source
        readonly property bool ready: state === PipeWire.PipeWireSourceItem.StreamState.Streaming
        anchors.fill: parent
        visible: nodeId > 0
        nodeId: request.nodeId
        enabled: visible
    }

    TaskManager.ScreencastingRequest {
        id: request
        uuid: picture.winId
    }
}
