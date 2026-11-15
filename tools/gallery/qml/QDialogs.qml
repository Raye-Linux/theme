import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

PageBase {
    id: page
    heading: "Dialogs and tooltips"

    function openPopups() {
        dialog.open()
    }

    function closePopups() {
        dialog.close()
    }

    SampleRow {
        caption: "Messages"
        ColumnLayout {
            Layout.preferredWidth: 460
            Layout.maximumWidth: 548
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: true
                text: "Updates are ready."
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: true
                type: Kirigami.MessageType.Positive
                text: "Your PC is up to date."
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: true
                type: Kirigami.MessageType.Warning
                text: "Battery is low."
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: true
                type: Kirigami.MessageType.Error
                text: "The update could not be installed."
            }
        }
    }

    SampleRow {
        caption: "Tooltip"
        Button {
            objectName: "hoverTarget"
            text: "Point here"
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: "Tooltips wait a moment, then stay until the pointer moves away."
        }
    }

    SampleRow {
        caption: "Dialog"
        Button {
            objectName: "focusTarget"
            text: "Open dialog"
            onClicked: page.openPopups()
        }
    }

    Dialog {
        id: dialog
        parent: Overlay.overlay
        // the style centres it, like a Windows content dialog: the
        // messages sit above, out of its way
        width: Math.min(448, parent.width - 48)
        modal: false
        title: "Restart to finish updating?"

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Raye installed 12 updates. Your files are safe."
        }

        footer: DialogButtonBox {
            Button { text: "Restart now"; highlighted: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: "Later"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
    }
}
