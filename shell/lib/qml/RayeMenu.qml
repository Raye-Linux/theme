// The shell's menu: Raye acrylic, 8 px corners, 4 px padding, rows of 32
// px; it fades in while sliding 8 px. It opens in a window of its own,
// so it can leave the taskbar or the flyout it belongs to.
//
//   RayeMenu {
//       id: menu
//       RayeMenuItem { text: i18nd("raye_shell_x", "Unpin from taskbar"); icon.name: "window-unpin" }
//       RayeMenuSeparator {}
//       RayeMenuItem { text: i18nd("raye_shell_x", "Close window"); shortcutText: "Alt+F4" }
//   }
//   ... menu.popup()
import QtQuick
import QtQuick.Templates as T
import org.raye.ui
import org.raye.shell

T.Menu {
    id: menu

    popupType: T.Popup.Window
    padding: Tokens.sizeMenuPadding
    margins: 0
    overlap: 4
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding, 200)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    delegate: RayeMenuItem {}

    contentItem: ListView {
        implicitHeight: contentHeight
        implicitWidth: {
            let w = 0
            for (let i = 0; i < count; ++i) {
                const item = itemAtIndex(i)
                if (item)
                    w = Math.max(w, item.implicitWidth)
            }
            return w
        }
        model: menu.contentModel
        interactive: Window.window ? contentHeight + menu.topPadding + menu.bottomPadding > Window.window.height : false
        clip: true
        currentIndex: menu.currentIndex
        keyNavigationEnabled: true
    }

    background: AcrylicSurface {
        implicitWidth: 200
        implicitHeight: Tokens.sizeMenuItem
        radius: ShellTokens.radiusFlyout
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: ShellTokens.durationFast; easing: Motion.entrance }
            NumberAnimation { target: menu.contentItem; property: "y"; from: menu.topPadding + ShellTokens.menuSlide; to: menu.topPadding; duration: ShellTokens.durationFast; easing: Motion.entrance }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: ShellTokens.durationFaster; easing: Motion.exit }
    }
}
