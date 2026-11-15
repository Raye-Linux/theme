// SPDX-License-Identifier: GPL-3.0-or-later
// When the animated wallpaper rests, and how a GIF keeps under its frame cap.
.pragma library

function isGif(url) {
    return /\.gif$/i.test(String(url))
}

// windows: [{ maximized, fullScreen, minimized }] of this screen and desktop
function covered(windows) {
    for (let i = 0; i < windows.length; ++i) {
        const w = windows[i]
        if (!w.minimized && (w.maximized || w.fullScreen))
            return true
    }
    return false
}

// a full-screen window counts even with "rest when covered" off: that is a
// game or a film, and the wallpaper under it is never seen
function shouldRest(o) {
    if (!o.visible)
        return true
    if (o.fullScreen)
        return true
    if (o.pauseCovered && o.covered)
        return true
    return o.pauseOnBattery && o.onBattery
}

// The next frame of a GIF drawn by a timer at most maxFps times a second.
// natural: the GIF's own frames a second. Returns { frame, carry }.
function nextFrame(frame, frameCount, carry, natural, maxFps) {
    if (frameCount < 1)
        return { frame: 0, carry: 0 }
    const step = carry + Math.max(natural, 0.01) / Math.max(maxFps, 1)
    const whole = Math.floor(step)
    return { frame: (frame + whole) % frameCount, carry: step - whole }
}

// the timer's interval: the cap, or slower when the GIF itself is slower
function interval(natural, maxFps) {
    const fps = Math.min(Math.max(natural, 1), Math.max(maxFps, 1))
    return Math.round(1000 / fps)
}
