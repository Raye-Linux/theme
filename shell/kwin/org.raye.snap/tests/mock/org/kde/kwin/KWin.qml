// Stand-in for KWin's script globals in the snap tests.
pragma Singleton
import QtQuick

QtObject {
    enum ClientAreaOption { PlacementArea = 0, MovementArea = 1, MaximizeArea = 2 }
    property var config: ({})
    function readConfig(key, fallback) {
        return config[key] !== undefined ? config[key] : fallback;
    }
}
