// Text drawn over a picture, with the soft shadow of the sign-in screens.
//
// With a GPU the shadow is a MultiEffect (blur 0.5, 1 px down, black
// 45 %). The software scene graph draws no effect at all (the text would
// vanish with it), so there a second copy of the text 1 px down stands in.
import QtQuick
import QtQuick.Effects

Text {
    id: label

    property bool shadow: !RayePalette.highContrast
    readonly property bool software: GraphicsInfo.api === GraphicsInfo.Software

    color: RayePalette.text
    font.family: RayePalette.family
    font.pixelSize: 14
    textFormat: Text.PlainText
    renderType: Text.QtRendering

    layer.enabled: shadow && !software && text !== ""
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowBlur: 0.5
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 1
        shadowColor: RayePalette.shadow
    }

    Text {
        z: -1
        y: 1
        width: label.width
        height: label.height
        visible: label.shadow && label.software
        text: label.text
        font: label.font
        color: RayePalette.shadow
        textFormat: label.textFormat
        horizontalAlignment: label.horizontalAlignment
        verticalAlignment: label.verticalAlignment
        wrapMode: label.wrapMode
        elide: label.elide
        maximumLineCount: label.maximumLineCount
        lineHeight: label.lineHeight
        lineHeightMode: label.lineHeightMode
        renderType: label.renderType
        Accessible.ignored: true
    }
}
