/*
    Raye windows: a new window settles in from 94 % over 250 ms while it
    fades in within the first 83 ms; a closed one shrinks to 96 % and fades
    out in 167 ms. The stock scale effect runs fade and zoom on one curve,
    so a window was half see-through for most of its opening.

    Based on KWin's "Scale" effect.
    SPDX-FileCopyrightText: 2018, 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Raye Linux

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

const blacklist = {
    // the logout screen and the splash have their own effects
    "ksmserver ksmserver": [],
    "ksmserver-logout-greeter ksmserver-logout-greeter": [],
    "ksplashqml ksplashqml": [],
    "spectacle org.kde.spectacle": ["region-editor"],
};

class RayeWindows {
    constructor() {
        effect.configChanged.connect(this.loadConfig.bind(this));
        effect.animationEnded.connect(this.cleanupForcedRoles.bind(this));
        effects.windowAdded.connect(this.slotWindowAdded.bind(this));
        effects.windowClosed.connect(this.slotWindowClosed.bind(this));
        effects.windowDataChanged.connect(this.slotWindowDataChanged.bind(this));
        this.loadConfig();
    }

    loadConfig() {
        this.openDuration = animationTime(effect.readConfig("OpenDuration", 250));
        this.fadeInDuration = animationTime(effect.readConfig("FadeInDuration", 83));
        this.closeDuration = animationTime(effect.readConfig("CloseDuration", 167));
        this.inScale = effect.readConfig("InScale", 0.94);
        this.outScale = effect.readConfig("OutScale", 0.96);
    }

    // animations off (or turned down to nothing)
    get off() {
        return this.openDuration < 2 || this.closeDuration < 2;
    }

    static isAnimated(window) {
        // plasmashell's own surfaces animate themselves; only its windows
        // with a title bar (a settings dialog) are ours
        if (window.windowClass == "plasmashell plasmashell"
                || window.windowClass == "plasmashell org.kde.plasmashell") {
            return window.hasDecoration;
        }
        const tags = blacklist[window.windowClass];
        if (tags && (tags.length === 0 || tags.includes(window.tag))) {
            return false;
        }
        if (window.hasDecoration) {
            return true;
        }
        if (window.popupWindow || window.lockScreen || window.outline) {
            return false;
        }
        if (!window.managed) {
            return false;
        }
        return !!(window.normalWindow || window.dialog);
    }

    // a window is not blurred while KWin transforms it, unless told to
    setupForcedRoles(window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, true);
        window.setData(Effect.WindowForceBlurRole, true);
    }

    cleanupForcedRoles(window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, null);
        window.setData(Effect.WindowForceBlurRole, null);
    }

    slotWindowAdded(window) {
        if (this.off || effects.hasActiveFullScreenEffect || !window.visible) {
            return;
        }
        if (!RayeWindows.isAnimated(window) || effect.isGrabbed(window, Effect.WindowAddedGrabRole)) {
            return;
        }
        this.setupForcedRoles(window);
        window.rayeOpenAnimation = animate({
            window: window,
            duration: this.openDuration,
            animations: [{
                type: Effect.Scale,
                from: this.inScale,
                curve: QEasingCurve.OutQuint
            }, {
                type: Effect.Opacity,
                from: 0,
                duration: this.fadeInDuration,
                curve: QEasingCurve.Linear
            }]
        });
    }

    slotWindowClosed(window) {
        if (this.off || effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!RayeWindows.isAnimated(window) || !window.visible || window.skipsCloseAnimation) {
            return;
        }
        if (effect.isGrabbed(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        if (window.rayeOpenAnimation) {
            cancel(window.rayeOpenAnimation);
            delete window.rayeOpenAnimation;
        }
        this.setupForcedRoles(window);
        window.rayeCloseAnimation = animate({
            window: window,
            duration: this.closeDuration,
            animations: [{
                type: Effect.Scale,
                to: this.outScale,
                curve: QEasingCurve.InCubic
            }, {
                type: Effect.Opacity,
                to: 0,
                curve: QEasingCurve.Linear
            }]
        });
    }

    // another effect took the window over: stop ours
    slotWindowDataChanged(window, role) {
        if (role == Effect.WindowAddedGrabRole) {
            if (window.rayeOpenAnimation && effect.isGrabbed(window, role)) {
                cancel(window.rayeOpenAnimation);
                delete window.rayeOpenAnimation;
                this.cleanupForcedRoles(window);
            }
        } else if (role == Effect.WindowClosedGrabRole) {
            if (window.rayeCloseAnimation && effect.isGrabbed(window, role)) {
                cancel(window.rayeCloseAnimation);
                delete window.rayeCloseAnimation;
                this.cleanupForcedRoles(window);
            }
        }
    }
}

new RayeWindows();
