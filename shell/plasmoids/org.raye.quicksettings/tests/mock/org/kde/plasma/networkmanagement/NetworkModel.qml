// Stand-in for the connection list in the Backend test: two VPNs (one of
// them WireGuard, type 19), the cable, a saved Wi-Fi network, a VPN that is
// not saved, and a Wi-Fi network in range that was never used. The cable
// carries the details NetworkManager gives for an active connection.
// The roles start with a capital, like the real model's.
import QtQuick

ListModel {
    Component.onCompleted: {
        append({ Name: "Office", ItemUniqueName: "Office", Type: 11, ConnectionPath: "/vpn/1", ConnectionState: 2, DevicePath: "/dev/1", SpecificPath: "", Uuid: "u1", SecurityType: 0, Signal: 0, ConnectionDetailsModel: [] })
        append({ Name: "Home", ItemUniqueName: "Home", Type: 14, ConnectionPath: "/wifi/1", ConnectionState: 0, DevicePath: "/dev/2", SpecificPath: "/ap/1", Uuid: "u2", SecurityType: 7, Signal: 60, ConnectionDetailsModel: [] })
        append({ Name: "Tunnel", ItemUniqueName: "Tunnel", Type: 19, ConnectionPath: "/vpn/2", ConnectionState: 0, DevicePath: "", SpecificPath: "", Uuid: "u3", SecurityType: 0, Signal: 0, ConnectionDetailsModel: [] })
        append({ Name: "Not saved", ItemUniqueName: "Not saved", Type: 11, ConnectionPath: "", ConnectionState: 0, DevicePath: "", SpecificPath: "", Uuid: "", SecurityType: 0, Signal: 0, ConnectionDetailsModel: [] })
        append({ Name: "Wired connection 1", ItemUniqueName: "Wired connection 1", Type: 13, ConnectionPath: "/wired/1", ConnectionState: 2, DevicePath: "/dev/3", SpecificPath: "", Uuid: "u4", SecurityType: 0, Signal: 0,
                 ConnectionDetailsModel: [
                     { isSection: false, detailLabel: "Hardware Address", detailValue: "AA:BB:CC:DD:EE:FF" },
                     { isSection: false, detailLabel: "IPv4 Address", detailValue: "192.168.50.50" },
                     { isSection: false, detailLabel: "IPv4 Gateway", detailValue: "192.168.50.1" },
                 ] })
        append({ Name: "Cafe", ItemUniqueName: "Cafe", Type: 14, ConnectionPath: "", ConnectionState: 0, DevicePath: "/dev/2", SpecificPath: "/ap/2", Uuid: "", SecurityType: 7, Signal: 90, ConnectionDetailsModel: [] })
    }
}
