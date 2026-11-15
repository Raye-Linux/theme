// Every org.raye.ui piece in its states, Night and Day, to look at next
// to the Windows 11 controls. Not installed.
//
//   qml6 -I <build>/qml qml/gallery.qml            (Raye Night)
//   qml6 -I <build>/qml qml/gallery.qml -- day     (Raye Day)
import QtQuick
import QtQuick.Layouts
import org.raye.ui

MicaWindow {
    id: win

    readonly property var args: Qt.application.arguments

    width: 1280
    height: 800
    visible: true
    title: "Raye UI"

    Component.onCompleted: Tokens.forceMode = args.indexOf("day") >= 0 ? "day" : "night"

    component SectionTitle: Text {
        font: Tokens.bodyStrong
        color: Tokens.textPrimary
        Layout.topMargin: Tokens.s7
        Layout.bottomMargin: Tokens.s2
    }

    // navigation pane, like Settings
    Rectangle {
        id: nav
        width: 280
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: "transparent"

        Column {
            anchors.fill: parent
            anchors.margins: Tokens.s5
            anchors.topMargin: Tokens.s8
            spacing: Tokens.s2

            Text {
                text: "Raye UI"
                font: Tokens.subtitle
                color: Tokens.textPrimary
                bottomPadding: Tokens.s5
            }
            Repeater {
                model: ["Buttons", "Toggles", "Cards", "Messages", "Surfaces"]
                delegate: Item {
                    required property string modelData
                    required property int index
                    width: parent.width
                    height: Tokens.sizeNavItem
                    Rectangle {
                        anchors.fill: parent
                        radius: Tokens.radiusControl
                        color: index === 0 ? Tokens.fillSubtleSelected
                             : index === 2 ? Tokens.fillSubtleHover : "transparent"
                    }
                    Pill {
                        active: index === 0
                        x: 0
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        x: Tokens.s5
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                        font: Tokens.body
                        color: Tokens.textPrimary
                    }
                }
            }
        }
    }

    // content on the layer, 8 px top-left corner
    Rectangle {
        id: layer
        anchors.left: nav.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: Tokens.s10
        color: Tokens.bgLayer
        radius: Tokens.radiusOverlay
        border.width: 1
        border.color: Tokens.strokeCard

        Flickable {
            anchors.fill: parent
            anchors.leftMargin: Tokens.s9
            anchors.rightMargin: Tokens.s9
            contentHeight: page.implicitHeight + Tokens.s8
            clip: true

            ColumnLayout {
                id: page
                width: parent.width
                spacing: Tokens.s2

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Tokens.s7
                    Text {
                        text: "Controls"
                        font: Tokens.title
                        color: Tokens.textPrimary
                        Layout.fillWidth: true
                    }
                    Text { text: "Day colours"; font: Tokens.body; color: Tokens.textSecondary }
                    Switch {
                        text: "Day colours"
                        checked: Tokens.day
                        onToggled: Tokens.forceMode = checked ? "day" : "night"
                    }
                }

                SectionTitle { text: "Buttons" }
                Flow {
                    Layout.fillWidth: true
                    spacing: Tokens.s3
                    AccentButton { text: "Restart now" }
                    AccentButton { text: "Cancel"; accent: false }
                    AccentButton { text: "Install"; icon.name: "download" }
                    AccentButton { text: "Disabled"; enabled: false }
                    AccentButton { text: "Disabled"; accent: false; enabled: false }
                    AccentButton {
                        text: "Focus ring"
                        accent: false
                        FocusRing { shown: true }
                    }
                }

                SectionTitle { text: "Toggle switches" }
                Flow {
                    Layout.fillWidth: true
                    spacing: Tokens.s7
                    Switch { text: "On"; checked: true }
                    Switch { text: "Off" }
                    Switch { text: "On, disabled"; checked: true; enabled: false }
                    Switch { text: "Off, disabled"; enabled: false }
                    Switch {
                        text: "Focus ring"
                        checked: true
                        FocusRing { shown: true; radius: 10; anchors.fill: undefined; x: -3; y: 3; width: 46; height: 26 }
                    }
                }

                SectionTitle { text: "Settings cards" }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.s2
                    SettingsCard {
                        Layout.fillWidth: true
                        icon.name: "preferences-desktop-theme"
                        title: "Dark mode"
                        description: "Raye Night for apps and the desktop"
                        Switch { checked: true }
                    }
                    SettingsCard {
                        Layout.fillWidth: true
                        icon.name: "preferences-desktop-wallpaper"
                        title: "Background"
                        description: "Pictures, solid colour or a slideshow"
                        AccentButton { text: "Browse photos"; accent: false }
                    }
                    SettingsCard {
                        Layout.fillWidth: true
                        icon.name: "preferences-desktop-display"
                        title: "Display"
                        description: "Monitors, brightness, night light, display profile"
                        clickable: true
                    }
                    SettingsCard {
                        Layout.fillWidth: true
                        title: "A card with a title only"
                        clickable: true
                    }
                }

                SectionTitle { text: "Messages" }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.s3
                    InfoBar {
                        Layout.fillWidth: true
                        title: "Information"
                        message: "Raye checks for updates once a day."
                    }
                    InfoBar {
                        Layout.fillWidth: true
                        severity: InfoBar.Success
                        title: "You're up to date"
                        message: "Last checked today at 14:32."
                        closable: false
                    }
                    InfoBar {
                        Layout.fillWidth: true
                        severity: InfoBar.Warning
                        title: "Restart needed"
                        message: "Restart your computer to finish installing updates."
                        AccentButton { text: "Restart now" }
                    }
                    InfoBar {
                        Layout.fillWidth: true
                        severity: InfoBar.Error
                        title: "Something went wrong"
                        message: "The update could not be downloaded. Check your connection and try again."
                    }
                }

                SectionTitle { text: "Surfaces" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Tokens.s5
                    Card {
                        Layout.preferredWidth: 300
                        Layout.preferredHeight: 200
                        Column {
                            spacing: Tokens.s2
                            Text { text: "Card"; font: Tokens.bodyStrong; color: Tokens.textPrimary }
                            Text {
                                width: 260
                                text: "Cards group settings on a page. 8 px corners, 1 px stroke."
                                font: Tokens.body
                                color: Tokens.textSecondary
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                    Item {
                        id: frostDemo
                        Layout.preferredWidth: 460
                        Layout.preferredHeight: 200
                        // something colourful to blur
                        Item {
                            id: backdrop
                            anchors.fill: parent
                            Rectangle {
                                anchors.fill: parent
                                radius: Tokens.radiusOverlay
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0; color: Tokens.brandViolet }
                                    GradientStop { position: 1; color: Tokens.brandMagenta }
                                }
                            }
                            Repeater {
                                model: 6
                                Rectangle {
                                    required property int index
                                    x: 20 + index * 72; y: 30 + (index % 2) * 70
                                    width: 56; height: 56; radius: 28
                                    color: index % 2 ? "#5fe0a1" : "#ffc857"
                                }
                            }
                        }
                        Acrylic {
                            backdrop: backdrop
                            x: 150; y: 24
                            width: 260; height: 152
                            Column {
                                width: 252
                                Repeater {
                                    model: ["Open", "Open with", "Rename", "Delete"]
                                    delegate: Rectangle {
                                        required property string modelData
                                        required property int index
                                        width: parent.width
                                        height: Tokens.sizeMenuItem
                                        radius: Tokens.radiusControl
                                        color: index === 1 ? Tokens.fillSubtleHover : "transparent"
                                        Text {
                                            x: Tokens.s4
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: modelData
                                            font: Tokens.body
                                            color: Tokens.textPrimary
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
