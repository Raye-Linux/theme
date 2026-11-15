// A card on a page: the card colour over the page, a 1 px card stroke,
// 8 px corners and 16 px inside. Children go in the card.
//
//   Card { Label { text: "..." } }
import QtQuick
import QtQuick.Templates as T
import org.raye.ui

T.Frame {
    id: card

    // the colour of a card under the pointer, for cards that react to it
    property bool hoverable: false
    property real radius: Tokens.radiusOverlay

    padding: Tokens.s5
    hoverEnabled: hoverable
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    font: Tokens.body
    Accessible.role: Accessible.Pane

    background: Rectangle {
        radius: card.radius
        color: card.hoverable && card.hovered ? Tokens.bgCardHover : Tokens.bgCard
        border.width: 1
        border.color: Tokens.strokeCard

        Behavior on color {
            ColorAnimation { duration: Tokens.durationFaster }
        }
    }
}
