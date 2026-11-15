/*
    Maximise without the stock effect's blur. That one stretches a picture
    of the window at its old size to the new one and fades it into the real
    window: for a quarter of a second the window is soft, shown twice, and
    off its place. Here only the real window is drawn, at its real size and
    place, and it opens out from the size it had. Nothing is ever scaled.
    Going back down is immediate.

    SPDX-License-Identifier: GPL-3.0-or-later
*/

"use strict";

// AnimationEffect::Anchor; 0 is the middle
const LEFT = 1, TOP = 2, RIGHT = 4, BOTTOM = 8;

// which edge of the new frame the old one stood against, on one axis
function anchor(oldStart, oldEnd, newStart, newEnd, start, end) {
    const toStart = Math.abs(oldStart - newStart);
    const toEnd = Math.abs(oldEnd - newEnd);
    const toMiddle = Math.abs((oldStart + oldEnd) - (newStart + newEnd)) / 2;
    if (toStart <= toEnd && toStart <= toMiddle) {
        return start;
    }
    return toEnd <= toMiddle ? end : 0;
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
    return {
        from: { value1: fx, value2: fy },
        anchor: anchor(oldGeometry.x, oldGeometry.x + oldGeometry.width,
                       newGeometry.x, newGeometry.x + newGeometry.width, LEFT, RIGHT)
              | anchor(oldGeometry.y, oldGeometry.y + oldGeometry.height,
                       newGeometry.y, newGeometry.y + newGeometry.height, TOP, BOTTOM)
    };
}

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
        this.duration = animationTime(200);
    }

    manage(window) {
        window.windowMaximizedStateAboutToChange.connect(this.aboutToChange.bind(this));
        window.windowMaximizedStateChanged.connect(this.changed.bind(this));
        window.windowFrameGeometryChanged.connect(this.geometryChanged.bind(this));
    }

    aboutToChange(window) {
        if (window.rayeMaximize) {
            cancel(window.rayeMaximize);
            delete window.rayeMaximize;
        }
        window.rayeMaximizeFrom = Object.assign({}, window.geometry);
        window.rayeMaximizeWaiting = false;
    }

    changed(window) {
        const from = window.rayeMaximizeFrom;
        if (!from || !window.visible) {
            delete window.rayeMaximizeFrom;
            return;
        }
        // the new size may come with the next frame of the window
        if (window.geometry.width === from.width && window.geometry.height === from.height) {
            window.rayeMaximizeWaiting = true;
            return;
        }
        this.start(window);
    }

    geometryChanged(window) {
        if (window.rayeMaximizeWaiting) {
            window.rayeMaximizeWaiting = false;
            this.start(window);
        }
    }

    start(window) {
        const r = reveal(window.rayeMaximizeFrom, window.geometry);
        delete window.rayeMaximizeFrom;
        if (!r || this.duration <= 0) {
            return;
        }
        window.rayeMaximize = animate({
            window: window,
            duration: this.duration,
            animations: [{
                type: Effect.Clip,
                from: r.from,
                to: { value1: 1.0, value2: 1.0 },
                sourceAnchor: r.anchor,
                targetAnchor: r.anchor,
                curve: QEasingCurve.OutCubic
            }]
        });
    }
}

// the tests load this file without KWin
if (typeof effect !== "undefined") {
    new RayeMaximize();
}
