// "Pinned", "Recommended", "All apps": the title in Body strong on the
// left, an optional button on the right.
import QtQuick
import org.raye.ui
import org.raye.shell

Item {
    id: header

    property string title: ""
    property string buttonText: ""
    property bool buttonBack: false
    readonly property Item button: button
    signal buttonClicked()

    implicitHeight: 32

    Text {
        anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
        text: header.title
        font: Tokens.bodyStrong
        color: ShellTokens.text
    }
    TextLinkButton {
        id: button
        objectName: "sectionButton"
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        visible: header.buttonText !== ""
        text: header.buttonText
        back: header.buttonBack
        onClicked: header.buttonClicked()
    }
}
