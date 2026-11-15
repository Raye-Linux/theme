// A row of a RayeMenu: 32 px tall, a 16 px icon 12 px from the edge, the
// label in Body type, the keys on the right in the secondary colour, a
// hover backplate with 4 px corners inset 4 px. A check mark when it is
// checkable, an arrow when it opens a sub-menu.
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

T.MenuItem {
    id: item

    // keys that do the same, shown on the right: "Alt+F4"
    property string shortcutText: ""
    readonly property bool hasIcon: icon.name !== "" || icon.source.toString() !== ""

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Tokens.sizeMenuItem
    leftPadding: 12
    rightPadding: 12
    spacing: 12
    icon.width: Tokens.sizeIconMenu
    icon.height: Tokens.sizeIconMenu

    contentItem: Row {
        spacing: item.spacing
        Kirigami.Icon {
            anchors.verticalCenter: parent.verticalCenter
            width: Tokens.sizeIconMenu
            height: Tokens.sizeIconMenu
            visible: item.hasIcon || item.checkable
            source: item.checkable ? (item.checked ? "checkmark" : "") : (item.icon.name !== "" ? item.icon.name : item.icon.source)
            color: ShellTokens.text
            isMask: item.checkable
            opacity: item.enabled ? 1 : 0.4
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: item.text
            font: ShellTokens.body
            color: item.enabled ? ShellTokens.text : Tokens.textDisabled
            elide: Text.ElideRight
            textFormat: Text.PlainText
            width: Math.min(implicitWidth, 320)
        }
        Item {
            // pushes the keys and the arrow to the right
            width: Math.max(0, item.availableWidth - x - keys.implicitWidth - arrow.width - (arrow.visible ? item.spacing : 0))
            height: 1
        }
        Text {
            id: keys
            anchors.verticalCenter: parent.verticalCenter
            visible: item.shortcutText !== ""
            text: item.shortcutText
            font: ShellTokens.caption
            color: ShellTokens.textSecondary
            textFormat: Text.PlainText
        }
        Kirigami.Icon {
            id: arrow
            anchors.verticalCenter: parent.verticalCenter
            width: 12
            height: 12
            visible: item.subMenu !== null
            source: "arrow-right"
            color: ShellTokens.textSecondary
            isMask: true
        }
    }

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: Tokens.sizeMenuItem
        x: 0
        width: item.width
        height: item.height
        radius: ShellTokens.radiusButton
        color: item.down ? ShellTokens.pressFill
             : (item.highlighted || item.hovered) && item.enabled ? ShellTokens.hoverFill : "transparent"
        Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }
    }
}
