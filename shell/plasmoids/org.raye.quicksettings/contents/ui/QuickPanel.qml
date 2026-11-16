// Quick settings (Win+A): the media card when something plays, the grid of
// tiles with its pages, the brightness and volume sliders, and the bottom
// strip with the battery, Edit and All settings. The window around it is
// main.qml's; what the system says and does is `backend`'s (Backend.qml).
// `margin` is the room left around the cards for their shadow.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import "quicklogic.js" as Logic

FocusScope {
    id: panel

    property QtObject backend
    property bool shown: true
    property bool editing: false
    property int currentPage: 0
    // "" the main view, or a page of the panel
    property string pageId: ""

    readonly property int margin: 12
    readonly property int padding: 16
    readonly property int gap: 12
    readonly property int columns: 3
    readonly property int cellWidth: ShellTokens.toggleTileWidth + 8
    readonly property int cellHeight: ShellTokens.toggleTileHeight + 6 + 16 + 12

    signal requestClose()
    signal openSettings(string page)
    signal launch(string desktopId)
    // the closing slide is over
    signal closed()

    // what the PC can do: a tile it cannot is not drawn at all
    readonly property var available: ({
        // the network tile: Wi-Fi, or the cable of a PC that has no Wi-Fi
        wifi: !!backend && (backend.wifiAvailable || backend.wiredAvailable),
        bluetooth: !!backend && backend.bluetoothAvailable,
        airplane: !!backend && backend.airplaneAvailable,
        energysaver: !!backend && backend.energyAvailable,
        nightlight: !!backend && backend.nightAvailable,
        mic: !!backend && backend.micAvailable,
        hotspot: !!backend && backend.hotspotAvailable,
        vpn: !!backend && backend.vpnAvailable,
    })
    property list<string> shownIds: Logic.shownTiles(Array.from(ShellSettings.quickSettingsTiles), available)
    readonly property int pageCount: Logic.pageCount(shownIds.length)
    readonly property int visiblePage: Math.min(currentPage, pageCount - 1)
    readonly property var addable: Logic.addableTiles(Array.from(shownIds), available)

    width: 360 + 2 * margin
    height: content.height + 2 * margin
    // as tall as the panel gets on any of its pages: the window keeps this
    // size while it is open, so a page change never resizes it
    readonly property int reserveHeight: 2 * margin
        + (media.visible ? media.height + gap : 0)
        + Math.max(body.height, networkPage.tallest, bluetoothPage.tallest,
                   soundPage.tallest, vpnPage.height) + strip.height
    focus: true
    Keys.onEscapePressed: {
        if (pageId !== "")
            pageId = ""
        else if (editing)
            editing = false
        else
            requestClose()
    }

    onShownChanged: if (shown) { pageId = ""; editing = false; wifiExpanded = false }

    function title(id) {
        switch (id) {
        case "wifi":
            return networkMode === "ethernet" ? i18nd("raye_shell_quicksettings", "Ethernet")
                                              : i18nd("raye_shell_quicksettings", "Wi-Fi")
        case "bluetooth": return i18nd("raye_shell_quicksettings", "Bluetooth")
        case "airplane": return i18nd("raye_shell_quicksettings", "Airplane mode")
        case "energysaver": return i18nd("raye_shell_quicksettings", "Energy saver")
        case "nightlight": return i18nd("raye_shell_quicksettings", "Night light")
        case "accessibility": return i18nd("raye_shell_quicksettings", "Accessibility")
        case "dnd": return i18nd("raye_shell_quicksettings", "Do not disturb")
        case "mic": return i18nd("raye_shell_quicksettings", "Mic off")
        case "project": return i18nd("raye_shell_quicksettings", "Project")
        case "vpn": return i18nd("raye_shell_quicksettings", "VPN")
        case "hotspot": return i18nd("raye_shell_quicksettings", "Mobile hotspot")
        }
        return ""
    }
    readonly property var network: backend && backend.networkState ? backend.networkState : ({ kind: "none", name: "" })
    // "ethernet" on the cable or without a Wi-Fi card, "wifi" otherwise
    readonly property string networkMode: Logic.networkMode(backend && backend.networkConnections ? backend.networkConnections : [],
                                                            !!backend && backend.wifiAvailable)
    // the Wi-Fi networks under the cable, opened by hand
    property bool wifiExpanded: false
    readonly property bool wifiListShown: !!backend && backend.wifiAvailable && (networkMode === "wifi" || wifiExpanded)
    onNetworkModeChanged: {
        if (networkMode === "ethernet")
            wifiExpanded = false
        else if (pageId === "network" && backend && backend.scanNetworks)
            backend.scanNetworks()
    }
    readonly property var activeWifi: {
        const list = backend && backend.networkConnections ? backend.networkConnections : []
        return list.find(c => c.kind === "wifi" && c.state === "active") || null
    }
    // what the network tile says under it: Ethernet, the Wi-Fi network,
    // Not connected
    function networkLabel() {
        if (networkMode === "ethernet") {
            switch (network.kind) {
            case "wired": return i18nd("raye_shell_quicksettings", "Ethernet")
            case "connecting": return i18nd("raye_shell_quicksettings", "Connecting…")
            }
            return i18nd("raye_shell_quicksettings", "Not connected")
        }
        switch (network.kind) {
        case "wired": return i18nd("raye_shell_quicksettings", "Ethernet")
        case "wifi": return network.name
        case "connecting": return i18nd("raye_shell_quicksettings", "Connecting…")
        case "off": return ""
        }
        return i18nd("raye_shell_quicksettings", "Not connected")
    }
    function subtitle(id) {
        return id === "wifi" ? networkLabel() : ""
    }
    // a glyph this applet draws itself is not an icon name
    function tileIcon(id) {
        if (id === "wifi" && networkMode === "ethernet")
            return "network-wired-symbolic"
        if (id === "wifi" && activeWifi)
            return Logic.wifiIcon(activeWifi.signal, false)
        const name = Logic.tileIcon(id)
        return name.startsWith("applet:") ? "" : name
    }
    function isOn(id) {
        if (!backend)
            return false
        switch (id) {
        case "wifi": return networkMode === "ethernet" ? network.kind === "wired" : backend.wifiOn
        case "bluetooth": return backend.bluetoothOn
        case "airplane": return backend.airplaneOn
        case "energysaver": return backend.energyOn
        case "nightlight": return backend.nightOn
        case "dnd": return backend.dndOn
        case "mic": return backend.micMuted
        case "vpn": return backend.vpnOn
        case "hotspot": return backend.hotspotOn
        }
        return false
    }
    function press(id) {
        switch (id) {
        case "wifi":
            // on the cable there is nothing to turn off: the tile shows its page
            if (networkMode === "ethernet")
                openPage("network")
            else
                backend.setWifi(!backend.wifiOn)
            break
        case "bluetooth": backend.setBluetooth(!backend.bluetoothOn); break
        case "airplane": backend.setAirplane(!backend.airplaneOn); break
        case "energysaver": backend.setEnergySaver(!backend.energyOn); break
        case "nightlight":
            // night light was never turned on: that happens on its page
            if (backend.toggleNight() === false)
                settings(id)
            break
        case "dnd": backend.toggleDnd(); break
        case "mic": backend.toggleMic(); break
        case "vpn": openPage("vpn"); break
        case "hotspot": backend.toggleHotspot(); break
        case "project":
            launch("org.raye.project.desktop")
            requestClose()
            break
        case "accessibility": settings(id); break
        }
    }
    // the "›" of a tile opens its page here; only the link at the bottom
    // of that page goes to Raye Settings
    function expand(id) {
        const page = Logic.tilePage(id)
        if (page !== "")
            openPage(page)
        else
            settings(id)
    }
    // a PC on a cable goes to the cable's page, not to Wi-Fi
    function settingsPageOf(id) {
        if (id === "wifi" && networkMode === "ethernet")
            return "network/ethernet"
        return Logic.settingsPage(id)
    }
    function settings(id) {
        const page = settingsPageOf(id)
        if (page !== "") {
            openSettings(page)
            requestClose()
        }
    }
    property int selectedRow: -1
    // the open Wi-Fi row and its typed key, by network: a scan rebuilds the
    // list every few seconds, and an index or the field itself would be lost
    property string selectedKey: ""
    property string typedKey: ""
    property string typedPassword: ""
    function networkStatus(c) {
        if (c.state === "active")
            return c.kind === "wired" && c.address ? i18nd("raye_shell_quicksettings", "Connected · %1", c.address)
                                                   : i18nd("raye_shell_quicksettings", "Connected")
        if (c.state === "connecting")
            return i18nd("raye_shell_quicksettings", "Connecting…")
        if (c.kind === "wired")
            return i18nd("raye_shell_quicksettings", "Not connected")
        return c.secured ? i18nd("raye_shell_quicksettings", "Secured") : i18nd("raye_shell_quicksettings", "Open")
    }
    function openPage(id) {
        selectedRow = -1
        selectedKey = ""
        typedKey = ""
        typedPassword = ""
        wifiExpanded = false
        pageId = id
        if (id === "network" && networkMode === "wifi" && backend && backend.scanNetworks)
            backend.scanNetworks()
    }
    function setWifiExpanded(on) {
        wifiExpanded = on
        if (on && backend && backend.scanNetworks)
            backend.scanNetworks()
    }
    // the rows of a kind, each with its place in the backend's list
    function networksOf(kind) {
        const list = backend && backend.networkConnections ? backend.networkConnections : []
        return list.map((c, i) => ({ c: c, index: i })).filter(r => r.c.kind === kind)
    }
    function leaveTo(page) {
        openSettings(page)
        requestClose()
    }
    // the right-click menu of a tile
    property string menuTile: ""
    readonly property alias tileMenu: tileMenu
    function showTileMenu(id, item, x, y) {
        if (editing || settingsPageOf(id) === "")
            return
        menuTile = id
        tileMenu.popup(item, x, y)
    }
    function save(list) { ShellSettings.quickSettingsTiles = list }
    function removeTile(id) { save(Logic.removeTile(Array.from(shownIds), id)) }
    function addTile(id) { save(Logic.addTile(Array.from(shownIds), id)) }
    function moveTile(from, to) { save(Logic.moveTile(Array.from(shownIds), from, to)) }

    component Shadow: Item {
        property Item target
        x: target.x
        y: target.y
        width: target.width
        height: target.height
        visible: target.visible
        Repeater {
            model: 4
            Rectangle {
                required property int index
                anchors.fill: parent
                anchors.margins: -(index + 1) * 3
                anchors.topMargin: -(index + 1) * 3 + 6
                radius: ShellTokens.radiusFlyout + (index + 1) * 3
                color: Qt.rgba(0, 0, 0, Tokens.night ? 0.07 : 0.03)
            }
        }
    }

    Item {
        id: slide
        x: panel.margin
        y: panel.margin + (panel.shown ? 0 : ShellTokens.flyoutSlide)
        width: content.width
        height: content.height
        opacity: panel.shown ? 1 : 0
        Behavior on y {
            NumberAnimation {
                duration: panel.shown ? ShellTokens.durationSlow : ShellTokens.durationFast
                easing: panel.shown ? Motion.entrance : Motion.exit
            }
        }
        Behavior on opacity {
            SequentialAnimation {
                NumberAnimation {
                    duration: panel.shown ? ShellTokens.durationSlow : ShellTokens.durationFast
                    easing: panel.shown ? Motion.entrance : Motion.exit
                }
                ScriptAction { script: if (!panel.shown) panel.closed() }
            }
        }

        Shadow { target: media }
        Shadow { target: main }

        Column {
            id: content
            width: 360
            spacing: panel.gap

            MediaCard {
                id: media
                objectName: "mediaCard"
                width: parent.width
                visible: !!panel.backend && panel.backend.hasPlayer
                backend: panel.backend
            }

            Item {
                id: main
                width: parent.width
                height: (panel.pageId === "" ? body.height
                         : panel.pageId === "vpn" ? vpnPage.height
                         : panel.pageId === "network" ? networkPage.height
                         : panel.pageId === "bluetooth" ? bluetoothPage.height
                         : soundPage.height) + strip.height

                AcrylicSurface {
                    anchors.fill: parent
                    focused: panel.activeFocus
                }

                Column {
                    id: body
                    width: parent.width
                    topPadding: panel.padding
                    bottomPadding: 8
                    visible: panel.pageId === ""
                    spacing: 8

                    // --- the tiles
                    Item {
                        id: gridHolder
                        x: panel.padding
                        width: parent.width - 2 * panel.padding
                        height: grid.implicitHeight

                        Grid {
                            id: grid
                            columns: panel.columns
                            columnSpacing: 8
                            rowSpacing: 12
                            Repeater {
                                model: Logic.page(Array.from(panel.shownIds), panel.visiblePage)
                                TileCell {
                                    id: tileCell
                                    required property string modelData
                                    required property int index
                                    tileId: modelData
                                    title: panel.title(modelData)
                                    iconName: panel.tileIcon(modelData)
                                    subtitle: panel.subtitle(modelData)
                                    checked: panel.isOn(modelData)
                                    split: Logic.isSplit(modelData)
                                    editing: panel.editing
                                    onToggled: panel.press(modelData)
                                    onExpandRequested: panel.expand(modelData)
                                    onMenuRequested: (x, y) => panel.showTileMenu(modelData, tileCell, x, y)
                                    onRemoveRequested: panel.removeTile(modelData)
                                    onDropped: (x, y) => panel.moveTile(
                                        panel.visiblePage * Logic.PER_PAGE + index,
                                        panel.visiblePage * Logic.PER_PAGE
                                            + Logic.cellAt(x, y, panel.cellWidth, panel.cellHeight, panel.columns))
                                }
                            }
                        }

                        // the dots of the pages, on the right
                        Column {
                            objectName: "pageDots"
                            visible: panel.pageCount > 1
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6
                            Repeater {
                                model: panel.pageCount
                                Rectangle {
                                    required property int index
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: index === panel.visiblePage ? ShellTokens.accent : Qt.alpha(ShellTokens.text, 0.3)
                                    MouseArea {
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        onClicked: panel.currentPage = parent.index
                                    }
                                }
                            }
                        }
                        WheelHandler {
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                            onWheel: event => {
                                const next = panel.visiblePage + (event.angleDelta.y < 0 ? 1 : -1)
                                panel.currentPage = Math.max(0, Math.min(panel.pageCount - 1, next))
                            }
                        }
                    }

                    // --- Edit: what can be added
                    Flow {
                        visible: panel.editing
                        x: panel.padding
                        width: parent.width - 2 * panel.padding
                        spacing: 6
                        Repeater {
                            model: panel.editing ? panel.addable : []
                            Rectangle {
                                required property string modelData
                                objectName: "add-" + modelData
                                width: addRow.implicitWidth + 16
                                height: 28
                                radius: ShellTokens.radiusButton
                                color: addArea.containsMouse ? Tokens.fillControlHover : Tokens.fillControl
                                border.width: 1
                                border.color: Tokens.strokeControl
                                Row {
                                    id: addRow
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Text {
                                        text: "+"
                                        font: ShellTokens.body
                                        color: ShellTokens.text
                                    }
                                    Text {
                                        text: panel.title(parent.parent.modelData)
                                        font: ShellTokens.caption
                                        color: ShellTokens.text
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                MouseArea {
                                    id: addArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: panel.addTile(parent.modelData)
                                    Accessible.role: Accessible.Button
                                    Accessible.name: i18nd("raye_shell_quicksettings", "Add %1", panel.title(parent.modelData))
                                }
                            }
                        }
                    }

                    // --- the sliders
                    Column {
                        x: panel.padding - 8
                        width: parent.width - 2 * panel.padding + 16
                        visible: !panel.editing
                        SliderRow {
                            objectName: "brightnessRow"
                            width: parent.width
                            visible: !!panel.backend && panel.backend.brightnessAvailable
                            iconName: "brightness-high-symbolic"
                            label: i18nd("raye_shell_quicksettings", "Brightness")
                            value: panel.backend ? panel.backend.brightness : 0
                            onMoved: value => panel.backend.setBrightness(value)
                        }
                        SliderRow {
                            objectName: "volumeRow"
                            width: parent.width
                            visible: !!panel.backend && panel.backend.volumeAvailable
                            iconName: Logic.volumeIcon(panel.backend ? panel.backend.volume : 0, panel.backend ? panel.backend.muted : true)
                            iconClickable: true
                            iconTip: panel.backend && panel.backend.muted ? i18nd("raye_shell_quicksettings", "Unmute")
                                                                          : i18nd("raye_shell_quicksettings", "Mute")
                            label: i18nd("raye_shell_quicksettings", "Volume")
                            value: panel.backend ? panel.backend.volume : 0
                            showArrow: true
                            onMoved: value => panel.backend.setVolume(value)
                            onPressedChanged: pressed ? panel.backend.holdVolume() : panel.backend.releaseVolume()
                            onIconClicked: panel.backend.toggleMute()
                            onArrowClicked: panel.openPage("sound")
                        }
                    }
                }

                // --- the VPN page
                Column {
                    id: vpnPage
                    width: parent.width
                    visible: panel.pageId === "vpn"
                    topPadding: 8
                    bottomPadding: 8

                    Row {
                        x: 8
                        spacing: 4
                        height: 40
                        PanelButton {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 32
                            height: 32
                            onTaskbar: false
                            toolTipText: i18nd("raye_shell_quicksettings", "Back")
                            onClicked: panel.pageId = ""
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                source: "go-previous-symbolic"
                                color: ShellTokens.text
                                isMask: true
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: i18nd("raye_shell_quicksettings", "VPN")
                            font: ShellTokens.bodyStrong
                            color: ShellTokens.text
                        }
                    }
                    Repeater {
                        model: panel.backend ? panel.backend.vpnConnections : []
                        Rectangle {
                            id: vpnRow
                            required property var modelData
                            required property int index
                            objectName: "vpn-" + index
                            signal clicked()
                            onClicked: panel.backend.setVpnConnection(index, !modelData.active)
                            x: 8
                            width: parent.width - 16
                            height: 44
                            radius: ShellTokens.radiusButton
                            color: vpnArea.containsMouse ? ShellTokens.hoverFill : "transparent"
                            Kirigami.Icon {
                                x: 8
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16
                                height: 16
                                source: "network-vpn-symbolic"
                                color: vpnRow.modelData.active ? ShellTokens.accent : ShellTokens.text
                                isMask: true
                            }
                            Column {
                                x: 34
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 42
                                Text {
                                    width: parent.width
                                    text: vpnRow.modelData.name
                                    font: ShellTokens.body
                                    color: ShellTokens.text
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                                Text {
                                    width: parent.width
                                    text: vpnRow.modelData.active ? i18nd("raye_shell_quicksettings", "Connected")
                                                                  : i18nd("raye_shell_quicksettings", "Not connected")
                                    font: ShellTokens.caption
                                    color: ShellTokens.textSecondary
                                    elide: Text.ElideRight
                                }
                            }
                            MouseArea {
                                id: vpnArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: vpnRow.clicked()
                                Accessible.role: Accessible.Button
                                Accessible.name: vpnRow.modelData.name
                            }
                        }
                    }
                    Text {
                        x: 16
                        width: parent.width - 32
                        height: 40
                        visible: !!panel.backend && panel.backend.vpnConnections.length === 0
                        verticalAlignment: Text.AlignVCenter
                        text: i18nd("raye_shell_quicksettings", "No VPN connections yet")
                        font: ShellTokens.body
                        color: ShellTokens.textSecondary
                        elide: Text.ElideRight
                    }
                }

                // --- the network page: the cable, and the Wi-Fi networks
                // unless the cable is in use (folded then) or there is no card
                SubPage {
                    id: networkPage
                    objectName: "networkPage"
                    width: parent.width
                    visible: panel.pageId === "network"
                    readonly property var wired: panel.pageId === "network" ? panel.networksOf("wired") : []
                    readonly property var wireless: panel.pageId === "network" && panel.wifiListShown ? panel.networksOf("wifi") : []
                    title: panel.networkMode === "ethernet" ? i18nd("raye_shell_quicksettings", "Ethernet")
                                                            : i18nd("raye_shell_quicksettings", "Wi-Fi")
                    hasSwitch: panel.networkMode === "wifi" && !!panel.backend && panel.backend.wifiAvailable
                    switchOn: !!panel.backend && panel.backend.wifiOn
                    linkText: i18nd("raye_shell_quicksettings", "Network and internet settings")
                    onBack: panel.pageId = ""
                    onSwitchToggled: on => panel.backend.setWifi(on)
                    onLinkClicked: panel.leaveTo("network")

                    Repeater {
                        model: networkPage.wired
                        delegate: networkRow
                    }
                    WifiFold {
                        objectName: "wifiFold"
                        visible: panel.networkMode === "ethernet" && !!panel.backend && panel.backend.wifiAvailable
                        expanded: panel.wifiExpanded
                        wifiOn: !!panel.backend && panel.backend.wifiOn
                        onClicked: panel.setWifiExpanded(!panel.wifiExpanded)
                        onSwitchToggled: on => panel.backend.setWifi(on)
                    }
                    Repeater {
                        model: networkPage.wireless
                        delegate: networkRow
                    }
                    Text {
                        objectName: "emptyNetworks"
                        x: 8
                        width: parent.width - 16
                        height: 40
                        visible: text !== ""
                        verticalAlignment: Text.AlignVCenter
                        text: {
                            if (!panel.backend)
                                return ""
                            if (panel.wifiListShown && networkPage.wireless.length === 0)
                                return panel.backend.wifiOn ? i18nd("raye_shell_quicksettings", "No networks found")
                                                            : i18nd("raye_shell_quicksettings", "Wi-Fi is turned off")
                            if (panel.networkMode === "ethernet" && networkPage.wired.length === 0)
                                return i18nd("raye_shell_quicksettings", "No cable plugged in")
                            return ""
                        }
                        font: ShellTokens.body
                        color: ShellTokens.textSecondary
                        elide: Text.ElideRight
                    }
                }
                Component {
                    id: networkRow
                    PageRow {
                        required property var modelData
                        readonly property var connection: modelData.c
                        readonly property int place: modelData.index
                        objectName: "network-" + place
                        iconName: Logic.networkIcon(connection)
                        name: connection.kind === "wired" ? (Logic.cableName(connection.name) || i18nd("raye_shell_quicksettings", "Ethernet")) : connection.name
                        status: panel.networkStatus(connection)
                        readonly property string key: connection.kind + ":" + connection.name
                        current: connection.state === "active"
                        selected: panel.selectedKey === key
                        askPassword: Logic.networkAction(connection, "") === "password"
                        actionText: connection.state !== "" ? i18nd("raye_shell_quicksettings", "Disconnect")
                                                            : i18nd("raye_shell_quicksettings", "Connect")
                        savedPassword: panel.typedKey === key ? panel.typedPassword : ""
                        onPasswordEdited: text => { panel.typedKey = key; panel.typedPassword = text }
                        onClicked: panel.selectedKey = selected ? "" : key
                        onAction: {
                            panel.backend.connectNetwork(place, password)
                            panel.selectedKey = ""
                            panel.typedKey = ""
                            panel.typedPassword = ""
                        }
                    }
                }

                // --- the Bluetooth page: the paired devices
                SubPage {
                    id: bluetoothPage
                    objectName: "bluetoothPage"
                    width: parent.width
                    visible: panel.pageId === "bluetooth"
                    title: i18nd("raye_shell_quicksettings", "Bluetooth")
                    hasSwitch: true
                    switchOn: !!panel.backend && panel.backend.bluetoothOn
                    linkText: i18nd("raye_shell_quicksettings", "More Bluetooth settings")
                    onBack: panel.pageId = ""
                    onSwitchToggled: on => panel.backend.setBluetooth(on)
                    onLinkClicked: panel.leaveTo("devices/bluetooth")

                    Repeater {
                        model: panel.pageId === "bluetooth" && panel.backend && panel.backend.bluetoothOn
                               ? panel.backend.bluetoothDevices : []
                        PageRow {
                            required property var modelData
                            required property int index
                            objectName: "bluetooth-" + index
                            iconName: (modelData.icon || "network-bluetooth") + "-symbolic"
                            name: modelData.name
                            status: modelData.connected ? i18nd("raye_shell_quicksettings", "Connected")
                                                        : i18nd("raye_shell_quicksettings", "Paired")
                            current: modelData.connected
                            selected: panel.selectedRow === index
                            actionText: modelData.connected ? i18nd("raye_shell_quicksettings", "Disconnect")
                                                            : i18nd("raye_shell_quicksettings", "Connect")
                            onClicked: panel.selectedRow = selected ? -1 : index
                            onAction: {
                                panel.backend.setBluetoothDevice(index, !modelData.connected)
                                panel.selectedRow = -1
                            }
                        }
                    }
                    Text {
                        x: 8
                        width: parent.width - 16
                        height: 40
                        visible: !!panel.backend && (!panel.backend.bluetoothOn || panel.backend.bluetoothDevices.length === 0)
                        verticalAlignment: Text.AlignVCenter
                        text: panel.backend && !panel.backend.bluetoothOn
                              ? i18nd("raye_shell_quicksettings", "Bluetooth is turned off")
                              : i18nd("raye_shell_quicksettings", "No paired devices")
                        font: ShellTokens.body
                        color: ShellTokens.textSecondary
                        elide: Text.ElideRight
                    }
                }

                // --- the sound page: where the sound goes
                SubPage {
                    id: soundPage
                    objectName: "soundPage"
                    width: parent.width
                    visible: panel.pageId === "sound"
                    title: i18nd("raye_shell_quicksettings", "Sound output")
                    linkText: i18nd("raye_shell_quicksettings", "More volume settings")
                    onBack: panel.pageId = ""
                    onLinkClicked: panel.leaveTo("system/sound")

                    Repeater {
                        model: panel.pageId === "sound" && panel.backend ? panel.backend.outputs : []
                        PageRow {
                            required property var modelData
                            required property int index
                            objectName: "output-" + index
                            iconName: "audio-volume-high-symbolic"
                            name: modelData.name
                            current: modelData.active
                            onClicked: panel.backend.setOutput(index)
                        }
                    }
                    Text {
                        x: 8
                        width: parent.width - 16
                        height: 40
                        visible: !!panel.backend && panel.backend.outputs.length === 0
                        verticalAlignment: Text.AlignVCenter
                        text: i18nd("raye_shell_quicksettings", "No sound outputs found")
                        font: ShellTokens.body
                        color: ShellTokens.textSecondary
                        elide: Text.ElideRight
                    }
                }

                // --- the bottom strip
                Rectangle {
                    id: strip
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 48
                    bottomLeftRadius: ShellTokens.radiusFlyout
                    bottomRightRadius: ShellTokens.radiusFlyout
                    color: Qt.alpha(Tokens.night ? "#000000" : "#ffffff", 0.18)
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: ShellTokens.separator
                    }

                    PanelButton {
                        objectName: "batteryButton"
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !!panel.backend && panel.backend.hasBattery
                        implicitWidth: batteryRow.implicitWidth + 16
                        width: implicitWidth
                        height: 32
                        onTaskbar: false
                        toolTipText: i18nd("raye_shell_quicksettings", "Power and battery settings")
                        onClicked: {
                            panel.openSettings("system/power")
                            panel.requestClose()
                        }
                        Row {
                            id: batteryRow
                            anchors.centerIn: parent
                            spacing: 6
                            Kirigami.Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16
                                height: 16
                                source: panel.backend ? Logic.batteryIcon(panel.backend.batteryPercent, panel.backend.charging) : ""
                                color: ShellTokens.text
                                isMask: true
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: panel.backend ? i18nd("raye_shell_quicksettings", "%1%", panel.backend.batteryPercent) : ""
                                font: ShellTokens.caption
                                color: ShellTokens.text
                            }
                        }
                    }

                    Row {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        Rectangle {
                            objectName: "doneButton"
                            visible: panel.editing
                            anchors.verticalCenter: parent.verticalCenter
                            width: doneText.implicitWidth + 24
                            height: 32
                            radius: ShellTokens.radiusButton
                            color: doneArea.pressed ? Qt.darker(ShellTokens.accent, 1.1) : ShellTokens.accent
                            Text {
                                id: doneText
                                anchors.centerIn: parent
                                text: i18nd("raye_shell_quicksettings", "Done")
                                font: ShellTokens.body
                                color: ShellTokens.textOnAccent
                            }
                            MouseArea {
                                id: doneArea
                                anchors.fill: parent
                                onClicked: panel.editing = false
                                Accessible.role: Accessible.Button
                                Accessible.name: doneText.text
                            }
                        }
                        PanelButton {
                            objectName: "editButton"
                            visible: !panel.editing && panel.pageId === ""
                            width: 32
                            height: 32
                            onTaskbar: false
                            toolTipText: i18nd("raye_shell_quicksettings", "Edit quick settings")
                            onClicked: panel.editing = true
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                source: "document-edit-symbolic"
                                color: ShellTokens.text
                                isMask: true
                            }
                        }
                        PanelButton {
                            objectName: "settingsButton"
                            visible: !panel.editing
                            width: 32
                            height: 32
                            onTaskbar: false
                            toolTipText: i18nd("raye_shell_quicksettings", "All settings")
                            onClicked: {
                                panel.openSettings("")
                                panel.requestClose()
                            }
                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                source: "configure-symbolic"
                                color: ShellTokens.text
                                isMask: true
                            }
                        }
                    }
                }
            }
        }
    }

    RayeMenu {
        id: tileMenu
        RayeMenuItem {
            objectName: "goToSettings"
            text: i18nd("raye_shell_quicksettings", "Go to settings")
            icon.name: "configure-symbolic"
            onTriggered: panel.settings(panel.menuTile)
        }
    }
}
