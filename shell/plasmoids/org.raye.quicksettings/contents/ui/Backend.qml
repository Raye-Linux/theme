// What the system says and does for quick settings: network, sound, power,
// night light, media, Do not disturb. The rest of the applet only speaks
// to this object, so tests can hand it a stand-in (tests/FakeBackend.qml).
import QtQuick
import org.kde.bluezqt as BluezQt
import org.kde.kitemmodels as KItemModels
import org.kde.notificationmanager as NotificationManager
import org.kde.plasma.networkmanagement as PlasmaNM
import org.kde.plasma.private.battery
import org.kde.plasma.private.batterymonitor
import org.kde.plasma.private.brightnesscontrolplugin
import org.kde.plasma.private.mpris as Mpris
import org.kde.plasma.private.volume
import org.kde.plasma.workspace.dbus as DBus
import org.raye.shell

import "quicklogic.js" as Logic

Item {
    id: backend

    // the VPN list is only worth keeping up to date while the panel is open
    property bool watchVpn: false

    // --- network ---------------------------------------------------------
    PlasmaNM.Handler { id: handler }
    PlasmaNM.EnabledConnections { id: enabledConnections }
    PlasmaNM.AvailableDevices { id: availableDevices }
    PlasmaNM.NetworkStatus { id: networkStatus }
    PlasmaNM.ConnectionIcon { id: connectionIcon }

    readonly property string networkIcon: PlasmaNM.Configuration.airplaneModeEnabled
        ? "network-flightmode-on"
        : connectionIcon.connectionIcon + "-symbolic"
    readonly property string networkText: networkStatus.activeConnections

    readonly property bool wifiAvailable: availableDevices.wirelessDeviceAvailable
    readonly property bool wifiOn: enabledConnections.wirelessEnabled && !airplaneOn
    function setWifi(on) { handler.enableWireless(on) }

    readonly property bool airplaneAvailable: availableDevices.wirelessDeviceAvailable || availableDevices.modemDeviceAvailable
    readonly property bool airplaneOn: PlasmaNM.Configuration.airplaneModeEnabled
    function setAirplane(on) {
        handler.enableAirplaneMode(on)
        PlasmaNM.Configuration.airplaneModeEnabled = on
    }

    readonly property bool hotspotAvailable: availableDevices.wirelessDeviceAvailable && handler.hotspotSupported
    readonly property bool hotspotOn: handler.hotspotActive
    function toggleHotspot() {
        if (handler.hotspotActive)
            handler.stopHotspot()
        else
            handler.createHotspot()
    }

    // VPN: the saved ones, as { name, active, path, device, specific }
    readonly property bool vpnAvailable: vpnConnections.length > 0
    readonly property bool vpnOn: vpnConnections.some(c => c.active)
    property var vpnConnections: []
    function setVpnConnection(index, on) {
        const c = vpnConnections[index]
        if (!c)
            return
        if (on)
            handler.activateConnection(c.path, c.device, c.specific)
        else
            handler.deactivateConnection(c.path, c.device)
    }
    function readVpn() {
        const out = []
        for (let i = 0; i < vpnList.count; ++i) {
            const o = vpnList.objectAt(i)
            if (o)
                out.push({ name: o.name, active: o.state === PlasmaNM.Enums.Activated, path: o.path, device: o.device, specific: o.specific })
        }
        vpnConnections = out
    }
    // one list of connections for the VPN page and the network page
    PlasmaNM.NetworkModel { id: connectionModel }
    Instantiator {
        id: vpnList
        model: KItemModels.KSortFilterProxyModel {
            sourceModel: connectionModel
            // 19 is WireGuard, which the Enums of this module do not name.
            // The role numbers are asked of the model the rows come from.
            filterRowCallback: (row, parent) => {
                const roles = sourceModel.KItemModels.KRoleNames
                const typeRole = roles.role("Type")
                const pathRole = roles.role("ConnectionPath")
                if (typeRole < 0 || pathRole < 0)
                    return false
                const index = sourceModel.index(row, 0, parent)
                const type = sourceModel.data(index, typeRole)
                const path = sourceModel.data(index, pathRole)
                return (type === PlasmaNM.Enums.Vpn || type === 19) && !!path
            }
        }
        delegate: QtObject {
            // the model's roles start with a capital: not a property name
            readonly property string name: model.Name
            readonly property int state: model.ConnectionState
            readonly property string path: model.ConnectionPath
            readonly property string device: model.DevicePath
            readonly property string specific: model.SpecificPath
            onStateChanged: backend.readVpn()
        }
        onObjectAdded: backend.readVpn()
        onObjectRemoved: backend.readVpn()
    }

    // --- the network page: cables and Wi-Fi networks ----------------------
    readonly property bool wiredAvailable: availableDevices.wiredDeviceAvailable
    // as quicklogic.js describes them, the active ones first
    property var networkConnections: []
    readonly property var networkState: Logic.networkState(networkConnections, wifiAvailable, wifiOn)
    function readNetworks() {
        const out = []
        for (let i = 0; i < networkList.count; ++i) {
            const o = networkList.objectAt(i)
            if (!o)
                continue
            out.push({
                name: o.name,
                kind: o.type === PlasmaNM.Enums.Wired ? "wired" : "wifi",
                state: o.state === PlasmaNM.Enums.Activated ? "active"
                     : o.state === PlasmaNM.Enums.Activating ? "connecting" : "",
                saved: o.uuid !== "",
                secured: o.security !== PlasmaNM.Enums.NoneSecurity && o.security !== PlasmaNM.Enums.UnknownSecurity
                         && o.security !== PlasmaNM.Enums.OWE,
                // the ones a single password opens; the others are asked
                // for by the system's own prompt
                askPassword: o.uuid === "" && (o.security === PlasmaNM.Enums.StaticWep || o.security === PlasmaNM.Enums.WpaPsk
                             || o.security === PlasmaNM.Enums.Wpa2Psk || o.security === PlasmaNM.Enums.SAE),
                signal: o.signal,
                address: o.address,
                path: o.path,
                device: o.device,
                specific: o.specific,
            })
        }
        const sorted = Logic.sortNetworks(out)
        // the signal moves all the time: only what the list shows (the bars)
        // counts, so the rows are not rebuilt for nothing
        const key = JSON.stringify(sorted.map(c => [c.name, c.kind, c.state, c.saved, c.secured,
                                                    c.askPassword, Math.floor(c.signal / 25), c.address,
                                                    c.path, c.device, c.specific]))
        if (key === lastNetworkKey)
            return
        lastNetworkKey = key
        networkConnections = sorted
    }
    property string lastNetworkKey: ""
    // many networks change at once during a scan: read them at most every
    // 300 ms (a timer that restarts would never fire while signals keep moving)
    function readNetworksSoon() { if (!networkTimer.running) networkTimer.start() }
    Timer {
        id: networkTimer
        interval: 300
        onTriggered: backend.readNetworks()
    }
    function connectNetwork(index, password) {
        const c = networkConnections[index]
        if (!c)
            return
        const action = Logic.networkAction(c, password)
        if (action === "disconnect")
            handler.deactivateConnection(c.path, c.device)
        else if (c.saved)
            handler.activateConnection(c.path, c.device, c.specific)
        else if (password)
            handler.addAndActivateConnection(c.device, c.specific, password)
        else
            handler.addAndActivateConnection(c.device, c.specific)
    }
    function scanNetworks() { handler.requestScan() }
    function addressIn(details) {
        if (!details || typeof details.rowCount !== "function")
            return ""
        const role = details.KItemModels.KRoleNames.role("detailValue")
        if (role < 0)
            return ""
        const values = []
        for (let i = 0; i < details.rowCount(); ++i)
            values.push(details.data(details.index(i, 0), role))
        return Logic.ipv4Address(values)
    }
    Instantiator {
        id: networkList
        model: KItemModels.KSortFilterProxyModel {
            sourceModel: PlasmaNM.AppletProxyModel { sourceModel: connectionModel }
            filterRowCallback: (row, parent) => {
                const typeRole = sourceModel.KItemModels.KRoleNames.role("Type")
                if (typeRole < 0)
                    return false
                const type = sourceModel.data(sourceModel.index(row, 0, parent), typeRole)
                return type === PlasmaNM.Enums.Wired || type === PlasmaNM.Enums.Wireless
            }
        }
        delegate: QtObject {
            id: networkItem
            readonly property string name: model.ItemUniqueName || model.Name
            readonly property int type: model.Type
            readonly property int state: model.ConnectionState
            readonly property string uuid: model.Uuid || ""
            readonly property int security: model.SecurityType
            readonly property int signal: model.Signal
            readonly property string path: model.ConnectionPath
            readonly property string device: model.DevicePath
            readonly property string specific: model.SpecificPath
            // what NetworkManager shows of an active connection, as a model
            readonly property var details: model.ConnectionDetailsModel ?? null
            property string address: ""
            function readAddress() { address = state === PlasmaNM.Enums.Activated ? backend.addressIn(details) : "" }
            property Connections watchDetails: Connections {
                target: networkItem.details
                ignoreUnknownSignals: true
                function onModelReset() { networkItem.readAddress() }
                function onDataChanged() { networkItem.readAddress() }
                function onRowsInserted() { networkItem.readAddress() }
            }
            onDetailsChanged: readAddress()
            onStateChanged: { readAddress(); backend.readNetworksSoon() }
            onAddressChanged: backend.readNetworksSoon()
            onSignalChanged: backend.readNetworksSoon()
            onUuidChanged: backend.readNetworksSoon()
            Component.onCompleted: readAddress()
        }
        onObjectAdded: backend.readNetworksSoon()
        onObjectRemoved: backend.readNetworksSoon()
    }

    // --- Bluetooth -------------------------------------------------------
    readonly property bool bluetoothAvailable: BluezQt.Manager.bluetoothBlocked || BluezQt.Manager.adapters.length > 0
    readonly property bool bluetoothOn: BluezQt.Manager.bluetoothOperational
    function setBluetooth(on) {
        BluezQt.Manager.bluetoothBlocked = !on
        BluezQt.Manager.adapters.forEach(adapter => { adapter.powered = on })
    }

    // the devices paired with this PC: { name, icon, connected, address }
    property var bluetoothDevices: []
    function readBluetooth() {
        const out = []
        for (let i = 0; i < bluetoothList.count; ++i) {
            const o = bluetoothList.objectAt(i)
            if (o && o.paired)
                out.push({ name: o.name, icon: o.icon, connected: o.connected, address: o.address })
        }
        out.sort((a, b) => (b.connected - a.connected) || a.name.localeCompare(b.name))
        bluetoothDevices = out
    }
    function setBluetoothDevice(index, on) {
        const d = bluetoothDevices[index]
        if (!d)
            return
        for (let i = 0; i < bluetoothList.count; ++i) {
            const o = bluetoothList.objectAt(i)
            if (o && o.address === d.address) {
                if (on)
                    o.device.connectToDevice()
                else
                    o.device.disconnectFromDevice()
                return
            }
        }
    }
    Instantiator {
        id: bluetoothList
        model: BluezQt.DevicesModel {}
        delegate: QtObject {
            readonly property string name: model.Name || ""
            readonly property string icon: model.Icon || ""
            readonly property bool connected: !!model.Connected
            readonly property bool paired: !!model.Paired
            readonly property string address: model.Address || ""
            readonly property var device: model.Device
            onConnectedChanged: backend.readBluetooth()
            onPairedChanged: backend.readBluetooth()
        }
        onObjectAdded: backend.readBluetooth()
        onObjectRemoved: backend.readBluetooth()
    }

    // --- sound -----------------------------------------------------------
    // the outputs to choose from: { name, active }
    property var outputs: []
    function readOutputs() {
        const out = []
        for (let i = 0; i < outputList.count; ++i) {
            const o = outputList.objectAt(i)
            if (o)
                out.push({ name: o.name, active: o.isDefault })
        }
        outputs = out
    }
    function setOutput(index) {
        const o = outputList.objectAt(index)
        if (o && o.pulseObject)
            o.pulseObject.default = true
    }
    Instantiator {
        id: outputList
        model: PulseObjectFilterModel {
            filterVirtualDevices: true
            filterOutInactiveDevices: true
            sourceModel: SinkModel {}
        }
        delegate: QtObject {
            readonly property string name: model.Description || ""
            readonly property bool isDefault: !!model.Default
            readonly property var pulseObject: model.PulseObject
            onIsDefaultChanged: backend.readOutputs()
            onNameChanged: backend.readOutputs()
        }
        onObjectAdded: backend.readOutputs()
        onObjectRemoved: backend.readOutputs()
    }

    readonly property var sink: PreferredDevice.sink
    readonly property var source: PreferredDevice.source
    // what the sound server gives when there is no output at all
    readonly property bool volumeAvailable: !!sink && sink.name !== "auto_null"
    readonly property int volume: sink ? Math.round(sink.volume / PulseAudio.NormalVolume * 100) : 0
    readonly property bool muted: !!sink && sink.muted
    function setVolume(percent) {
        if (!sink)
            return
        sink.volume = Math.round(percent / 100 * PulseAudio.NormalVolume)
        if (percent > 0 && sink.muted)
            sink.muted = false
        chimeLevel = percent
        volumeChime.changed()
    }
    // the slider is held: one sound when it is let go, none while it moves
    function holdVolume() { volumeChime.hold() }
    function releaseVolume() { volumeChime.release() }

    // what was asked last: the sink reports its new volume a little later
    property int chimeLevel: 0
    VolumeFeedback { id: volumeFeedback }
    VolumeChime {
        id: volumeChime
        enabled: ShellSettings.volumeChime
        onRing: {
            if (backend.sink && !backend.sink.muted && backend.chimeLevel > 0)
                volumeFeedback.play(backend.sink.index)
        }
    }
    function toggleMute() {
        if (sink)
            sink.muted = !sink.muted
    }

    readonly property bool micAvailable: !!source
    readonly property bool micMuted: !!source && source.muted
    function toggleMic() {
        if (source)
            source.muted = !source.muted
    }

    // --- power -----------------------------------------------------------
    BatteryControlModel { id: batteryControl }
    readonly property bool hasBattery: batteryControl.hasInternalBatteries
    readonly property int batteryPercent: batteryControl.percent
    readonly property bool charging: batteryControl.state === BatteryControlModel.Charging

    PowerProfilesControl { id: profiles }
    readonly property bool energyAvailable: Array.from(profiles.profiles).indexOf("power-saver") >= 0
    readonly property bool energyOn: profiles.activeProfile === "power-saver"
    function setEnergySaver(on) {
        profiles.setProfile(on ? "power-saver" : "balanced")
    }

    // --- brightness: the first screen that can do it ----------------------
    ScreenBrightnessControl { id: screenBrightness }
    property string brightnessDisplay: ""
    property int brightness: 0
    readonly property bool brightnessAvailable: screenBrightness.isBrightnessAvailable && brightnessDisplay !== ""
    Repeater {
        model: screenBrightness.displays
        Item {
            required property int index
            required property string displayName
            required property int brightness
            required property int maxBrightness
            readonly property bool first: index === 0
            readonly property int percent: Logic.clampPercent(brightness, maxBrightness)
            onPercentChanged: update()
            onFirstChanged: update()
            Component.onCompleted: update()
            function update() {
                if (first) {
                    backend.brightnessDisplay = displayName
                    backend.brightness = percent
                }
            }
        }
    }
    function setBrightness(percent) {
        const displays = screenBrightness.displays
        const maxRole = displays.KItemModels.KRoleNames.role("maxBrightness")
        if (maxRole < 0)
            return
        const max = displays.data(displays.index(0, 0), maxRole)
        if (brightnessDisplay !== "" && max > 0)
            screenBrightness.setBrightness(brightnessDisplay, Math.round(percent * max / 100))
    }

    // --- night light -----------------------------------------------------
    DBus.Properties {
        id: nightLight
        busType: DBus.BusType.Session
        service: "org.kde.KWin.NightLight"
        path: "/org/kde/KWin/NightLight"
        iface: "org.kde.KWin.NightLight"
    }
    readonly property bool nightAvailable: Boolean(nightLight.properties.available)
    readonly property bool nightEnabled: Boolean(nightLight.properties.enabled)
    readonly property bool nightOn: nightEnabled && Boolean(nightLight.properties.running) && !Boolean(nightLight.properties.inhibited)
    // false: it was never turned on, and that happens on its Settings page
    function toggleNight() {
        if (!nightEnabled)
            return false
        NightLightInhibitor.toggleInhibition()
        return true
    }

    // --- Do not disturb --------------------------------------------------
    NotificationManager.Settings { id: notificationSettings }
    property date now: new Date()
    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: backend.now = new Date()
    }
    readonly property bool dndOn: {
        const until = notificationSettings.notificationsInhibitedUntil
        return notificationSettings.notificationsInhibitedByApplication
            || (until instanceof Date && !isNaN(until.getTime()) && until > now)
    }
    function toggleDnd() {
        if (dndOn) {
            notificationSettings.notificationsInhibitedUntil = undefined
            notificationSettings.revokeApplicationInhibitions()
        } else {
            // until turned off: Plasma reads a date far ahead as "no end"
            const d = new Date()
            d.setFullYear(d.getFullYear() + 100)
            notificationSettings.notificationsInhibitedUntil = d
        }
        notificationSettings.save()
        now = new Date()
    }

    // --- what is playing -------------------------------------------------
    Mpris.Mpris2Model { id: mpris }
    readonly property var player: mpris.currentPlayer
    readonly property bool hasPlayer: !!player && player.canControl
    readonly property string mediaTitle: player ? player.track : ""
    readonly property string mediaArtist: player ? player.artist : ""
    readonly property url mediaArt: player ? player.artUrl : ""
    readonly property string mediaSource: player ? player.identity : ""
    readonly property bool mediaPlaying: !!player && player.playbackStatus === Mpris.PlaybackStatus.Playing
    readonly property bool mediaCanPrevious: !!player && player.canGoPrevious
    readonly property bool mediaCanNext: !!player && player.canGoNext
    function mediaPlayPause() { if (player) player.PlayPause() }
    function mediaNext() { if (player) player.Next() }
    function mediaPrevious() { if (player) player.Previous() }
}
