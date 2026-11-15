// A floating surface inside a Raye window: a flyout, a popup, a menu of
// our own. The acrylic tint with 8 px corners, a 1 px outline and the
// flyout shadow; with backdrop set, what lies under it in the window is
// blurred through the tint (frosted glass). Children go on the surface.
//
//   Acrylic { backdrop: page; width: 300; height: 200; Column { ... } }
//
// Windows of their own (menus, tooltips) get their blur from the
// compositor instead; this is for surfaces drawn inside a window.
import QtQuick
import QtQuick.Effects
import org.raye.ui

Item {
    id: surface

    // the item under the surface to blur through it; null: tint only
    property Item backdrop: null
    property real radius: Tokens.radiusOverlay
    property bool shadow: true
    // drawn opaque when "Transparency effects" is off (MicaWindow knows)
    property bool translucent: true
    default property alias content: inner.data

    readonly property var elevation: Tokens.elevationFlyout

    implicitWidth: inner.childrenRect.width + 2 * Tokens.sizeMenuPadding
    implicitHeight: inner.childrenRect.height + 2 * Tokens.sizeMenuPadding

    RectangularShadow {
        anchors.fill: parent
        visible: surface.shadow && surface.elevation.opacity > 0
        radius: surface.radius
        blur: surface.elevation.blur
        offset.y: surface.elevation.y
        color: Qt.alpha(Tokens.shadow, surface.elevation.opacity)
    }

    // the blurred backdrop, cut to the surface's rounded shape
    Item {
        id: frost
        anchors.fill: parent
        visible: surface.translucent && surface.backdrop !== null
        layer.enabled: visible
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: shape
        }

        ShaderEffectSource {
            id: grab
            anchors.fill: parent
            sourceItem: surface.backdrop
            live: true
            hideSource: false
            // the part of the backdrop under the surface
            sourceRect: {
                // mapToItem() is not watched: name what moves the surface
                surface.x; surface.y; surface.width; surface.height
                return surface.backdrop ? surface.mapToItem(surface.backdrop, 0, 0, surface.width, surface.height)
                                        : Qt.rect(0, 0, 0, 0)
            }
            visible: false
        }
        MultiEffect {
            anchors.fill: parent
            source: grab
            blurEnabled: true
            blurMax: 48
            blur: 1
            saturation: 0.2
        }
    }

    Rectangle {
        id: shape
        anchors.fill: parent
        radius: surface.radius
        visible: false
        layer.enabled: true
        color: "black"
    }

    Rectangle {
        objectName: "acrylicTint"
        anchors.fill: parent
        radius: surface.radius
        color: surface.translucent && surface.backdrop !== null ? Tokens.acrylicTint : Tokens.acrylicTintSolid
        border.width: 1
        border.color: Tokens.strokeSurface
    }

    Item {
        id: inner
        anchors.fill: parent
        anchors.margins: Tokens.sizeMenuPadding
    }
}
