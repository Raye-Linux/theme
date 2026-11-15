// Plasma's own notifications item was in a tray of this shell and has been
// taken out. Its shared list of pop-ups outlives it: with no item left it
// cannot place them, and every notification also shows in the top left
// corner, next to Raye's. Its pop-ups only make sense while one of those
// items is there. Loaded only then: the module exists once it has loaded.
import QtQuick
import plasma.applet.org.kde.plasma.notifications as Stock

QtObject {
    Component.onCompleted: Stock.Globals.popupInstantiator.active = Qt.binding(() => Stock.Globals.plasmoid !== null)
}
