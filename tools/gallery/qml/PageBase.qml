// Frame of every Qt Quick page: the title, then the rows in a scroll view.
// A page that opens a menu or a dialog for the capture adds
// openPopups() and closePopups() functions; the program calls them.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    property string heading
    default property alias rows: column.data

    header: Label {
        text: page.heading
        font.pixelSize: 19
        font.weight: Font.DemiBold
        leftPadding: 24
        rightPadding: 24
        topPadding: 24
        bottomPadding: 12
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        // from the rows' own width only: availableWidth depends on the
        // scroll bars, which depend on this
        contentWidth: column.implicitWidth + 48
        contentHeight: column.implicitHeight + 24

        ColumnLayout {
            id: column
            x: 24
            width: Math.max(implicitWidth, scroll.availableWidth - 48)
            spacing: 12
        }
    }
}
