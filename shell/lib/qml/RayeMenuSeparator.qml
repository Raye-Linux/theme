// A line between groups of a RayeMenu: 1 px with 4 px above and below.
import QtQuick
import QtQuick.Templates as T
import org.raye.shell

T.MenuSeparator {
    topPadding: 4
    bottomPadding: 4
    implicitHeight: 9
    contentItem: Rectangle {
        implicitWidth: 188
        implicitHeight: 1
        color: ShellTokens.separator
    }
}
