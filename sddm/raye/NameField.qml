// The account name field, shown only when SDDM lists no account at all
// (every account hidden): the same look as the password field.
//
// Out: accepted() on Enter (TextField's own signal)
import QtQuick
import QtQuick.Controls.Basic as Basic
import org.raye.signin

Basic.TextField {
    id: field

    readonly property string placeholderWord: Strings.language === "fr" ? "Nom d'utilisateur"
                                                                         : "User name"

    implicitWidth: Math.round(300 * RayePalette.textScale)
    implicitHeight: Math.round(36 * RayePalette.textScale)
    leftPadding: 12
    rightPadding: 12
    placeholderText: placeholderWord
    placeholderTextColor: RayePalette.textSecondary
    color: RayePalette.text
    selectionColor: RayePalette.accent
    selectedTextColor: RayePalette.textOnAccent
    font.family: RayePalette.family
    font.pixelSize: Math.round(14 * RayePalette.textScale)
    verticalAlignment: TextInput.AlignVCenter
    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhPreferLowercase
    maximumLength: 256
    Accessible.name: placeholderWord

    background: Rectangle {
        radius: RayePalette.fieldRadius
        color: RayePalette.fieldFill
        border.width: RayePalette.highContrast && field.activeFocus ? RayePalette.focusWidth : 1
        border.color: RayePalette.highContrast && field.activeFocus ? RayePalette.focus
                                                                    : RayePalette.fieldBorder

        // the accent line when the field has focus
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: RayePalette.fieldRadius / 2
            anchors.rightMargin: RayePalette.fieldRadius / 2
            height: 2
            radius: 1
            color: RayePalette.accent
            opacity: field.activeFocus && !RayePalette.highContrast ? 1 : 0
            Behavior on opacity {
                NumberAnimation { duration: Motion.fast }
            }
        }
    }
}
