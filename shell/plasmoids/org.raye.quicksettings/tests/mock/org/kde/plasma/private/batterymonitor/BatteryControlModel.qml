import QtQuick

QtObject {
    enum State { NoCharge, Charging, Discharging, FullyCharged }
    property bool hasInternalBatteries: false
    property int percent: 0
    property int state: 0
}
