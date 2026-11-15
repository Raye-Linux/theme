// One row of a page: a caption, then the samples side by side.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    property alias caption: label.text
    default property alias samples: inner.data

    spacing: 16

    Label {
        id: label
        Layout.preferredWidth: 168
        Layout.preferredHeight: 32
        Layout.alignment: Qt.AlignTop
        verticalAlignment: Text.AlignVCenter
        color: palette.placeholderText
    }

    RowLayout {
        id: inner
        spacing: 8
        Layout.fillWidth: true
    }
}
