// The values the Backend reads, as NetworkManager numbers them.
import QtQuick

QtObject {
    enum ConnectionType { Wired = 13, Vpn = 11, Wireless = 14 }
    enum ConnectionStatus { Unknown = 0, Activating = 1, Activated = 2 }
    enum SecurityType { UnknownSecurity = 0, NoneSecurity = 1, StaticWep = 2, WpaPsk = 5, Wpa2Psk = 7, SAE = 9, OWE = 11 }
}
