/*
    Maximise without the stock effect's blur. That one stretches a picture
    of the window at its old size to the new one and fades it into the real
    window: for a quarter of a second the window is soft, shown twice, and
    off its place. Here only the real window is drawn, at its real size and
    place, and it opens out from the frame it had, title bar first: each
    edge goes from where it was to where it is going. Nothing is ever scaled.
    Going back down, the window slides a short way into its place from the
    middle of the frame it left. A window dropped on a snap zone opens out
    from the top left of the zone, where the snap preview showed it.

    SPDX-License-Identifier: GPL-3.0-or-later
*/

"use strict";

// AnimationEffect::Anchor; 0 is the middle
const LEFT = 1, TOP = 2;

// the farthest a restored window travels, in logical pixels
const SETTLE_PX = 64;

// the slide of a restored window: a quarter of the way from the middle of
// the frame it left, or null when it is already there
function settle(oldGeometry, newGeometry) {
    if (!oldGeometry) {
        return null;
    }
    const clamp = v => Math.max(-SETTLE_PX, Math.min(SETTLE_PX, Math.round(v / 4)));
    const dx = clamp((oldGeometry.x + oldGeometry.width / 2) - (newGeometry.x + newGeometry.width / 2));
    const dy = clamp((oldGeometry.y + oldGeometry.height / 2) - (newGeometry.y + newGeometry.height / 2));
    return dx === 0 && dy === 0 ? null : { value1: dx, value2: dy };
}

// the opening-out, or null when the window got no bigger
function reveal(oldGeometry, newGeometry) {
    if (!oldGeometry || newGeometry.width <= 0 || newGeometry.height <= 0) {
        return null;
    }
    const fx = Math.min(1, oldGeometry.width / newGeometry.width);
    const fy = Math.min(1, oldGeometry.height / newGeometry.height);
    if (fx > 0.98 && fy > 0.98) {
        return null;
    }
    return { value1: fx, value2: fy };
}

// One axis of a maximise. KWin keeps a clip on the new frame's top left
// and does not move it with the window, so the window is shifted back by
// where it stood and the clip covers up to its old far edge: the first
// frame is exactly the old one.
function axis(oldStart, oldSize, newStart, newSize) {
    const shift = Math.max(0, Math.round(oldStart - newStart));
    return { shift: shift, size: Math.min(1, (shift + oldSize) / newSize) };
}

// the maximise opening-out, or null when the window got no bigger
function opening(oldGeometry, newGeometry) {
    if (!reveal(oldGeometry, newGeometry)) {
        return null;
    }
    const x = axis(oldGeometry.x, oldGeometry.width, newGeometry.x, newGeometry.width);
    const y = axis(oldGeometry.y, oldGeometry.height, newGeometry.y, newGeometry.height);
    return {
        from: { value1: x.size, value2: y.size },
        shift: x.shift || y.shift ? { value1: x.shift, value2: y.shift } : null
    };
}

// a new size later than this is not the maximise's any more
const WAIT_MS = 1000;

class RayeMaximize {
    constructor() {
        effect.configChanged.connect(this.loadConfig.bind(this));
        effects.windowAdded.connect(this.manage.bind(this));
        for (const window of effects.stackingOrder) {
            this.manage(window);
        }
        this.loadConfig();
    }

    loadConfig() {
        this.duration = animationTime(250);
        this.settleDuration = animationTime(167);
        this.fadeDuration = animationTime(83);
    }

    manage(window) {
        window.windowMaximizedStateAboutToChange.connect(this.aboutToChange.bind(this));
        window.windowMaximizedStateChanged.connect(this.changed.bind(this));
        window.windowFrameGeometryChanged.connect(this.geometryChanged.bind(this));
        if (window.windowFinishUserMovedResized) {
            window.windowFinishUserMovedResized.connect(this.dropped.bind(this));
        }
    }

    // let go after a drag: if KWin then gives it a bigger frame (a snap
    // zone), it opens out from where the preview stood
    dropped(window) {
        window.rayeDropped = Date.now();
    }

    aboutToChange(window) {
        if (window.rayeMaximize) {
            cancel(window.rayeMaximize);
            delete window.rayeMaximize;
        }
        window.rayeMaximizeFrom = Object.assign({}, window.geometry);
        window.rayeMaximizeWaiting = 0;
    }

    changed(window) {
        const from = window.rayeMaximizeFrom;
        if (!from || !window.visible) {
            delete window.rayeMaximizeFrom;
            return;
        }
        // the new size may come with the next frame of the window
        if (window.geometry.width === from.width && window.geometry.height === from.height) {
            window.rayeMaximizeWaiting = Date.now();
            return;
        }
        this.start(window);
    }

    geometryChanged(window, oldGeometry) {
        if (window.rayeMaximizeWaiting) {
            const late = Date.now() - window.rayeMaximizeWaiting > WAIT_MS;
            window.rayeMaximizeWaiting = 0;
            if (!late) {
                this.start(window);
                return;
            }
            delete window.rayeMaximizeFrom;
        }
        if (!window.rayeDropped) {
            return;
        }
        // the zone's size comes once the app has drawn at it: wait a little
        const recent = Date.now() - window.rayeDropped < 600;
        if (!recent || window.rayeMaximizeFrom || !oldGeometry) {
            delete window.rayeDropped;
            return;
        }
        const g = window.geometry;
        if (g.width === oldGeometry.width && g.height === oldGeometry.height) {
            return;
        }
        delete window.rayeDropped;
        const from = reveal(oldGeometry, g);
        if (from && this.duration > 1) {
            this.open(window, from, null);
        }
    }

    open(window, from, shift) {
        const animations = [{
            type: Effect.Clip,
            from: from,
            to: { value1: 1.0, value2: 1.0 },
            sourceAnchor: LEFT | TOP,
            targetAnchor: LEFT | TOP,
            curve: QEasingCurve.OutQuint
        }];
        if (shift) {
            animations.push({
                type: Effect.Translation,
                from: shift,
                to: { value1: 0, value2: 0 },
                curve: QEasingCurve.OutQuint
            });
        }
        window.rayeMaximize = animate({
            window: window,
            duration: this.duration,
            animations: animations
        });
    }

    start(window) {
        const from = window.rayeMaximizeFrom;
        delete window.rayeMaximizeFrom;
        delete window.rayeDropped;
        if (this.duration <= 1) {
            return;
        }
        const r = opening(from, window.geometry);
        if (r) {
            this.open(window, r.from, r.shift);
            return;
        }
        // pulled off the top by its title bar: it follows the pointer
        if (window.move || window.resize) {
            return;
        }
        // smaller: restored, it settles in
        const animations = [{
            type: Effect.Opacity,
            from: 0.7,
            to: 1.0,
            duration: this.fadeDuration,
            curve: QEasingCurve.Linear
        }];
        const offset = settle(from, window.geometry);
        if (offset) {
            animations.push({
                type: Effect.Translation,
                from: offset,
                to: { value1: 0, value2: 0 },
                curve: QEasingCurve.OutQuint
            });
        }
        window.rayeMaximize = animate({
            window: window,
            duration: this.settleDuration,
            animations: animations
        });
    }
}

// the tests load this file without KWin
if (typeof effect !== "undefined") {
    new RayeMaximize();
}
