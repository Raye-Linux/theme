// The picture behind every sign-in screen: sharp with a light tint on the
// lock view, blurred with a darker tint on the sign-in view.
//
//   source         the lock picture; the default Raye picture by default
//   blurredSource  the same picture already blurred and tinted (used by
//                  the software scene graph); found by itself for the
//                  default picture
//   blurred        sharp (lock view) or blurred (sign-in view)
//   tint           0..1 opacity of the night tint; follows `blurred`
//
// With a GPU the blur is a MultiEffect over a half-size copy of the
// picture. The software scene graph draws no MultiEffect: it then uses
// the pre-blurred file when there is one (its tint is baked in), or the
// sharp picture under a stronger tint. High contrast: plain black.
import QtQuick
import QtQuick.Effects

Item {
    id: backdrop

    readonly property real pixelHeight: height * Screen.devicePixelRatio
    property url source: RayePalette.defaultPicture(pixelHeight)
    property url blurredSource: RayePalette.isDefaultPicture(source)
                                ? RayePalette.defaultBlurred(pixelHeight) : ""
    property bool blurred: false
    property real tint: blurred ? RayePalette.signInTint : RayePalette.lockTint

    readonly property bool software: GraphicsInfo.api === GraphicsInfo.Software
    // what the software path ends up drawing
    readonly property bool usesPreblurred: software && blurred
                                           && preblurred.status === Image.Ready
    readonly property bool usesBlurEffect: !software && blurred

    clip: true

    Rectangle {
        anchors.fill: parent
        color: RayePalette.night
    }

    Image {
        id: picture
        anchors.fill: parent
        source: backdrop.source
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(Math.ceil(backdrop.width * Screen.devicePixelRatio),
                            Math.ceil(backdrop.height * Screen.devicePixelRatio))
        asynchronous: true
        cache: true
        smooth: true
    }

    // GPU: the blur, drawn from a half-size copy (cheaper, and smoother).
    // It is drawn a little larger than the screen so its soft edges are
    // cut off.
    Loader {
        id: blurLoader
        anchors.fill: parent
        active: !backdrop.software && !RayePalette.highContrast
        opacity: backdrop.blurred ? 1 : 0
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation { duration: Motion.slow; easing.type: Motion.enter }
        }
        sourceComponent: Item {
            ShaderEffectSource {
                id: half
                anchors.fill: parent
                sourceItem: picture
                textureSize: Qt.size(Math.max(1, Math.ceil(width / 2)),
                                     Math.max(1, Math.ceil(height / 2)))
                hideSource: false
                visible: false
                smooth: true
            }
            MultiEffect {
                anchors.fill: parent
                scale: 1.08
                source: half
                autoPaddingEnabled: false
                blurEnabled: true
                blurMax: 64
                blur: 1.0
            }
        }
    }

    // software: the pre-blurred picture, tint included
    Image {
        id: preblurred
        anchors.fill: parent
        source: backdrop.software && backdrop.blurredSource.toString() !== ""
                ? backdrop.blurredSource : ""
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(Math.ceil(backdrop.width * Screen.devicePixelRatio),
                            Math.ceil(backdrop.height * Screen.devicePixelRatio))
        asynchronous: true
        opacity: backdrop.usesPreblurred ? 1 : 0
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation { duration: Motion.slow; easing.type: Motion.enter }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: RayePalette.night
        opacity: {
            if (backdrop.usesPreblurred)
                return 0
            if (backdrop.software && backdrop.blurred)
                return RayePalette.flatTint
            return backdrop.tint
        }
        Behavior on opacity {
            NumberAnimation { duration: Motion.slow; easing.type: Motion.enter }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: RayePalette.highContrast
        color: "#000000"
    }
}
