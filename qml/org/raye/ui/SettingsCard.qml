// A row of a settings page, like Windows' settings cards: a 20 px icon,
// the title (Body) with a description under it (Caption, secondary), and
// on the right whatever the row controls (a switch, a combo box, a button).
// At least 68 px high with a description, 48 without.
//
//   SettingsCard {
//       icon.name: "preferences-desktop-theme"
//       title: qsTr("Dark mode"); description: qsTr("...")
//       Switch { showStateText: true }
//   }
//
// clickable: the whole card is a button (a page to open): it reacts to
// the pointer, Enter and Space, and shows a chevron when it holds nothing.
import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.ui

T.AbstractButton {
    id: card

    property string title
    property string description
    property bool clickable: false
    // what the row controls, on the right
    default property alias trailing: trailingRow.data

    readonly property bool hasIcon: icon.name !== "" || icon.source.toString() !== ""

    implicitWidth: Math.max(320, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(description !== "" ? 68 : 48,
                             implicitContentHeight + topPadding + bottomPadding)
    leftPadding: Tokens.s5
    rightPadding: Tokens.s5
    topPadding: Tokens.s4
    bottomPadding: Tokens.s4
    spacing: Tokens.s5
    hoverEnabled: clickable
    focusPolicy: clickable ? Qt.StrongFocus : Qt.NoFocus
    font: Tokens.body
    icon.width: Tokens.sizeIconList
    icon.height: Tokens.sizeIconList

    Accessible.role: clickable ? Accessible.Button : Accessible.Grouping
    Accessible.name: title
    Accessible.description: description
    Accessible.onPressAction: if (clickable) card.clicked()

    Keys.onReturnPressed: event => { if (card.clickable) card.clicked(); event.accepted = card.clickable }
    Keys.onEnterPressed: event => { if (card.clickable) card.clicked(); event.accepted = card.clickable }

    contentItem: Item {
        implicitWidth: iconItem.implicitWidth + texts.implicitWidth + trailingRow.implicitWidth
                       + chevron.implicitWidth + 3 * card.spacing
        implicitHeight: Math.max(texts.implicitHeight, trailingRow.implicitHeight, iconItem.implicitHeight)

        Kirigami.Icon {
            id: iconItem
            anchors.verticalCenter: parent.verticalCenter
            visible: card.hasIcon
            width: visible ? card.icon.width : 0
            height: card.icon.height
            implicitWidth: width
            implicitHeight: height
            source: card.icon.name !== "" ? card.icon.name : card.icon.source
            color: card.enabled ? Tokens.textPrimary : Tokens.textDisabled
        }

        Column {
            id: texts
            anchors.left: iconItem.right
            anchors.leftMargin: card.hasIcon ? card.spacing : 0
            anchors.right: trailingRow.left
            anchors.rightMargin: trailingRow.width > 0 || chevron.visible ? card.spacing : 0
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0
            Text {
                width: parent.width
                text: card.title
                font: Tokens.body
                lineHeight: Tokens.bodyLineHeight
                lineHeightMode: Text.FixedHeight
                color: card.enabled ? Tokens.textPrimary : Tokens.textDisabled
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                visible: card.description !== ""
                text: card.description
                font: Tokens.caption
                lineHeight: Tokens.captionLineHeight
                lineHeightMode: Text.FixedHeight
                color: card.enabled ? Tokens.textSecondary : Tokens.textDisabled
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
        }

        Row {
            id: trailingRow
            anchors.right: chevron.visible ? chevron.left : parent.right
            anchors.rightMargin: chevron.visible ? card.spacing : 0
            anchors.verticalCenter: parent.verticalCenter
            spacing: Tokens.s3
        }

        // a chevron for a card that opens a page
        Item {
            id: chevron
            visible: card.clickable && trailingRow.children.length === 0
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: visible ? 12 : 0
            width: implicitWidth
            height: 12
            // drawn rather than taken from an icon theme
            Shape {
                anchors.fill: parent
                ShapePath {
                    strokeWidth: 1
                    strokeColor: card.enabled ? Tokens.textSecondary : Tokens.textDisabled
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: 4.5; startY: 2
                    PathLine { x: 8.5; y: 6 }
                    PathLine { x: 4.5; y: 10 }
                }
            }
        }
    }

    background: Rectangle {
        radius: Tokens.radiusControl
        color: !card.clickable || !card.enabled ? Tokens.bgCard
             : card.down ? Tokens.fillControlPressed
             : card.hovered ? Tokens.bgCardHover : Tokens.bgCard
        border.width: 1
        border.color: Tokens.strokeCard

        Behavior on color {
            ColorAnimation { duration: Tokens.durationFaster }
        }

        FocusRing {
            target: card
            radius: Tokens.radiusControl
        }
    }
}
