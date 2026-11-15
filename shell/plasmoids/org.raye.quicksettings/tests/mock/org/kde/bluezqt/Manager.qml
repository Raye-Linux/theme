// Stand-in for BluezQt's manager in the Backend test: no Bluetooth at all.
pragma Singleton
import QtQuick

QtObject {
    property bool bluetoothBlocked: false
    property bool bluetoothOperational: false
    property var adapters: []
}
