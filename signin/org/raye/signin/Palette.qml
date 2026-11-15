// Colours, sizes and pictures of the sign-in screens.
//
// The sign-in surfaces are always light text on a picture, whatever the
// app theme (like Windows), and the SDDM greeter runs as a user with no
// colour scheme at all: so every value here is a constant, nothing is
// read from Plasma or Kirigami.
//
// The accessibility state (high contrast, larger text) lives here too,
// so one toggle reaches every piece of the screen at once.
//
// The type is RayePalette (see qmldir): QtQuick already has a Palette
// type, and it wins over a module's own Palette in every file.
pragma Singleton

import QtQuick

QtObject {
    id: palette

    // --- pictures ----------------------------------------------------
    // Installed by raye-theme. The tests and the gallery point these at
    // the repo.
    property url assets: "file:///usr/share/raye/signin/"
    property url wallpaperDir: "file:///usr/share/wallpapers/Raye/contents/images/"
    property url eye: "file:///usr/share/pixmaps/raye-eye.png"

    readonly property url defaultFace: assets.toString() + "face-512.png"

    function icon(name) {
        return assets.toString() + "icons/" + name + ".svg"
    }

    // The default Raye picture for a screen <pixelHeight> device pixels high.
    function defaultPicture(pixelHeight) {
        return wallpaperDir.toString() + (pixelHeight > 1080 ? "3840x2160.png" : "1920x1080.png")
    }

    // The same picture blurred and tinted at build time, for the software
    // scene graph, which draws no blur. The sign-in tint is baked in.
    function defaultBlurred(pixelHeight) {
        return assets.toString() + (pixelHeight > 1080 ? "raye-night-blur-3840.jpg"
                                                       : "raye-night-blur-1920.jpg")
    }

    function isDefaultPicture(url) {
        var s = url.toString()
        return s === defaultPicture(1080) || s === defaultPicture(2160)
    }

    // --- accessibility ------------------------------------------------
    property bool highContrast: false
    property bool largerText: false
    readonly property real textScale: largerText ? 1.25 : 1.0

    // --- colours -----------------------------------------------------
    readonly property color night: "#100f13"
    readonly property color nightHigh: "#1b1a20"

    readonly property color text: highContrast ? "#ffffff" : "#f1f0f4"
    readonly property color textSecondary: highContrast ? "#ffffff" : "#b5b3bc"
    readonly property color accent: highContrast ? "#ffd400" : "#7b3cff"
    readonly property color accentHover: highContrast ? "#ffe24d" : "#8950ff"
    readonly property color accentPressed: highContrast ? "#e6bf00" : "#6a2fe6"
    readonly property color textOnAccent: highContrast ? "#000000" : "#ffffff"
    readonly property color glow: "#7b3cff"
    readonly property color error: "#ff7a9a"
    readonly property color warning: "#ffb84d"
    readonly property color warningBand: highContrast ? "#000000" : "#2effb84d"

    // tint over the picture: lock view 45 %, sign-in view 62 %
    readonly property real lockTint: 0.45
    readonly property real signInTint: 0.62
    // no blur (software scene graph, no pre-blurred file): a stronger tint
    readonly property real flatTint: 0.80

    readonly property color fieldFill: highContrast ? "#000000" : "#c71b1a20"
    readonly property color fieldBorder: highContrast ? "#ffffff" : "#38f1f0f4"
    readonly property color hoverFill: highContrast ? "#33ffffff" : "#14f1f0f4"
    readonly property color pressFill: highContrast ? "#1affffff" : "#0af1f0f4"
    readonly property color selectedFill: highContrast ? "#33ffffff" : "#1ff1f0f4"
    readonly property color ring: highContrast ? "#ffffff" : "#33f1f0f4"

    readonly property color flyoutFill: highContrast ? "#000000" : "#eb1b1a20"
    readonly property color flyoutBorder: highContrast ? "#ffffff" : "#1af1f0f4"

    readonly property color shadow: "#73000000"
    readonly property color dim: "#99000000"

    // focus ring: accent, or yellow and thicker in high contrast
    readonly property color focus: highContrast ? "#ffd400" : "#f1f0f4"
    readonly property int focusWidth: highContrast ? 3 : 2

    // --- type --------------------------------------------------------
    readonly property string displayFamily: "Inter Display"
    readonly property string family: "Inter"

    // --- shapes ------------------------------------------------------
    readonly property int controlRadius: 4
    readonly property int fieldRadius: 6
    readonly property int flyoutRadius: 8
    // distance of the corner bar and the user list from the screen edges
    readonly property int edge: 24
    readonly property int listEdge: 48
}
