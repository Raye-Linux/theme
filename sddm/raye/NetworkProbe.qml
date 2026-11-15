// The network state, from plasma-nm's QML module: Main.qml loads this
// file with a Loader, so a missing module only hides the network icon.
import QtQuick
import org.kde.plasma.networkmanagement as PlasmaNM

Item {
    // plasma-nm's icon name ("network-wireless-60-locked", ...)
    readonly property string icon: connection.connectionIcon

    property PlasmaNM.NetworkStatus status: PlasmaNM.NetworkStatus {}
    property PlasmaNM.ConnectionIcon connection: PlasmaNM.ConnectionIcon {
        connectivity: status.connectivity
    }
}
