// The shell's tooltip: 4 px corners, Caption type, half a second before it
// shows (a tenth right after another one), in a window of its own so it is
// never cut by the panel.
//
//   RayeTooltip { visible: button.hovered; text: i18nd("raye_shell_x", "Search") }
import QtQuick
import QtQuick.Templates as T
import org.raye.ui
import org.raye.shell

T.ToolTip {
    id: tip

    delay: TooltipWarmth.warm ? ShellTokens.tooltipReshowDelay : ShellTokens.tooltipDelay
    timeout: -1
    // above a bottom taskbar
    y: -implicitHeight - 8
    x: parent ? Math.round((parent.width - implicitWidth) / 2) : 0
    popupType: T.Popup.Window

    leftPadding: Tokens.sizeTooltipPaddingLeft
    rightPadding: Tokens.sizeTooltipPaddingRight
    topPadding: Tokens.sizeTooltipPaddingTop
    bottomPadding: Tokens.sizeTooltipPaddingBottom
    // the text on one line, in whole pixels: the window is a whole number
    // of pixels and a hair less than the text cut "Start" into "Star" / "t"
    readonly property real lineWidth: Math.ceil(Math.max(oneLine.advanceWidth, oneLine.width)) + leftPadding + rightPadding
    implicitWidth: Math.min(Tokens.sizeTooltipMaxWidth, lineWidth)
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

    TextMetrics {
        id: oneLine
        text: tip.text
        font: ShellTokens.caption
    }

    contentItem: Text {
        text: tip.text
        font: ShellTokens.caption
        color: ShellTokens.text
        // only a text longer than the widest tooltip goes on more lines
        wrapMode: tip.lineWidth > Tokens.sizeTooltipMaxWidth ? Text.Wrap : Text.NoWrap
        textFormat: Text.PlainText
    }

    background: Rectangle {
        radius: ShellTokens.radiusTooltip
        color: ShellTokens.tooltip
        border.width: 1
        border.color: ShellTokens.stroke
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: ShellTokens.durationFast; easing: Motion.entrance }
    }
    onAboutToShow: TooltipWarmth.shown()
    onClosed: TooltipWarmth.hidden()

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: ShellTokens.durationFaster; easing: Motion.exit }
    }
}
