// A flyout that opens upward from a corner bar button: radius 8, night
// fill at 92 %, 1 px light border, 40 px rows (16 px icon, 14 px text).
// Esc or a click outside closes it; Up and Down move, Enter or Space
// picks; Tab and Shift+Tab move too.
//
// In:  anchorItem  the button it opens from (right edges line up)
//      rows        [{ id, text, icon, checkable, checked, selected, enabled }]
//                  (selected: the one picked of a list, read out as checked)
//      message     a plain line above the rows (e.g. "Sign in to connect
//                  to a network")
// Out: triggered(id)  a row was picked; a plain row also closes the
//                     flyout, a checkable one stays open (its owner flips
//                     `checked` in the rows it gives)
import QtQuick
import QtQuick.Effects
import QtQuick.Templates as T

T.Popup {
    id: flyout

    property Item anchorItem: null
    property var rows: []
    property string message: ""
    // give the keys back to the button on close; off when the owner sends
    // them elsewhere after a pick
    property bool focusBack: true
    property int currentRow: -1
    readonly property int rowCount: rows ? rows.length : 0

    signal triggered(string id)

    // the widest line, measured once per change of rows or words
    readonly property real naturalWidth: {
        var w = message !== "" ? measure(message) + 24 : 0
        for (var i = 0; i < rowCount; ++i)
            w = Math.max(w, 40 + measure(rows[i].text) + (rows[i].checkable ? 12 + 36 : 0) + 12)
        return Math.ceil(w)
    }

    function measure(text) {
        return metrics.advanceWidth(text)
    }

    FontMetrics {
        id: metrics
        font.family: RayePalette.family
        font.pixelSize: Math.round(14 * RayePalette.textScale)
    }

    parent: anchorItem
    implicitWidth: Math.max(240, naturalWidth + leftPadding + rightPadding)
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    x: anchorItem ? anchorItem.width - width : 0
    y: -height - 8
    padding: 4
    margins: 8
    modal: false
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent

    function firstEnabled(from, step) {
        for (var n = 0; n < rowCount; ++n) {
            var i = ((from + step * n) % rowCount + rowCount) % rowCount
            if (rows[i].enabled !== false)
                return i
        }
        return -1
    }

    function move(step) {
        if (rowCount === 0)
            return
        var start = currentRow < 0 ? (step > 0 ? 0 : rowCount - 1) : currentRow + step
        currentRow = firstEnabled(start, step)
    }

    function pick(i) {
        if (i < 0 || i >= rowCount || rows[i].enabled === false)
            return
        var row = rows[i]
        currentRow = i
        flyout.triggered(row.id)
        if (!row.checkable)
            flyout.close()
    }

    onOpened: {
        currentRow = firstEnabled(0, 1)
        content.forceActiveFocus()
    }
    onClosed: {
        if (anchorItem && focusBack)
            anchorItem.forceActiveFocus()
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Motion.enter }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Motion.normal; easing.type: Motion.enter }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.fast; easing.type: Motion.leave }
    }

    background: Item {
        // the shadow (GPU only): drawn from a hidden shape of the panel
        Rectangle {
            id: shape
            anchors.fill: parent
            radius: RayePalette.flyoutRadius
            color: "#000000"
            visible: false
        }
        MultiEffect {
            anchors.fill: shape
            source: shape
            visible: GraphicsInfo.api !== GraphicsInfo.Software && !RayePalette.highContrast
            shadowEnabled: true
            shadowBlur: 1.0
            shadowVerticalOffset: 8
            shadowColor: "#80000000"
        }
        Rectangle {
            anchors.fill: parent
            radius: RayePalette.flyoutRadius
            color: RayePalette.flyoutFill
            border.width: 1
            border.color: RayePalette.flyoutBorder
        }
    }

    contentItem: FocusScope {
        id: content
        implicitWidth: flyout.naturalWidth
        implicitHeight: rowsColumn.implicitHeight
        focus: true

        Keys.onUpPressed: flyout.move(-1)
        Keys.onDownPressed: flyout.move(1)
        Keys.onTabPressed: flyout.move(1)
        Keys.onBacktabPressed: flyout.move(-1)
        Keys.onReturnPressed: flyout.pick(flyout.currentRow)
        Keys.onEnterPressed: flyout.pick(flyout.currentRow)
        Keys.onSpacePressed: flyout.pick(flyout.currentRow)

        Column {
            id: rowsColumn
            width: parent.width

            Text {
                visible: flyout.message !== ""
                width: rowsColumn.width
                leftPadding: 12
                rightPadding: 12
                topPadding: 10
                bottomPadding: 10
                text: flyout.message
                color: RayePalette.textSecondary
                font.family: RayePalette.family
                font.pixelSize: Math.round(14 * RayePalette.textScale)
                wrapMode: Text.NoWrap
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            Repeater {
                model: flyout.rows
                delegate: T.AbstractButton {
                    id: row

                    required property var modelData
                    required property int index
                    readonly property bool current: flyout.currentRow === index

                    width: rowsColumn.width
                    height: Math.round(40 * RayePalette.textScale)
                    enabled: modelData.enabled !== false
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    checkable: false
                    Accessible.role: modelData.checkable ? Accessible.CheckBox : Accessible.MenuItem
                    Accessible.name: modelData.text
                    Accessible.checkable: modelData.checkable === true || modelData.selected !== undefined
                    Accessible.checked: modelData.checked === true || modelData.selected === true

                    onClicked: flyout.pick(index)
                    onHoveredChanged: if (hovered) flyout.currentRow = index

                    background: Rectangle {
                        radius: RayePalette.controlRadius
                        color: row.pressed ? RayePalette.pressFill
                             : row.current ? RayePalette.hoverFill : "transparent"
                        border.width: row.current && content.activeFocus && RayePalette.highContrast
                                      ? RayePalette.focusWidth : 0
                        border.color: RayePalette.focus
                    }

                    contentItem: Item {
                        Image {
                            id: icon
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            visible: row.modelData.icon !== undefined && row.modelData.icon !== ""
                            source: visible ? RayePalette.icon(row.modelData.icon) : ""
                            sourceSize: Qt.size(16, 16)
                            width: 16
                            height: 16
                            opacity: row.enabled ? 1 : 0.4
                        }
                        Text {
                            id: label
                            x: 40
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.modelData.text
                            color: RayePalette.text
                            opacity: row.enabled ? 1 : 0.4
                            font.family: RayePalette.family
                            font.pixelSize: Math.round(14 * RayePalette.textScale)
                        }
                        // a small switch for the toggles
                        Rectangle {
                            visible: row.modelData.checkable === true
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36
                            height: 18
                            radius: 9
                            color: row.modelData.checked ? RayePalette.accent : "transparent"
                            border.width: row.modelData.checked ? 0 : 1
                            border.color: RayePalette.textSecondary
                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                anchors.verticalCenter: parent.verticalCenter
                                x: row.modelData.checked ? parent.width - width - 4 : 4
                                color: row.modelData.checked ? RayePalette.textOnAccent : RayePalette.textSecondary
                                Behavior on x {
                                    NumberAnimation { duration: Motion.fast; easing.type: Motion.enter }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
