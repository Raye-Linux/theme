// The fakes put themselves here, so a test can reach them.
pragma Singleton
import QtQuick
QtObject {
    property var tasks: null
    property var power: null
}
