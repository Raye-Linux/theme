// The panel in a window like the one main.qml makes, to see the window
// keep its size when a page opens or closes.
import QtQuick
import org.kde.plasma.core as PlasmaCore

import "../contents/ui"

PlasmaCore.Dialog {
    id: dialog

    property QtObject backend: FakeBackend {}
    property Item panel: quick

    location: PlasmaCore.Types.Floating
    type: PlasmaCore.Dialog.AppletPopup
    backgroundHints: PlasmaCore.Dialog.NoBackground
    x: 40
    y: 40

    mainItem: PanelFrame {
        id: frame
        panel: quick
        QuickPanel {
            id: quick
            y: frame.height - height
            backend: dialog.backend
        }
    }
}
