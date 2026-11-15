// Stand-in until the desktops flyout lands (SH-19 replaces this file).
// The Start applet loads it once and sets:
//   anchorItem    the Task view button
//   hovering      the pointer has been on that button for 400 ms
import QtQuick

Item {
    property Item anchorItem
    property bool hovering: false
}
