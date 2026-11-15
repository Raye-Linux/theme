// Stand-in for KWin's live window thumbnail: a drawn window of the size
// the test gives it ("1600x900" in the id, or a 16:9 window).
import QtQuick

Rectangle {
    id: thumb
    property var wId
    readonly property var dims: {
        const m = String(wId).match(/(\d+)x(\d+)/)
        return m ? [Number(m[1]), Number(m[2])] : [1600, 900]
    }
    implicitWidth: dims[0]
    implicitHeight: dims[1]
    radius: 4
    clip: true
    gradient: Gradient {
        GradientStop { position: 0; color: "#2b2340" }
        GradientStop { position: 1; color: "#181226" }
    }
    Rectangle { width: parent.width; height: Math.max(6, parent.height * 0.08); color: "#3a3052" }
    Column {
        x: parent.width * 0.08; y: parent.height * 0.2
        spacing: Math.max(3, thumb.height * 0.05)
        Repeater {
            model: 5
            Rectangle {
                width: thumb.width * (0.3 + 0.5 * ((index * 37) % 10) / 10)
                height: Math.max(2, thumb.height * 0.04)
                radius: height / 2
                color: index === 0 ? "#7b3cff" : "#4a4060"
            }
        }
    }
}
