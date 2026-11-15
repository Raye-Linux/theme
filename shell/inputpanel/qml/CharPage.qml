// Kaomoji or symbols: the groups as chips along the top, then the grid.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Item {
    id: page

    required property QtObject panel
    required property var groups // [{ id, items }]
    required property var titles // id -> translated title
    property int columns: 8
    property int textSize: 20
    property int current: 0

    readonly property var items: groups.length > 0 ? groups[Math.min(current, groups.length - 1)].items : []

    function takeFocus() {
        grid.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Tokens.s2

        // groups
        ListView {
            id: chips
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            Layout.leftMargin: Tokens.s3
            Layout.rightMargin: Tokens.s3
            orientation: ListView.Horizontal
            spacing: Tokens.s2
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: page.groups
            currentIndex: page.current
            highlightFollowsCurrentItem: true
            delegate: PanelButton {
                required property int index
                required property var modelData
                width: implicitContentWidth + 2 * Tokens.s4
                height: 32
                text: page.titles[modelData.id] ?? modelData.id
                selected: page.current === index
                onClicked: {
                    page.current = index
                    grid.positionViewAtBeginning()
                }
                contentItem: Text {
                    text: parent.text
                    font: Tokens.body
                    color: parent.selected ? Tokens.accentText : Tokens.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Tokens.s2
            Layout.rightMargin: Tokens.s2
            Layout.bottomMargin: Tokens.s2
            clip: true
            model: page.items
            cellWidth: Math.floor(width / page.columns)
            cellHeight: page.columns > 4 ? 42 : 46
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds
            Basic.ScrollBar.vertical: Basic.ScrollBar { policy: Basic.ScrollBar.AsNeeded }
            Accessible.role: Accessible.List
            Accessible.name: page.titles[page.groups[page.current]?.id] ?? ""
            Keys.onReturnPressed: if (currentIndex >= 0) page.panel.pickText(page.items[currentIndex])
            Keys.onEnterPressed: if (currentIndex >= 0) page.panel.pickText(page.items[currentIndex])
            Keys.onLeftPressed: event => {
                // Ctrl+Left / Ctrl+Right change the group
                if (event.modifiers & Qt.ControlModifier) { page.current = Math.max(0, page.current - 1); return }
                moveCurrentIndexLeft()
            }
            Keys.onRightPressed: event => {
                if (event.modifiers & Qt.ControlModifier) { page.current = Math.min(page.groups.length - 1, page.current + 1); return }
                moveCurrentIndexRight()
            }

            delegate: MouseArea {
                id: cell
                required property int index
                required property string modelData
                width: grid.cellWidth
                height: grid.cellHeight
                hoverEnabled: true
                onClicked: page.panel.pickText(modelData)
                Accessible.role: Accessible.Button
                Accessible.name: modelData

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: Tokens.radiusControl
                    color: cell.pressed ? Tokens.fillSubtlePressed
                         : (cell.containsMouse || (grid.activeFocus && cell.GridView.isCurrentItem)) ? Tokens.fillSubtleHover
                         : page.columns <= 4 ? Tokens.bgCard : "transparent"
                    border.width: grid.activeFocus && cell.GridView.isCurrentItem ? Tokens.sizeFocusOuter : 0
                    border.color: Tokens.focusOuter
                }
                Text {
                    anchors.fill: parent
                    anchors.margins: Tokens.s1
                    text: cell.modelData
                    font.family: Tokens.body.family
                    font.pixelSize: page.textSize
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: 9
                    color: Tokens.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }
    }
}
