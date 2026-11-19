// What the Raye shell decides on Hyprland, kept apart from the windows so
// tests can run it: the taskbar rows made from Hyprland's client list, the
// requests sent back to it, and where Win+arrows puts a window.
.pragma library

// Minimized windows wait on a special workspace: Hyprland has no
// minimized state of its own.
var MINIMIZED = "special:raye-minimized"

function isMinimized(client) {
    return !!(client && client.workspace && client.workspace.name === MINIMIZED)
}

// Windows the taskbar shows: mapped, on a real or the minimized workspace.
// Other special workspaces (a scratchpad) are not "open apps".
function isShown(client) {
    if (!client || client.mapped === false || client.hidden === true)
        return false
    const ws = client.workspace || {}
    if (isMinimized(client))
        return true
    return typeof ws.id === "number" ? ws.id > 0 : true
}

function stripDesktop(id) {
    id = String(id || "")
    return id.length > 8 && id.substring(id.length - 8) === ".desktop" ? id.substring(0, id.length - 8) : id
}

// app: { id, name, icon } from the desktop entry of the window, or null
function keyFor(client, app) {
    if (app && app.id)
        return stripDesktop(app.id).toLowerCase()
    return String(client.class || client.initialClass || client.address || "").toLowerCase()
}

// One row per app (grouped) or per window, pinned apps first and in their
// order, then the others in the order they opened. Each row has the roles
// TaskBar.qml reads from Plasma's tasks model, plus `windows`.
//   clients  hyprctl -j clients
//   apps     { address: { id, name, icon } }
//   pinned   desktop ids, "org.kde.dolphin.desktop"
//   seen     { address: n }, the order the shell first saw each window in;
//            Hyprland's own list moves with focus, buttons must not
function taskRows(clients, activeAddress, pinned, apps, grouped, seen) {
    clients = (clients || []).filter(isShown)
    seen = seen || {}
    const place = c => (c.address in seen ? seen[c.address] : Number.MAX_SAFE_INTEGER)
    clients = clients.map((c, i) => ({ c: c, i: i }))
                     .sort((a, b) => (place(a.c) - place(b.c)) || (a.i - b.i))
                     .map(e => e.c)
    apps = apps || {}
    const groups = []
    const byKey = {}
    function groupFor(key, app, client) {
        if (!byKey[key]) {
            byKey[key] = { key: key, app: app, client: client, windows: [], pinned: false }
            groups.push(byKey[key])
        }
        return byKey[key]
    }
    const pins = []
    for (let i = 0; i < (pinned || []).length; ++i) {
        const id = String(pinned[i])
        const key = stripDesktop(id).toLowerCase()
        if (key === "" || byKey[key])
            continue
        const g = groupFor(key, { id: id, name: "", icon: "" }, null)
        g.pinned = true
        pins.push(g)
    }
    for (let i = 0; i < clients.length; ++i) {
        const c = clients[i]
        const app = apps[c.address] || null
        const g = groupFor(keyFor(c, app), app, c)
        if (app && app.id) {
            g.app = { id: app.id, name: app.name || g.app.name, icon: app.icon || g.app.icon }
        }
        g.windows.push({
            address: c.address,
            title: String(c.title || ""),
            active: c.address === activeAddress,
            minimized: isMinimized(c),
            urgent: !!c.urgent,
            pid: c.pid || 0
        })
    }
    const rows = []
    for (let i = 0; i < groups.length; ++i) {
        const g = groups[i]
        if (g.windows.length === 0) {
            rows.push(row(g, [], true))
        } else if (grouped) {
            rows.push(row(g, g.windows, false))
        } else {
            for (let w = 0; w < g.windows.length; ++w)
                rows.push(row(g, [g.windows[w]], false))
        }
    }
    return rows
}

function row(group, windows, launcher) {
    const app = group.app || {}
    const id = app.id ? String(app.id) : ""
    const one = windows.length === 1 ? windows[0] : null
    const appName = app.name || (group.client ? String(group.client.class || "") : "")
    return {
        display: one ? (one.title || appName) : appName,
        decoration: app.icon || (group.client ? String(group.client.class || "") : "") || "application-x-executable",
        AppName: appName,
        AppId: stripDesktop(id) || (group.client ? String(group.client.class || "") : ""),
        AppPid: one ? one.pid : (windows.length ? windows[0].pid : 0),
        LauncherUrlWithoutIcon: id ? "applications:" + id : "",
        desktopId: id,
        IsLauncher: launcher,
        IsStartup: false,
        IsActive: windows.some(w => w.active),
        IsMinimized: windows.length > 0 && windows.every(w => w.minimized),
        IsDemandingAttention: windows.some(w => w.urgent),
        HasLauncher: group.pinned,
        ChildCount: windows.length > 1 ? windows.length : 0,
        windows: windows
    }
}

// --- requests ------------------------------------------------------------

function addr(a) {
    return "address:" + String(a)
}

// Brings a window up: back from the minimized workspace onto the one in
// use first, then focused and on top of the other floating windows.
function activateCommands(window, workspaceId) {
    const out = []
    if (window.minimized)
        out.push("dispatch movetoworkspacesilent " + workspaceId + "," + addr(window.address))
    out.push("dispatch focuswindow " + addr(window.address))
    out.push("dispatch alterzorder top," + addr(window.address))
    return out
}

function minimizeCommands(window) {
    return ["dispatch movetoworkspacesilent " + MINIMIZED + "," + addr(window.address)]
}

function closeCommands(window) {
    return ["dispatch closewindow " + addr(window.address)]
}

// Several requests in one write, the way hyprctl --batch sends them.
function batch(commands) {
    return commands.length === 1 ? commands[0] : "[[BATCH]]" + commands.join(";")
}

// Win+D: every window of the workspace in use goes, and the same press
// brings exactly those back.
function showDesktop(clients, workspaceId, hidden) {
    if (hidden && hidden.length > 0) {
        return {
            hidden: [],
            commands: hidden.map(a => "dispatch movetoworkspacesilent " + workspaceId + "," + addr(a))
        }
    }
    const gone = (clients || []).filter(c => isShown(c) && !isMinimized(c)
                                         && c.workspace && c.workspace.id === workspaceId)
                                .map(c => c.address)
    return {
        hidden: gone,
        commands: gone.map(a => "dispatch movetoworkspacesilent " + MINIMIZED + "," + addr(a))
    }
}

// --- Win+arrows ----------------------------------------------------------

// The room windows may take on a monitor, in layout pixels:
// hyprctl -j monitors gives pixels and the panel's reserved edges.
function workArea(monitor) {
    const scale = monitor.scale > 0 ? monitor.scale : 1
    const r = monitor.reserved || [0, 0, 0, 0]  // left, top, right, bottom
    const transposed = monitor.transform % 2 === 1
    const w = Math.round((transposed ? monitor.height : monitor.width) / scale)
    const h = Math.round((transposed ? monitor.width : monitor.height) / scale)
    return { x: monitor.x + r[0], y: monitor.y + r[1], width: w - r[0] - r[2], height: h - r[1] - r[3] }
}

function rectFor(mode, area) {
    const hw = Math.floor(area.width / 2)
    const hh = Math.floor(area.height / 2)
    switch (mode) {
    case "left": return { x: area.x, y: area.y, width: hw, height: area.height }
    case "right": return { x: area.x + hw, y: area.y, width: area.width - hw, height: area.height }
    case "top-left": return { x: area.x, y: area.y, width: hw, height: hh }
    case "top-right": return { x: area.x + hw, y: area.y, width: area.width - hw, height: hh }
    case "bottom-left": return { x: area.x, y: area.y + hh, width: hw, height: area.height - hh }
    case "bottom-right": return { x: area.x + hw, y: area.y + hh, width: area.width - hw, height: area.height - hh }
    }
    return null
}

// What a Win+arrow does to a window, the way Windows 11 does it:
//   mode    "" (where the user put it), "left", "right", "top-left",
//           "top-right", "bottom-left", "bottom-right" or "maximized"
//   key     "left", "right", "up", "down"
// Gives { mode, action }: action is "snap", "maximize", "restore" (back to
// the size it had before it was snapped) or "minimize".
var SNAP_STEPS = {
    "": { left: "left", right: "right", up: "maximized", down: "minimize" },
    "left": { left: "left", right: "", up: "top-left", down: "bottom-left" },
    "right": { left: "", right: "right", up: "top-right", down: "bottom-right" },
    "top-left": { left: "top-left", right: "top-right", up: "top-left", down: "left" },
    "top-right": { left: "top-left", right: "top-right", up: "top-right", down: "right" },
    "bottom-left": { left: "bottom-left", right: "bottom-right", up: "left", down: "minimize" },
    "bottom-right": { left: "bottom-left", right: "bottom-right", up: "right", down: "minimize" },
    "maximized": { left: "left", right: "right", up: "maximized", down: "" }
}

function snapStep(mode, key) {
    const steps = SNAP_STEPS[mode || ""] || SNAP_STEPS[""]
    const next = steps[key]
    if (next === undefined)
        return { mode: mode || "", action: "none" }
    if (next === "minimize")
        return { mode: mode || "", action: "minimize" }
    if (next === "maximized")
        return { mode: "maximized", action: mode === "maximized" ? "none" : "maximize" }
    if (next === "")
        return { mode: "", action: "restore" }
    return { mode: next, action: next === mode ? "none" : "snap" }
}

// The requests for a step. `restore` is the place the window had before
// its first snap ({x, y, width, height}), `wasMaximized` whether Hyprland
// has it maximized now.
function snapCommands(step, window, area, restore, wasMaximized) {
    const a = addr(window.address)
    const out = []
    if (step.action === "minimize")
        return minimizeCommands(window)
    if (step.action === "maximize")
        return ["dispatch fullscreenstate 1 -1"]
    if (wasMaximized)
        out.push("dispatch fullscreenstate 0 -1")
    let r = null
    if (step.action === "snap")
        r = rectFor(step.mode, area)
    else if (step.action === "restore")
        r = restore
    if (!r)
        return out
    out.push("dispatch setfloating " + a)
    out.push("dispatch resizewindowpixel exact " + r.width + " " + r.height + "," + a)
    out.push("dispatch movewindowpixel exact " + r.x + " " + r.y + "," + a)
    return out
}

// --- events --------------------------------------------------------------

// The events after which the client list has changed: anything else on
// socket2 (a layer opening, the keyboard layout...) needs no new list.
var CLIENT_EVENTS = ["openwindow", "closewindow", "movewindow", "movewindowv2", "windowtitle",
                     "windowtitlev2", "activewindowv2", "urgent", "workspacev2",
                     "fullscreen", "changefloatingmode", "minimized", "focusedmonv2"]

function parseEvent(line) {
    const i = String(line).indexOf(">>")
    if (i < 0)
        return null
    return { name: line.substring(0, i), data: line.substring(i + 2) }
}

function changesClients(name) {
    return CLIENT_EVENTS.indexOf(name) >= 0
}
