// The window of a Raye application (Settings, Store, Update, setup):
// while it is active the compositor blurs what is behind it and the
// window paints its background a little see-through over that, like
// Windows' Mica; inactive, or with "Transparency effects" off, or on a
// compositor without blur, it is plain and opaque. Same colours either way.
//
//   MicaWindow { title: "Settings"; width: 1000; height: 700; ... }
import QtQuick
import QtQuick.Window
import org.raye.ui
// the C++ part (plugin/): only in the built or installed module
import org.raye.ui.effects

Window {
    id: root

    // false: always opaque, whatever the settings say
    property bool micaEnabled: true
    readonly property bool mica: micaEnabled && active && effects.transparencyEffects
                                 && effects.blurAvailable

    color: mica ? Qt.alpha(Tokens.bgBase, Tokens.night ? 0.90 : 0.88) : Tokens.bgBase

    onActiveChanged: if (active) effects.refresh()

    WindowEffects {
        id: effects
        window: root
        blurBehind: root.micaEnabled && root.active
    }

    Behavior on color {
        ColorAnimation { duration: Tokens.durationFast }
    }
}
