// Stand-in for KWin's options in the snap tests: what the script reads.
pragma Singleton
import QtQuick

QtObject {
    property bool electricBorderMaximize: true
    signal configChanged()
}
