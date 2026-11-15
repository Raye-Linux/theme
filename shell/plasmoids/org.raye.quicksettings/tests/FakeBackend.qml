// A stand-in for Backend.qml: the same names, no system service behind
// them, and a list of what the panel asked for in `calls`.
import QtQuick

import "../contents/ui/quicklogic.js" as Logic

QtObject {
    property list<string> calls
    function called(what) { calls = Array.from(calls).concat([what]) }

    property string networkIcon: "network-wired-activated"
    property string networkText: "Wired connection"

    property bool wifiAvailable: true
    property bool wifiOn: true
    property string wifiName: "Maison"
    function setWifi(on) { called("wifi:" + on); wifiOn = on }

    property bool wiredAvailable: true
    // on the cable, with two Wi-Fi networks in range
    property var networkConnections: [
        { name: "Wired connection 1", kind: "wired", state: "active", saved: true, secured: false, askPassword: false, signal: 0, address: "192.168.50.50" },
        { name: "Maison", kind: "wifi", state: "", saved: true, secured: true, askPassword: false, signal: 80 },
        { name: "Cafe", kind: "wifi", state: "", saved: false, secured: true, askPassword: true, signal: 40 },
    ]
    readonly property var networkState: Logic.networkState(networkConnections, wifiAvailable, wifiOn)
    function connectNetwork(index, password) { called("network:" + index + (password ? ":" + password : "")) }
    function scanNetworks() { called("scan") }

    property var bluetoothDevices: [
        { name: "Headphones", icon: "audio-headphones", connected: true, address: "AA:02" },
        { name: "Mouse", icon: "input-mouse", connected: false, address: "AA:01" },
    ]
    function setBluetoothDevice(index, on) { called("btdevice:" + index + ":" + on) }

    property var outputs: [{ name: "Speakers", active: true }, { name: "Headphones", active: false }]
    function setOutput(index) { called("output:" + index) }

    property bool bluetoothAvailable: true
    property bool bluetoothOn: false
    function setBluetooth(on) { called("bluetooth:" + on); bluetoothOn = on }

    property bool airplaneAvailable: true
    property bool airplaneOn: false
    function setAirplane(on) { called("airplane:" + on); airplaneOn = on }

    property bool energyAvailable: true
    property bool energyOn: false
    function setEnergySaver(on) { called("energy:" + on); energyOn = on }

    property bool nightAvailable: true
    property bool nightOn: true
    function toggleNight() { called("night"); nightOn = !nightOn }

    property bool dndOn: false
    function toggleDnd() { called("dnd"); dndOn = !dndOn }

    property bool micAvailable: true
    property bool micMuted: false
    function toggleMic() { called("mic"); micMuted = !micMuted }

    property bool hotspotAvailable: false
    property bool hotspotOn: false
    function toggleHotspot() { called("hotspot"); hotspotOn = !hotspotOn }

    property bool vpnAvailable: false
    property bool vpnOn: false
    property var vpnConnections: []
    function toggleVpn() { called("vpn"); vpnOn = !vpnOn }
    function setVpnConnection(index, on) { called("vpnconn:" + index + ":" + on) }

    property bool volumeAvailable: true
    property int volume: 45
    property bool muted: false
    function setVolume(v) { called("volume:" + v); volume = v }
    function toggleMute() { called("mute"); muted = !muted }
    function holdVolume() { called("holdvolume") }
    function releaseVolume() { called("releasevolume") }

    property bool brightnessAvailable: true
    property int brightness: 70
    function setBrightness(v) { called("brightness:" + v); brightness = v }

    property bool hasBattery: true
    property int batteryPercent: 82
    property bool charging: false

    property bool hasPlayer: true
    property string mediaTitle: "Night Drive"
    property string mediaArtist: "Raye Sessions"
    property url mediaArt: ""
    property string mediaSource: "Music"
    property bool mediaPlaying: true
    property bool mediaCanPrevious: true
    property bool mediaCanNext: true
    function mediaPlayPause() { called("playpause"); mediaPlaying = !mediaPlaying }
    function mediaNext() { called("next") }
    function mediaPrevious() { called("previous") }

    function activate() { called("activate") }
}
