import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

PageBase {
    heading: "Lists, tabs, scroll bars"

    SampleRow {
        caption: "List"
        Frame {
            padding: 1
            Layout.preferredWidth: 260
            Layout.preferredHeight: 200
            ListView {
                id: list
                anchors.fill: parent
                clip: true
                currentIndex: 0
                model: [
                    { icon: "folder-documents", name: "Documents" },
                    { icon: "folder-download", name: "Downloads" },
                    { icon: "folder-music", name: "Music" },
                    { icon: "folder-pictures", name: "Pictures" },
                    { icon: "folder-videos", name: "Videos" },
                    { icon: "user-trash", name: "Recycle Bin" },
                    { icon: "folder", name: "Projects" },
                    { icon: "folder", name: "Games" }
                ]
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    objectName: index === 2 ? "hoverTarget" : (index === 1 ? "focusTarget" : "")
                    width: ListView.view.width
                    text: modelData.name
                    icon.name: modelData.icon
                    highlighted: ListView.isCurrentItem
                    enabled: index !== 5
                }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
            }
        }
    }

    SampleRow {
        caption: "Tab bar"
        TabBar {
            currentIndex: 1
            TabButton { text: "General"; width: implicitWidth }
            TabButton { text: "Sharing"; width: implicitWidth }
            TabButton { text: "Security"; width: implicitWidth; enabled: false }
        }
    }

    SampleRow {
        caption: "Frame, group box"
        Frame { Label { text: "Content in a frame" } }
        GroupBox { title: "Group box"; Label { text: "Content in a group box" } }
    }

    SampleRow {
        caption: "Scroll bar"
        ScrollBar {
            orientation: Qt.Horizontal
            size: 0.3
            position: 0.2
            policy: ScrollBar.AlwaysOn
            Layout.preferredWidth: 240
        }
        ScrollBar {
            orientation: Qt.Vertical
            size: 0.2
            position: 0.5
            policy: ScrollBar.AlwaysOn
            Layout.preferredHeight: 120
        }
    }
}
