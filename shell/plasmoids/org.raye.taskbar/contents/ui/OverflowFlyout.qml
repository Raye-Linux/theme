// The flyout of the "..." button: the apps that do not fit on the
// taskbar, five to a row, on the Raye acrylic above the button. A click
// does what it does on the taskbar, then the flyout closes.
import QtQuick
import QtQuick.Templates as T
import org.raye.ui
import org.raye.shell

import "taskbarlogic.js" as Logic

T.Popup {
    id: flyout

    property Item bar
    property Item anchorItem

    readonly property int columns: 5
    readonly property int hidden: bar ? bar.buttonCount - bar.visibleCount : 0

    popupType: T.Popup.Window
    padding: 8
    parent: anchorItem
    // centred over the button, the flyout gap above the taskbar
    x: anchorItem ? Math.round((anchorItem.width - width) / 2) : 0
    y: anchorItem ? -height - (ShellTokens.flyoutGap + (ShellTokens.taskbarHeight - anchorItem.height) / 2) : 0
    implicitWidth: grid.implicitWidth + leftPadding + rightPadding
    implicitHeight: grid.implicitHeight + topPadding + bottomPadding
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
    focus: true
    onHiddenChanged: if (hidden <= 0) close()

    background: AcrylicSurface {
        focused: flyout.activeFocus
    }

    contentItem: Grid {
        id: grid
        columns: Math.max(1, Math.min(flyout.columns, flyout.hidden))
        spacing: 4

        Repeater {
            model: flyout.bar ? flyout.bar.model : null

            delegate: TaskButton {
                required property int index
                required property var model

                // only the buttons the taskbar has no room for
                visible: flyout.bar && index >= flyout.bar.visibleCount
                width: visible ? ShellTokens.buttonSize : 0
                height: visible ? ShellTokens.buttonSize : 0

                title: model.display || model.AppName || ""
                iconSource: model.decoration || ""
                isLauncher: !!model.IsLauncher
                isStartup: !!model.IsStartup
                isActive: !!model.IsActive
                isMinimized: !!model.IsMinimized
                demandsAttention: !!model.IsDemandingAttention
                childCount: model.ChildCount || 0
                flashing: flyout.bar ? flyout.bar.settings.flashing : true
                count: flyout.bar && flyout.bar.settings.badges
                       ? (flyout.bar.badgeCounts[Logic.appKey(model.LauncherUrlWithoutIcon, model.AppId)] || 0) : 0

                onClicked: (mouseButton, shift, ctrl) => {
                    flyout.bar.click(index, mouseButton, shift, ctrl)
                    flyout.close()
                }
                onContextMenuRequested: {
                    flyout.close()
                    flyout.bar.openJumpList(index)
                }

                RayeTooltip {
                    visible: parent.hovered && parent.title !== ""
                    text: parent.title
                }
            }
        }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: ShellTokens.durationFast; easing: Motion.entrance }
            NumberAnimation { target: grid; property: "y"; from: flyout.topPadding + ShellTokens.menuSlide; to: flyout.topPadding; duration: ShellTokens.durationFast; easing: Motion.entrance }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: ShellTokens.durationFaster; easing: Motion.exit }
    }
}
