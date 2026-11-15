// The corner bar, bottom right of the sign-in view: 40 x 40 buttons with
// 20 px icons, 4 px apart, right to left Power, Accessibility, Network,
// then the keyboard layout code when there is more than one layout. Each
// opens its flyout upward.
//
// In:  networkIcon     a network icon name ("network-wired",
//                      "network-wireless-3", "network-none"); empty hides
//                      the button
//      networkText     the network flyout's line (default "Sign in to
//                      connect to a network")
//      layoutCode      "FR", "US"; shown when showLayout
//      canSleep canHibernate canShutDown canRestart updatePending
//      keyboardOn keyboardAvailable   (accessibility flyout)
//      showPower showAccessibility
//      emergencyRestart  Ctrl+click on Power asks for an emergency restart
//                        instead of opening the flyout (security screen)
// Out: sleepRequested() hibernateRequested() shutDownRequested()
//      restartRequested() updateAndShutDownRequested()
//      updateAndRestartRequested() keyboardToggled(on) layoutClicked()
//      emergencyRestartRequested()
// The parent places it (24 px from the right and bottom edges).
import QtQuick

FocusScope {
    id: bar

    property string networkIcon: ""
    property string networkText: Strings.tr.networkSignIn
    property string layoutCode: ""
    property bool showLayout: false
    property bool showPower: true
    property bool showAccessibility: true
    property bool canSleep: true
    property bool canHibernate: false
    property bool canShutDown: true
    property bool canRestart: true
    property bool updatePending: false
    property bool keyboardOn: false
    property bool keyboardAvailable: true
    property bool emergencyRestart: false

    readonly property alias powerFlyout: powerFlyout
    readonly property alias accessFlyout: accessFlyout
    readonly property alias networkFlyout: networkFlyout
    readonly property bool flyoutOpen: powerFlyout.visible || accessFlyout.visible
                                       || networkFlyout.visible

    signal sleepRequested()
    signal hibernateRequested()
    signal shutDownRequested()
    signal restartRequested()
    signal updateAndShutDownRequested()
    signal updateAndRestartRequested()
    signal keyboardToggled(bool on)
    signal layoutClicked()
    signal emergencyRestartRequested()

    function closeFlyouts() {
        powerFlyout.close()
        accessFlyout.close()
        networkFlyout.close()
    }

    function toggle(f) {
        var open = f.visible
        closeFlyouts()
        if (!open)
            f.open()
    }

    implicitWidth: row.implicitWidth
    implicitHeight: 40

    Row {
        id: row
        anchors.right: parent.right
        spacing: 4

        CornerButton {
            id: layoutButton
            objectName: "layoutButton"
            visible: bar.showLayout && bar.layoutCode !== ""
            text: bar.layoutCode
            accessibleName: Strings.tr.keyboardLayout + " " + bar.layoutCode
            onClicked: bar.layoutClicked()
        }

        CornerButton {
            id: networkButton
            objectName: "networkButton"
            visible: bar.networkIcon !== ""
            iconName: bar.networkIcon
            accessibleName: Strings.tr.network
            checked: networkFlyout.visible
            onClicked: bar.toggle(networkFlyout)

            Flyout {
                id: networkFlyout
                anchorItem: networkButton
                message: bar.networkText
            }
        }

        CornerButton {
            id: accessButton
            objectName: "accessButton"
            visible: bar.showAccessibility
            iconName: "accessibility"
            accessibleName: Strings.tr.accessibility
            checked: accessFlyout.visible
            onClicked: bar.toggle(accessFlyout)

            AccessFlyout {
                id: accessFlyout
                anchorItem: accessButton
                keyboardOn: bar.keyboardOn
                keyboardAvailable: bar.keyboardAvailable
                onKeyboardToggled: on => bar.keyboardToggled(on)
            }
        }

        CornerButton {
            id: powerButton
            objectName: "powerButton"
            visible: bar.showPower
            iconName: "power"
            accessibleName: Strings.tr.power
            checked: powerFlyout.visible

            // Ctrl+click: the hidden emergency restart. A plain click
            // falls through to the button.
            MouseArea {
                anchors.fill: parent
                enabled: bar.emergencyRestart
                acceptedButtons: Qt.LeftButton
                onPressed: mouse => mouse.accepted = (mouse.modifiers & Qt.ControlModifier) !== 0
                onClicked: {
                    bar.closeFlyouts()
                    bar.emergencyRestartRequested()
                }
            }

            onClicked: bar.toggle(powerFlyout)

            PowerFlyout {
                id: powerFlyout
                anchorItem: powerButton
                canSleep: bar.canSleep
                canHibernate: bar.canHibernate
                canShutDown: bar.canShutDown
                canRestart: bar.canRestart
                updatePending: bar.updatePending
                onSleepRequested: bar.sleepRequested()
                onHibernateRequested: bar.hibernateRequested()
                onShutDownRequested: bar.shutDownRequested()
                onRestartRequested: bar.restartRequested()
                onUpdateAndShutDownRequested: bar.updateAndShutDownRequested()
                onUpdateAndRestartRequested: bar.updateAndRestartRequested()
            }
        }
    }
}
