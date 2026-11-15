/*
    Raye popups: menus and tooltips fade in with a short slide from the
    side they open on, and close at once, the way Windows 11 does it.

    Based on KWin's "Fading popups" effect.
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Raye Linux

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

// Windows 11 motion: popups come in over 167 ms on a quick-out curve
// and go away in 83 ms. The slide is 6 px.
var FADE_IN_MS = 167;
var FADE_OUT_MS = 83;
var SLIDE_PX = 6;

var blacklist = [
    // ignore black background behind lockscreen
    "ksmserver ksmserver",
    // The logout screen has to be animated only by the logout effect.
    "ksmserver-logout-greeter ksmserver-logout-greeter",
    // The lockscreen isn't a popup window
    "kscreenlocker_greet kscreenlocker_greet",
    // KDE Plasma splash screen has to be animated only by the login effect.
    "ksplashqml ksplashqml",
];

function isPopupWindow(window) {
    if (blacklist.indexOf(window.windowClass) != -1) {
        return false;
    }
    // combo box popups, tooltips, popup menus
    if (window.popupWindow) {
        return true;
    }
    if (window.outline) {
        return true;
    }
    // Override-redirect windows: popups of X11 programs. Utility windows
    // like a browser's address bar list are left alone.
    if (!window.managed) {
        return !window.utility;
    }
    // Other short-lived windows fade, without the slide.
    if (window.splash || window.toolbar
            || window.notification || window.onScreenDisplay
            || window.criticalNotification
            || window.appletPopup) {
        return true;
    }
    return false;
}

// Only menus and tooltips slide; notifications, OSDs and the rest just fade.
function slides(window) {
    return !!(window.popupMenu || window.dropdownMenu || window.tooltip
              || window.comboBox);
}

// Where a popup comes from: a menu that opened below the pointer slides
// down into place, one that opened above it slides up. Returns the
// offset the popup starts at, {value1: x, value2: y} in logical pixels,
// or null when it does not slide.
function slideOffset(window, cursor) {
    if (!slides(window) || !cursor) {
        return null;
    }
    var g = window.frameGeometry || window.geometry;
    if (!g || !(g.height > 0)) {
        return null;
    }
    var middle = g.y + g.height / 2;
    return {value1: 0, value2: cursor.y <= middle ? -SLIDE_PX : SLIDE_PX};
}

var rayePopupsEffect = {
    loadConfig: function () {
        rayePopupsEffect.fadeInDuration = animationTime(FADE_IN_MS);
        rayePopupsEffect.fadeOutDuration = animationTime(FADE_OUT_MS);
    },
    slotWindowAdded: function (window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!isPopupWindow(window) || !window.visible) {
            return;
        }
        if (!effect.grab(window, Effect.WindowAddedGrabRole)) {
            return;
        }
        var animations = [{
            type: Effect.Opacity,
            curve: QEasingCurve.OutQuint,
            from: 0.0,
            to: 1.0
        }];
        var offset = slideOffset(window, effects.cursorPos);
        if (offset) {
            animations.push({
                type: Effect.Translation,
                curve: QEasingCurve.OutQuint,
                from: offset,
                to: {value1: 0, value2: 0}
            });
        }
        window.rayePopupInAnimation = animate({
            window: window,
            duration: rayePopupsEffect.fadeInDuration,
            animations: animations
        });
    },
    slotWindowClosed: function (window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!isPopupWindow(window)) {
            return;
        }
        if (!window.visible || window.skipsCloseAnimation) {
            return;
        }
        if (!effect.grab(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        window.rayePopupOutAnimation = animate({
            window: window,
            curve: QEasingCurve.Linear,
            duration: rayePopupsEffect.fadeOutDuration,
            type: Effect.Opacity,
            from: 1.0,
            to: 0.0
        });
    },
    slotWindowDataChanged: function (window, role) {
        // another effect took the window over: stop ours
        if (role == Effect.WindowAddedGrabRole) {
            if (window.rayePopupInAnimation && effect.isGrabbed(window, role)) {
                cancel(window.rayePopupInAnimation);
                delete window.rayePopupInAnimation;
            }
        } else if (role == Effect.WindowClosedGrabRole) {
            if (window.rayePopupOutAnimation && effect.isGrabbed(window, role)) {
                cancel(window.rayePopupOutAnimation);
                delete window.rayePopupOutAnimation;
            }
        }
    },
    init: function () {
        rayePopupsEffect.loadConfig();
        effect.configChanged.connect(rayePopupsEffect.loadConfig);
        effects.windowAdded.connect(rayePopupsEffect.slotWindowAdded);
        effects.windowClosed.connect(rayePopupsEffect.slotWindowClosed);
        effects.windowDataChanged.connect(rayePopupsEffect.slotWindowDataChanged);
    }
};

rayePopupsEffect.init();
