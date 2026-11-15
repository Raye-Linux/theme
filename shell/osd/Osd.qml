// The Raye on-screen display: a window with no frame of its own, centred
// near the bottom of the screen, just above the taskbar. What it shows is
// in OsdItem.qml.
//
// plasmashell creates it once and drives it through the properties below
// (keep their names and types: a build test compares them with the stock
// Osd.qml and with what plasmashell sets). At every show plasmashell sets
// animateOpacity false, opacity 1, visible true, then animateOpacity true
// and opacity 0, and hides the window again after `timeout` ms. The fade
// is done by OsdItem on the content; the window stays opaque until the
// end so a compositor that honours window opacity shows the same thing.
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.private.volume
import org.raye.shell

PlasmaCore.Dialog {
    id: root

    property alias timeout: osd.timeout
    property alias osdValue: osd.osdValue
    property alias osdMaxValue: osd.osdMaxValue
    property alias osdAdditionalText: osd.osdAdditionalText
    property alias icon: osd.icon
    property alias showingProgress: osd.showingProgress
    property bool animateOpacity: false

    location: PlasmaCore.Types.Floating
    type: PlasmaCore.Dialog.OnScreenDisplay
    backgroundHints: PlasmaCore.Dialog.NoBackground
    flags: Qt.WindowDoesNotAcceptFocus | Qt.WindowStaysOnTopHint
    hideOnWindowDeactivate: false
    // clicks go through, except on the media card's buttons
    outputOnly: !osd.interactive

    x: osd.windowX
    y: osd.windowY

    mainItem: OsdItem {
        id: osd
        shownWindow: root.visible

        // the volume keys: a short sound on the output in use, once per
        // press, and once more when a held key lets go
        VolumeFeedback { id: feedback }
        // one volume key press calls volumeChanged twice with the same value
        QtObject {
            id: lastVolume
            property real value: -1
            property double at: 0
        }
        VolumeChime {
            id: chime
            objectName: "volumeChime"
            enabled: ShellSettings.volumeChime
            onRing: {
                const sink = PreferredDevice.sink;
                if (sink && !sink.muted && osd.number > 0 && !osd.icon.startsWith("audio-volume-muted"))
                    feedback.play(sink.index);
            }
        }
    }

    // on the screen in use (where the pointer is), asked at every show:
    // plasmashell sets animateOpacity false just before it shows the window
    function place() {
        const area = ActiveOutput.geometry();
        if (area.width > 0 && area.height > 0)
            osd.area = area;
    }

    onAnimateOpacityChanged: {
        if (animateOpacity) {
            osd.show();
            if (osd.volume) {
                const now = Date.now();
                if (osd.number !== lastVolume.value || now - lastVolume.at > 100)
                    chime.changed();
                lastVolume.value = osd.number;
                lastVolume.at = now;
            }
        } else {
            place();
        }
    }

    onVisibleChanged: {
        if (visible) {
            place();
            osd.show();
        } else {
            osd.hidden();
        }
    }

    // opaque until plasmashell hides the window
    Behavior on opacity {
        enabled: root.animateOpacity
        SequentialAnimation {
            PauseAnimation { duration: root.timeout }
            NumberAnimation { duration: 0 }
        }
    }
}
