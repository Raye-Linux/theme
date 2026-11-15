// The battery, from the module the stock sign-in screen uses: Main.qml
// loads this file with a Loader, so a missing module only hides the
// battery icon.
import QtQuick
import org.kde.plasma.private.battery

Item {
    readonly property bool hasBattery: battery.hasInternalBatteries
    readonly property int percent: battery.percent
    readonly property bool pluggedIn: battery.pluggedIn

    property BatteryControlModel battery: BatteryControlModel {}
}
