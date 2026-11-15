// Stand-in for Plasma's player list in the OSD tests and the harness: the
// player in use is whatever the test puts in currentPlayer.
import QtQuick

QtObject {
    property var currentPlayer: null
}
