// The other accounts, bottom left of the sign-in view: a vertical list of
// 48 px tiles (40 px round picture, name 14 px semibold). The current one
// has the hover fill and a 3 px accent pill on its left. Six tiles, then
// it scrolls. Hidden when there is only one account.
//
// In:  model      SDDM's userModel, a ListModel, or a JS array of
//                 { name, realName, icon }
//      currentIndex
// Out: userSelected(index)  click, Enter, or the arrow keys
//
// The parent places it (48 px from the left and bottom edges).
import QtQuick
import QtQuick.Templates as T

FocusScope {
    id: root

    property alias model: list.model
    property alias currentIndex: list.currentIndex
    readonly property alias count: list.count
    property int tileWidth: 248
    property int maxTiles: 6

    signal userSelected(int index)

    implicitWidth: tileWidth
    implicitHeight: Math.min(list.count, maxTiles) * 48 + Math.max(0, Math.min(list.count, maxTiles) - 1) * list.spacing
    visible: list.count > 1

    Accessible.role: Accessible.List
    Accessible.name: Strings.tr.otherUsers

    function select(i) {
        if (i < 0 || i >= list.count)
            return
        list.currentIndex = i
        list.positionViewAtIndex(i, ListView.Contain)
        root.userSelected(i)
    }

    ListView {
        id: list
        anchors.fill: parent
        spacing: 4
        clip: true
        focus: true
        interactive: count > root.maxTiles
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: false
        highlightFollowsCurrentItem: false

        Keys.onUpPressed: root.select(Math.max(0, currentIndex - 1))
        Keys.onDownPressed: root.select(Math.min(count - 1, currentIndex + 1))

        delegate: T.AbstractButton {
            id: tile

            required property int index
            required property var model
            readonly property var user: model.modelData !== undefined ? model.modelData : model
            readonly property string displayName: user.realName ? user.realName : (user.name || "")
            readonly property bool current: ListView.isCurrentItem

            width: ListView.view.width
            height: 48
            hoverEnabled: true
            focusPolicy: Qt.NoFocus
            Accessible.role: Accessible.ListItem
            Accessible.name: displayName
            Accessible.selected: current

            onClicked: root.select(index)

            background: Rectangle {
                radius: RayePalette.controlRadius
                color: tile.pressed ? RayePalette.pressFill
                     : (tile.current || tile.hovered) ? RayePalette.hoverFill : "transparent"
                border.width: tile.current && list.activeFocus ? RayePalette.focusWidth : 0
                border.color: RayePalette.focus
                Behavior on color {
                    ColorAnimation { duration: Motion.fast }
                }

                // the selection pill
                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: tile.current ? 18 : 0
                    radius: 1.5
                    color: RayePalette.accent
                    Behavior on height {
                        NumberAnimation { duration: Motion.fast; easing.type: Motion.enter }
                    }
                }
            }

            contentItem: Item {
                UserPicture {
                    id: face
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    size: 40
                    source: tile.user.icon ? tile.user.icon : ""
                }
                ShadowText {
                    anchors.left: face.right
                    anchors.leftMargin: 12
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: tile.displayName
                    elide: Text.ElideRight
                    font.pixelSize: Math.round(14 * RayePalette.textScale)
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
