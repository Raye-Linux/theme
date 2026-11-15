// The Raye window switcher (Alt+Tab): the open windows as cards in rows
// on a plate of Raye acrylic in the middle of the screen. Each card has
// the window's icon and name above its thumbnail and a close button on
// hover; the selected card gets a light outline.
// Rows wrap before the plate gets too wide for the screen. Tab, Shift+Tab
// and the arrows move, a click picks, Delete closes the selected window,
// letting go of Alt switches.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.kwin as KWin
import org.kde.kirigami as Kirigami
import org.raye.ui
import "layout.js" as Layout

KWin.TabBoxSwitcher {
    id: tabBox

    Instantiator {
        active: tabBox.visible
        delegate: PlasmaCore.Dialog {
            location: PlasmaCore.Types.Floating
            visible: true
            flags: Qt.Popup | Qt.X11BypassWindowManagerHint
            // the plate's own size: the dialog adds its frame around it
            x: tabBox.screenGeometry.x + Math.round((tabBox.screenGeometry.width - switcher.width) / 2)
            y: tabBox.screenGeometry.y + Math.round((tabBox.screenGeometry.height - switcher.height) / 2)

            mainItem: Switcher {
                id: switcher
                box: tabBox
            }

            onSceneGraphError: () => {
                // nothing: without this a graphics reset may end KWin
            }
        }
    }

    component Switcher: FocusScope {
        id: switcher

        // the KWin switcher (its model, selection and screen)
        required property var box
        readonly property int count: grid.count
        readonly property var size: Layout.contentSize(count, box.screenGeometry.width, box.screenGeometry.height)

        focus: true
        width: count > 0 ? size.width + 2 * Layout.platePadding : empty.implicitWidth
        height: count > 0 ? size.height + 2 * Layout.platePadding : empty.implicitHeight
        Accessible.role: Accessible.List

        // cuts what scrolls, with room for the outline of the selected card
        Item {
            anchors.fill: parent
            anchors.margins: Layout.platePadding - Layout.outlineRoom
            clip: true

            GridView {
                id: grid
                objectName: "switcherGrid"
                x: Layout.outlineRoom
                y: Layout.outlineRoom
                width: parent.width - 2 * Layout.outlineRoom + Layout.gap
                height: parent.height - 2 * Layout.outlineRoom + Layout.gap
                focus: true
                model: switcher.box.model
                currentIndex: switcher.box.currentIndex
                cellWidth: Layout.cardWidth() + Layout.gap
                cellHeight: Layout.cardHeight() + Layout.gap
                keyNavigationWraps: true
                highlightMoveDuration: 0
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height

                delegate: Card {
                    box: switcher.box
                    current: GridView.isCurrentItem
                }

                onCurrentIndexChanged: switcher.box.currentIndex = currentIndex
            }

            // Tab and Shift+Tab move KWin's selection: follow it
            Connections {
                target: switcher.box
                function onCurrentIndexChanged() {
                    if (grid.currentIndex !== switcher.box.currentIndex)
                        grid.currentIndex = switcher.box.currentIndex
                }
            }
        }

        Column {
            id: empty
            visible: switcher.count === 0
            anchors.centerIn: parent
            padding: 24
            spacing: 8
            Kirigami.Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 32
                height: 32
                source: "window"
                color: Tokens.textSecondary
                isMask: true
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: i18nd("raye_shell_tabbox", "No open windows")
                font: Tokens.body
                color: Tokens.textSecondary
                textFormat: Text.PlainText
            }
        }

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Left)
                grid.moveCurrentIndexLeft()
            else if (event.key === Qt.Key_Right)
                grid.moveCurrentIndexRight()
            else if (event.key === Qt.Key_Up)
                grid.moveCurrentIndexUp()
            else if (event.key === Qt.Key_Down)
                grid.moveCurrentIndexDown()
            else if (event.key === Qt.Key_Delete && grid.currentItem && grid.currentItem.closeable)
                switcher.box.model.close(grid.currentIndex)
            else
                return
            event.accepted = true
        }
    }

    component Card: MouseArea {
        id: card

        required property var box
        required property int index
        required property var model
        property bool current: false
        readonly property bool closeable: model.closeable === true && typeof box.model.close !== "undefined"
        readonly property var thumbSize: Layout.thumbSize(thumbnail.implicitWidth, thumbnail.implicitHeight)

        width: Layout.cardWidth()
        height: Layout.cardHeight()
        hoverEnabled: true
        Accessible.role: Accessible.ListItem
        Accessible.name: model.caption
        onClicked: box.model.activate(index)

        // the light outline of the selected card
        Rectangle {
            anchors.fill: parent
            radius: Tokens.radiusOverlay
            color: card.containsMouse || card.current ? Tokens.fillSubtleHover : "transparent"
            border.width: card.current ? 2 : 0
            border.color: Tokens.focusOuter
        }

        Row {
            id: title
            x: Layout.cardPadding
            y: Layout.cardPadding
            width: parent.width - 2 * Layout.cardPadding - (close.visible ? close.width + 4 : 0)
            height: Layout.titleHeight
            spacing: 8
            Kirigami.Icon {
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                source: card.model.icon
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 24
                text: card.model.caption
                font: Tokens.caption
                color: Tokens.textPrimary
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
        }

        Rectangle {
            id: close
            objectName: "closeButton"
            x: parent.width - width - 6
            y: 6
            width: 24
            height: 24
            radius: Tokens.radiusControl
            visible: card.closeable && (card.containsMouse || closeArea.containsMouse || Kirigami.Settings.hasTransientTouchInput)
            color: closeArea.pressed ? Tokens.captionClosePressed : closeArea.containsMouse ? Tokens.captionClose : "transparent"
            Kirigami.Icon {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "window-close-symbolic"
                color: closeArea.containsMouse ? "#ffffff" : Tokens.textPrimary
                isMask: true
            }
            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: card.box.model.close(card.index)
            }
            Accessible.role: Accessible.Button
            Accessible.name: i18nd("raye_shell_tabbox", "Close window")
        }

        // KWin.WindowThumbnail needs a parent of the size it is to fill
        Item {
            x: Layout.cardPadding + Math.round((Layout.thumbWidth - width) / 2)
            y: Layout.cardPadding + Layout.titleHeight + Layout.titleGap + Math.round((Layout.thumbMaxHeight - height) / 2)
            width: card.thumbSize.width
            height: card.thumbSize.height

            KWin.WindowThumbnail {
                id: thumbnail
                anchors.fill: parent
                wId: card.model.windowId
            }
        }
    }
}
