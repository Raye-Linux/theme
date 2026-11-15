// A message strip inside a page, like Windows' InfoBar: 48 px high at
// least, 4 px corners, a 16 px severity sign in a coloured circle, the
// title in Body Strong and the message after it (under it when it does
// not fit), then optional buttons and a close button.
//
//   InfoBar {
//       severity: InfoBar.Warning
//       title: qsTr("Restart needed"); message: qsTr("...")
//       AccentButton { text: qsTr("Restart now") }
//   }
import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Templates as T
import org.raye.ui

T.Control {
    id: bar

    enum Severity { Information, Success, Warning, Error }

    property int severity: InfoBar.Information
    property string title
    property string message
    property bool closable: true
    // buttons after the message
    default property alias actions: actionRow.data

    signal closeClicked()

    readonly property color signColor: severity === InfoBar.Success ? Tokens.statusSuccess
                                     : severity === InfoBar.Warning ? Tokens.statusCaution
                                     : severity === InfoBar.Error ? Tokens.statusCritical
                                     : Tokens.accent
    // a light wash of the severity over the card colour; information
    // stays on the plain card, as in Windows
    readonly property color fillColor: severity === InfoBar.Information ? Tokens.bgCard
                                     : Qt.alpha(signColor, Tokens.night ? 0.14 : 0.12)

    implicitWidth: Math.max(320, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(48, implicitContentHeight + topPadding + bottomPadding)
    leftPadding: Tokens.s5
    rightPadding: closable ? Tokens.s2 : Tokens.s5
    topPadding: Tokens.s3
    bottomPadding: Tokens.s3
    font: Tokens.body

    Accessible.role: Accessible.AlertMessage
    Accessible.name: title
    Accessible.description: message

    contentItem: RowLayout {
        spacing: Tokens.s4

        Rectangle {
            id: sign
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: (Tokens.bodyLineHeight - height) / 2 + 6
            implicitWidth: 16
            implicitHeight: 16
            radius: 8
            color: bar.signColor
            Accessible.ignored: true

            Shape {
                anchors.fill: parent
                ShapePath {
                    strokeWidth: 1.5
                    strokeColor: Tokens.night ? Tokens.bgBase : Tokens.textOnAccent
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    // a tick, a cross, or the stem of "i" / "!"
                    PathSvg {
                        path: bar.severity === InfoBar.Success ? "M 4.5 8.2 L 7 10.5 L 11.5 5.5"
                            : bar.severity === InfoBar.Error ? "M 5.5 5.5 L 10.5 10.5 M 10.5 5.5 L 5.5 10.5"
                            : bar.severity === InfoBar.Warning ? "M 8 4.5 L 8 8.8"
                            : "M 8 7.2 L 8 11.5"
                    }
                }
            }
            // the dot of "i" or "!"
            Rectangle {
                visible: bar.severity === InfoBar.Information || bar.severity === InfoBar.Warning
                width: 2; height: 2; radius: 1
                x: 7
                y: bar.severity === InfoBar.Warning ? 10.5 : 4.2
                color: Tokens.night ? Tokens.bgBase : Tokens.textOnAccent
            }
        }

        // title and message: on one line when they fit, the message under
        // the title otherwise
        Flow {
            id: texts
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            spacing: Tokens.s4
            Text {
                text: bar.title
                visible: text !== ""
                font: Tokens.bodyStrong
                color: Tokens.textPrimary
                lineHeight: Tokens.bodyLineHeight
                lineHeightMode: Text.FixedHeight
            }
            Text {
                text: bar.message
                visible: text !== ""
                width: Math.min(implicitWidth, texts.width)
                font: Tokens.body
                color: Tokens.textPrimary
                lineHeight: Tokens.bodyLineHeight
                lineHeightMode: Text.FixedHeight
                wrapMode: Text.Wrap
            }
        }

        Row {
            id: actionRow
            Layout.alignment: Qt.AlignVCenter
            spacing: Tokens.s3
            visible: children.length > 0
        }

        T.Button {
            id: closeButton
            Layout.alignment: Qt.AlignTop
            visible: bar.closable
            implicitWidth: Tokens.sizeControlHeight
            implicitHeight: Tokens.sizeControlHeight
            padding: 0
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            // "Close" read to screen readers, in the system's language
            Accessible.name: Qt.locale().name.startsWith("fr") ? "Fermer" : "Close"
            Accessible.role: Accessible.Button
            onClicked: bar.closeClicked()
            Keys.onReturnPressed: clicked()

            background: Rectangle {
                radius: Tokens.radiusControl
                color: closeButton.down ? Tokens.fillSubtlePressed
                     : closeButton.hovered ? Tokens.fillSubtleHover : "transparent"
                FocusRing { target: closeButton }
            }
            contentItem: Shape {
                ShapePath {
                    strokeWidth: 1
                    strokeColor: Tokens.textPrimary
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    PathSvg { path: "M 11 11 L 21 21 M 21 11 L 11 21" }
                }
            }
        }
    }

    background: Rectangle {
        radius: Tokens.radiusControl
        color: bar.fillColor
        border.width: 1
        border.color: Tokens.strokeCard
    }
}
