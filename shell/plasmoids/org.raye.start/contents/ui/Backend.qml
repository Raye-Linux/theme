// What the Start menu shows and does, made the first time Start opens
// (nothing of it runs before): the pins (one list for every Start of the
// session, [Start] Pinned of rayeshellrc, so every screen and every restart
// shows the same), all apps, recent files and new apps, search, the
// account and power. The views (start/, search/) only talk to this object;
// tests give them a stand-in with the same names.
import QtCore
import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.private.kicker as Kicker
import org.kde.plasma.private.sessions as Sessions
import org.kde.coreaddons as KCoreAddons
import org.raye.shell

import "menulogic.js" as Menu

Item {
    id: backend

    // the PlasmoidItem, for the kicker models
    property Item applet
    // the popup asks to close (an app started, a menu entry chosen)
    signal done()

    visible: false

    readonly property var pins: Menu.cleanPins(ShellSettings.startPinned)
    readonly property alias pinnedModel: pinnedModel

    Kicker.SimpleFavoritesModel {
        id: pinnedModel
        favorites: backend.pins
    }

    function isPinned(id) { return Menu.isPinned(pins, id) }
    function pin(id) { ShellSettings.startPinned = Menu.pin(pins, id) }
    function unpin(id) { ShellSettings.startPinned = Menu.unpin(pins, id) }
    function moveToFront(id) { ShellSettings.startPinned = Menu.moveToFront(pins, id) }
    // `from` and `to` are rows of pinnedModel, which leaves out pins of
    // apps that are gone: map them back to the list
    function movePin(from, to) {
        const shown = pinnedRows()
        if (from < 0 || from >= shown.length || to < 0 || to >= shown.length || from === to)
            return
        const moved = Menu.movePin(shown, from, to)
        // pins not shown (apps not installed now) keep their place at the end
        ShellSettings.startPinned = moved.concat(pins.filter(id => shown.indexOf(id) < 0))
    }
    // the ids of the pins the grid shows, in its order
    function pinnedRows() {
        const out = []
        for (let i = 0; i < pinnedIds.count; ++i) {
            const o = pinnedIds.objectAt(i)
            if (o)
                out.push(Menu.pinId(o.favoriteId))
        }
        return out
    }
    Instantiator {
        id: pinnedIds
        model: pinnedModel
        delegate: QtObject {
            required property var model
            readonly property string favoriteId: model.favoriteId || ""
        }
    }

    // [Taskbar] Pinned for org.raye.taskbar, and the launchers of a stock
    // task list on this panel (its pins plugin copies them to every bar)
    function taskList() {
        const applets = applet && applet.Plasmoid.containment ? applet.Plasmoid.containment.applets : []
        for (let i = 0; i < applets.length; ++i) {
            const a = applets[i]
            if (a && (a.pluginName === "org.kde.plasma.icontasks" || a.pluginName === "org.kde.plasma.taskmanager"))
                return a
        }
        return null
    }
    function launcherUrl(id) {
        const v = Menu.pinId(id)
        return v.indexOf("://") > 0 ? v : "applications:" + v
    }
    function isOnTaskbar(id) {
        const v = Menu.pinId(id)
        const t = taskList()
        if (t && t.configuration && t.configuration.launchers)
            return Menu.cleanPins(t.configuration.launchers).indexOf(v) >= 0
        return Menu.isPinned(ShellSettings.taskbarPinned, v)
    }
    function setOnTaskbar(id, on) {
        const v = Menu.pinId(id)
        ShellSettings.taskbarPinned = on ? Menu.pin(ShellSettings.taskbarPinned, v) : Menu.unpin(ShellSettings.taskbarPinned, v)
        const t = taskList()
        if (t && t.configuration) {
            const current = t.configuration.launchers ? Array.from(t.configuration.launchers) : []
            const keep = current.filter(l => Menu.pinId(l) !== v)
            t.configuration.launchers = on ? keep.concat([launcherUrl(v)]) : keep
        }
    }

    // the Store's page of an app (Uninstall there), Raye Settings for the
    // rest: nothing of KDE's own dialogs
    function storeKey(id) {
        return Menu.pinId(id).replace(/\.desktop$/, "")
    }
    function uninstall(id) {
        const key = storeKey(id)
        // Windows programs are not in the Store; Settings lists and removes them
        if (key.indexOf("raye-win-") === 0)
            Launcher.openSettings("apps/installed")
        else
            Launcher.openUrl("appstream://" + key)
        done()
    }
    function appSettings(id) {
        Launcher.openSettings("apps/installed")
        done()
    }
    function launch(id) {
        const v = Menu.pinId(id)
        if (v.indexOf("://") > 0)
            Launcher.openUrl(v)
        else
            Launcher.launch(v)
        done()
    }
    function trigger(model, row) {
        if (model && row >= 0)
            model.trigger(row, "", null)
        done()
    }
    function openUrl(url) {
        Launcher.openUrl(url)
        done()
    }
    function openSettings(page) {
        Launcher.openSettings(page)
        done()
    }

    // every app, read when All apps (or a jump to a letter) is first shown
    property bool appsWanted: false
    Loader {
        id: rootLoader
        active: backend.appsWanted
        sourceComponent: Kicker.RootModel {
            autoPopulate: false
            appletInterface: backend.applet
            flat: true
            sorted: true
            showSeparators: false
            showTopLevelItems: true
            showAllApps: true
            showAllAppsCategorized: false
            showRecentApps: false
            showRecentDocs: false
            showPowerSession: false
            showFavoritesPlaceholder: false
            Component.onCompleted: refresh()
        }
    }
    // The flat list of every app, rebuilt when apps come or go, as plain
    // objects sorted with letters (the views add the headers).
    property var apps: []
    readonly property var appsSource: rootLoader.item && rootLoader.item.count > 0 ? rootLoader.item.modelForRow(0) : null
    Instantiator {
        id: appRows
        model: backend.appsSource
        delegate: QtObject {
            required property int index
            required property var model
            readonly property var entry: ({
                name: model.display || "",
                icon: model.decoration || "",
                favoriteId: model.favoriteId || "",
                url: model.url ? model.url.toString() : "",
                row: index
            })
        }
        onObjectAdded: appsTimer.restart()
        onObjectRemoved: appsTimer.restart()
    }
    Timer {
        id: appsTimer
        interval: 50
        onTriggered: {
            const out = []
            for (let i = 0; i < appRows.count; ++i) {
                const o = appRows.objectAt(i)
                if (o && o.entry.name !== "")
                    out.push(o.entry)
            }
            out.sort(Menu.compareApps)
            backend.apps = out
        }
    }
    function launchApp(entry) {
        const source = entry && entry.source ? entry.source : backend.appsSource
        if (entry && source)
            source.trigger(entry.row, "", null)
        done()
    }

    // The apps by Raye category (raye-applications.menu), read when the
    // Category view of All is first shown: [{ name, apps: [entry] }], in
    // the order of the menu file.
    property bool categoriesWanted: false
    property var categories: []
    Loader {
        id: categoryLoader
        active: backend.categoriesWanted
        sourceComponent: Kicker.AppsModel {
            flat: false
            sorted: false
            showSeparators: false
            showTopLevelItems: false
            appletInterface: backend.applet
        }
    }
    Instantiator {
        id: categoryRows
        model: categoryLoader.item
        delegate: QtObject {
            id: category
            required property int index
            required property var model
            readonly property string name: model.display || ""
            readonly property var source: model.hasChildren ? categoryLoader.item.modelForRow(index) : null
            property Instantiator rows: Instantiator {
                model: category.source
                delegate: QtObject {
                    required property int index
                    required property var model
                    readonly property var entry: ({
                        name: model.display || "",
                        icon: model.decoration || "",
                        favoriteId: model.favoriteId || "",
                        row: index,
                        source: category.source
                    })
                }
                onObjectAdded: categoriesTimer.restart()
                onObjectRemoved: categoriesTimer.restart()
            }
        }
        onObjectAdded: categoriesTimer.restart()
        onObjectRemoved: categoriesTimer.restart()
    }
    Timer {
        id: categoriesTimer
        interval: 50
        onTriggered: {
            const out = []
            for (let i = 0; i < categoryRows.count; ++i) {
                const c = categoryRows.objectAt(i)
                if (!c || !c.rows)
                    continue
                const apps = []
                for (let j = 0; j < c.rows.count; ++j) {
                    const a = c.rows.objectAt(j)
                    if (a && a.entry.name !== "" && a.entry.favoriteId !== "")
                        apps.push(a.entry)
                }
                apps.sort(Menu.compareApps)
                if (c.name !== "" && apps.length > 0)
                    out.push({ name: c.name, apps: apps })
            }
            backend.categories = out
        }
    }

    // the buttons next to power
    function openFolder(key) {
        switch (key) {
        case "settings": openSettings(""); return
        case "files": launch("org.kde.dolphin.desktop"); return
        case "network": openUrl("remote:/"); return
        case "documents": openUrl(StandardPaths.writableLocation(StandardPaths.DocumentsLocation)); return
        case "downloads": openUrl(StandardPaths.writableLocation(StandardPaths.DownloadLocation)); return
        case "music": openUrl(StandardPaths.writableLocation(StandardPaths.MusicLocation)); return
        case "pictures": openUrl(StandardPaths.writableLocation(StandardPaths.PicturesLocation)); return
        case "videos": openUrl(StandardPaths.writableLocation(StandardPaths.MoviesLocation)); return
        case "personal": openUrl(StandardPaths.writableLocation(StandardPaths.HomeLocation)); return
        }
    }

    // only there when the machine has Windows on it: raye-dualboot installs
    // this entry when the firmware lists a Windows boot entry
    readonly property bool canRestartToWindows: windowsEntry.valid
    AppJumpList {
        id: windowsEntry
        recentLimit: 0
        desktopId: "raye-restart-to-windows.desktop"
    }
    function restartToWindows() {
        launch("raye-restart-to-windows.desktop")
    }

    readonly property alias recentFiles: recentFiles
    readonly property alias newApps: newApps
    readonly property var mostUsedApps: mostUsedLoader.item
    Kicker.RecentUsageModel {
        id: recentFiles
        shownItems: Kicker.RecentUsageModel.OnlyDocs
        ordering: 0
    }
    // nothing asks the activity manager for this while it is off
    Loader {
        id: mostUsedLoader
        active: ShellSettings.startMostUsed
        sourceComponent: Kicker.RecentUsageModel {
            shownItems: Kicker.RecentUsageModel.OnlyApps
            ordering: Kicker.RecentUsageModel.Popular
        }
    }
    RecentlyAddedModel {
        id: newApps
        limit: 6
    }

    readonly property alias runner: runner
    readonly property var runnerIds: Menu.searchRunners
    Kicker.RunnerModel {
        id: runner
        appletInterface: backend.applet
        mergeResults: false
        runners: Menu.searchRunners
        favoritesModel: pinnedModel
    }

    readonly property alias user: user
    readonly property alias sessions: sessions
    KCoreAddons.KUser {
        id: user
    }
    Sessions.SessionManagement {
        id: sessions
    }
    readonly property bool updatesReady: UpdateState.restartPending

    // Shut down, Restart and Sign out go through the session's end screen
    // (ForcePrompt): Start's own menu was the question, so that screen asks
    // nothing and stays up with the words until the session is gone. With
    // Skip nothing would be on screen while applications close. The mode is
    // passed by name, its numbers are not the obvious ones.
    function power(action) {
        switch (action) {
        case "sleep": sessions.suspend(); break
        case "shutdown": sessions.requestShutdown(Sessions.SessionManagement.ForcePrompt); break
        case "restart": sessions.requestReboot(Sessions.SessionManagement.ForcePrompt); break
        case "update-shutdown": UpdateState.updateAndShutdown(); break
        case "update-restart": UpdateState.updateAndRestart(); break
        case "lock": sessions.lock(); break
        case "signout": sessions.requestLogout(Sessions.SessionManagement.ForcePrompt); break
        case "switch": sessions.switchUser(); break
        case "account": Launcher.openSettings("accounts/your-info"); break
        }
        done()
    }

    // the stock launcher's pins, moved over once
    // Only looked at while the Start pins are still Raye's defaults. Its
    // links are shared by every stock launcher (kactivitymanagerd); the
    // model's `favorites` list is always empty, so the rows are read.
    Loader {
        id: migration
        active: Menu.cleanPins(ShellSettings.startPinned).join(",") === Menu.defaultPins.join(",")
        sourceComponent: Kicker.KAStatsFavoritesModel {
            Component.onCompleted: initForClient("org.raye.start.migration")
        }
    }
    Instantiator {
        id: stockRows
        model: migration.item
        delegate: QtObject {
            required property var model
            readonly property string favoriteId: model.favoriteId || ""
        }
        onObjectAdded: migrateTimer.restart()
    }
    Timer {
        id: migrateTimer
        interval: 500
        onTriggered: {
            const stock = []
            for (let i = 0; i < stockRows.count; ++i) {
                const o = stockRows.objectAt(i)
                if (o && o.favoriteId !== "")
                    stock.push(o.favoriteId)
            }
            const next = Menu.migratedPins(stock, ShellSettings.startPinned, Menu.defaultPins, null)
            if (next)
                ShellSettings.startPinned = next
        }
    }
}
