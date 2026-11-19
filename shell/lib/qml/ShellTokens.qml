// The shell's own sizes, colours and timings: the taskbar, its buttons
// and pills, the flyouts. Colours, radii and type come from the Raye
// design tokens (org.raye.ui Tokens), so Night, Day and the user's accent
// follow; this file only names what is the shell's.
//
//   import org.raye.shell
//   Rectangle { color: ShellTokens.hoverFill; radius: ShellTokens.radiusButton }
//
// Easing curves for animations are on Motion (Motion.entrance, .exit,
// .pointToPoint); the durations here follow the user's animation speed
// and are 0 when animations are off.
pragma Singleton

import QtQuick
import org.raye.ui
import org.raye.shell

QtObject {
    // --- taskbar ----------------------------------------------------------
    readonly property int taskbarHeight: 48
    // "Show smaller taskbar buttons"
    readonly property int taskbarSmallHeight: 32
    readonly property int buttonSize: 40
    readonly property int buttonSmallSize: 32
    readonly property int iconSize: 24
    readonly property int iconSmallSize: 16
    // one button to the next
    readonly property int buttonPitch: 44
    // a labelled button when apps are not combined
    readonly property int labelButtonMinWidth: 160
    readonly property int labelButtonMaxWidth: 200
    // margin of the group when left aligned
    readonly property int edgeMargin: 8
    readonly property int showDesktopWidth: 8

    // the indicator under an open app: short and grey, long and accent
    // for the one in use
    readonly property int pillHeight: 3
    readonly property int pillShort: 6
    readonly property int pillLong: 16
    readonly property int badgeSize: 16

    // --- flyouts ----------------------------------------------------------
    // space between the taskbar and a flyout
    readonly property int flyoutGap: 12
    readonly property int startWidth: 680
    // Start on a screen at least wideStartScreen wide
    readonly property int startWideWidth: 840
    readonly property int wideStartScreen: 1600
    readonly property int startMaxHeight: 760
    readonly property int quickSettingsWidth: 360
    readonly property int toggleTileWidth: 96
    readonly property int toggleTileHeight: 48
    readonly property int splitTileArrowWidth: 32

    // --- shape ------------------------------------------------------------
    // [Appearance] Look=windows10: every corner of the shell is square
    readonly property bool windows10: ShellSettings.windows10
    readonly property real radiusButton: windows10 ? 0 : Tokens.radiusControl
    readonly property real radiusFlyout: windows10 ? 0 : Tokens.radiusOverlay
    readonly property real radiusTooltip: windows10 ? 0 : Tokens.radiusTooltip

    // --- colours ----------------------------------------------------------
    readonly property color text: Tokens.textPrimary
    readonly property color textSecondary: Tokens.textSecondary
    readonly property color accent: Tokens.accent
    readonly property color textOnAccent: Tokens.textOnAccent
    readonly property color taskbar: Tokens.acrylicTaskbar
    readonly property color taskbarSolid: Tokens.acrylicTaskbarSolid
    // the taskbar keeps its own shade in either mode: what sits on it
    // takes its colours from these, not from the page colours
    readonly property bool taskbarDark: Tokens.luminance(taskbarSolid) < 0.5
    readonly property color taskbarText: taskbarDark ? "#f1f0f4" : "#17151c"
    readonly property color taskbarHoverFill: taskbarDark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0.090, 0.082, 0.110, 0.05)
    readonly property color taskbarPressFill: taskbarDark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0.090, 0.082, 0.110, 0.03)
    readonly property color taskbarSelectedFill: taskbarDark ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(0.090, 0.082, 0.110, 0.07)
    readonly property color pillRunningOnTaskbar: Qt.alpha(taskbarText, 0.55)
    readonly property color acrylic: Tokens.acrylicTint
    readonly property color acrylicSolid: Tokens.acrylicTintSolid
    // the 1 px light line inside every surface, white 8 % at night
    readonly property color stroke: Tokens.strokeSurface
    // the faint violet rim of the surface that has the focus
    readonly property color focusRim: Qt.alpha(Tokens.accent, Tokens.night ? 0.35 : 0.25)
    readonly property color hoverFill: Tokens.fillSubtleHover
    readonly property color pressFill: Tokens.fillSubtlePressed
    // a button that is on (the app in use, an open flyout)
    readonly property color selectedFill: Tokens.night ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(0.090, 0.082, 0.110, 0.07)
    readonly property color pillRunning: Qt.alpha(Tokens.textPrimary, 0.55)
    readonly property color attention: "#e0a458"
    readonly property color badge: Tokens.accent
    readonly property color tooltip: Tokens.bgTooltip
    readonly property color separator: Tokens.strokeDivider

    // --- type -------------------------------------------------------------
    readonly property font caption: Tokens.caption
    readonly property font body: Tokens.body
    readonly property font bodyStrong: Tokens.bodyStrong

    // --- motion -----------------------------------------------------------
    readonly property int durationFaster: Tokens.durationFaster
    readonly property int durationFast: Tokens.durationFast
    readonly property int durationNormal: Tokens.durationNormal
    readonly property int durationSlow: Tokens.durationSlow
    readonly property real pressScale: 0.9
    // an app's icon hops when it starts
    readonly property int launchHop: 6
    // the Raye eye on the Start button blinks when pressed
    readonly property int blinkClosed: Tokens.durationFaster
    readonly property int blinkOpen: Tokens.durationFast
    // a flyout slides this far in as it opens
    readonly property int flyoutSlide: 40
    readonly property int menuSlide: 8
    readonly property int tooltipDelay: 500
    // the next tooltip, while one was just shown
    readonly property int tooltipReshowDelay: 100
}
