// org.raye.ui: sizes from the design rules, colours from the tokens,
// keyboard use, focus rings and the names screen readers get.
//
//   qmltestrunner -input tests/qml -import <dir holding org/raye/ui>
import QtQuick
import QtTest
import org.raye.ui

Item {
    id: root
    width: 800
    height: 600

    Component { id: accentButton; AccentButton { text: "Restart now" } }
    Component { id: plainButton; AccentButton { text: "Cancel"; accent: false } }
    Component { id: switchComp; Switch { text: "Bluetooth" } }
    Component { id: cardComp; Card { Item { implicitWidth: 100; implicitHeight: 40 } } }
    Component { id: settingsCard; SettingsCard { title: "Dark mode"; description: "Use dark colours" } }
    Component {
        id: cardWithSwitch
        SettingsCard { title: "Bluetooth"; Switch { objectName: "trailingSwitch" } }
    }
    Component { id: clickableCard; SettingsCard { title: "Display"; clickable: true } }
    Component { id: infoBar; InfoBar { title: "Restart needed"; message: "Updates are ready" } }
    Component { id: pillComp; Pill {} }
    Component { id: acrylicComp; Acrylic { width: 200; height: 120 } }
    Component { id: sourceItem; Rectangle { width: 400; height: 300; color: "#c040ff" } }

    TestCase {
        name: "RayeUi"
        when: windowShown

        function init() {
            Tokens.forceMode = ""
        }

        function cleanup() {
            Tokens.forceMode = ""
        }

        function near(a, b, what) {
            verify(Qt.colorEqual(a, b), what + ": " + a + " is not " + b)
        }

        // colours change with a short animation
        function soon(get, want, what) {
            tryVerify(() => Qt.colorEqual(get(), want), 2000, what + ": " + get() + " is not " + want)
        }

        // --- AccentButton -------------------------------------------------

        function test_button_size() {
            const b = createTemporaryObject(accentButton, root)
            compare(b.height, 32)
            verify(b.width >= 64)
            compare(b.background.radius, Tokens.radiusControl)
            compare(b.font.family, "Inter")
        }

        function test_button_colours_follow_the_accent() {
            const b = createTemporaryObject(accentButton, root)
            near(b.background.color, Tokens.accent, "rest")
            near(b.contentItem.children[1].color, Tokens.textOnAccent, "text")
            b.enabled = false
            soon(() => b.background.color, Tokens.accentDisabled, "disabled")
        }

        function test_standard_button() {
            const b = createTemporaryObject(plainButton, root)
            near(b.background.color, Tokens.fillControl, "rest")
            compare(b.background.border.width, 1)
            near(b.background.border.color, Tokens.strokeControl, "stroke")
            near(b.contentItem.children[1].color, Tokens.textPrimary, "text")
        }

        function test_button_keyboard() {
            const b = createTemporaryObject(accentButton, root)
            const spy = createTemporaryObject(spyComp, root, { target: b, signalName: "clicked" })
            b.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Space)
            keyClick(Qt.Key_Return)
            compare(spy.count, 2)
            compare(b.Accessible.name, "Restart now")
            compare(b.Accessible.role, Accessible.Button)
        }

        // --- the focus ring: keyboard only --------------------------------

        function ring(item) {
            // the FocusRing is the last child of the control's background
            const kids = item.background.children
            return kids[kids.length - 1]
        }

        function test_focus_ring_only_for_the_keyboard() {
            const b = createTemporaryObject(accentButton, root, { x: 20, y: 20 })
            const r = ring(b)
            verify(!r.visible)
            mouseClick(b)
            verify(b.activeFocus)
            verify(!r.visible, "a click shows no ring")
            root.forceActiveFocus()
            verify(!b.activeFocus)
            b.forceActiveFocus(Qt.TabFocusReason)
            verify(r.visible, "Tab shows the ring")
            // 2 + 1 px outside the control, corners 3 px rounder
            compare(r.anchors.margins, -3)
            compare(r.children[1].radius, Tokens.radiusControl + 3)
            compare(r.children[1].border.width, 2)
            near(r.children[1].border.color, Tokens.focusOuter, "outer ring")
            compare(r.children[2].border.width, 1)
            // the glow only at night
            Tokens.forceMode = "night"
            verify(r.children[0].visible)
            Tokens.forceMode = "day"
            verify(!r.children[0].visible)
        }

        // --- Switch -------------------------------------------------------

        function test_switch_sizes() {
            const s = createTemporaryObject(switchComp, root)
            compare(s.indicator.width, 40)
            compare(s.indicator.height, 20)
            compare(s.indicator.radius, 10)
            const knob = s.indicator.children[0]
            compare(knob.width, 12)
            compare(knob.height, 12)
            compare(s.contentItem.text, "Off")
            // the state text sits 12 px after the track
            compare(s.contentItem.leftPadding, 40 + 12)
        }

        function test_switch_keyboard_and_state() {
            const s = createTemporaryObject(switchComp, root)
            s.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Space)
            verify(s.checked)
            compare(s.contentItem.text, "On")
            soon(() => s.indicator.color, Tokens.accent, "on track")
            compare(s.indicator.border.width, 0)
            near(s.indicator.children[0].color, Tokens.textOnAccent, "on knob")
            keyClick(Qt.Key_Space)
            verify(!s.checked)
            compare(s.indicator.border.width, 1)
            near(s.indicator.border.color, Tokens.strokeStrong, "off stroke")
            compare(s.Accessible.name, "Bluetooth")
            verify(s.Accessible.checkable)
        }

        function test_switch_knob_grows_under_the_pointer() {
            const s = createTemporaryObject(switchComp, root, { x: 10, y: 10 })
            const knob = s.indicator.children[0]
            mouseMove(s, 20, 16)
            tryCompare(s, "hovered", true)
            compare(s.knobWidth, 14)
            mousePress(s, 20, 16)
            compare(s.knobWidth, 17)
            compare(s.knobHeight, 14)
            mouseRelease(s, 20, 16)
            verify(s.checked)
        }

        function test_switch_without_state_text() {
            const s = createTemporaryObject(switchComp, root, { showStateText: false })
            compare(s.implicitWidth, 40)
            verify(!s.contentItem.visible)
        }

        // --- cards --------------------------------------------------------

        function test_card() {
            const c = createTemporaryObject(cardComp, root)
            compare(c.background.radius, 8)
            near(c.background.color, Tokens.bgCard, "card")
            near(c.background.border.color, Tokens.strokeCard, "stroke")
            compare(c.width, 100 + 32)
            compare(c.height, 40 + 32)
            compare(c.Accessible.role, Accessible.Pane)
        }

        function test_settings_card() {
            const c = createTemporaryObject(settingsCard, root, { width: 600 })
            verify(c.height >= 68, "with a description: 68 px at least")
            compare(c.Accessible.name, "Dark mode")
            compare(c.Accessible.description, "Use dark colours")
            compare(c.focusPolicy, Qt.NoFocus)
            compare(c.background.radius, Tokens.radiusControl)
            c.description = ""
            tryCompare(c, "height", 48)
        }

        function test_settings_card_holds_a_switch() {
            const c = createTemporaryObject(cardWithSwitch, root, { width: 600 })
            const s = findChild(c, "trailingSwitch")
            verify(s)
            // right edge: 16 px from the card's right side, no chevron
            const p = s.mapToItem(c, s.width, 0)
            compare(Math.round(c.width - p.x), 16)
            verify(!c.contentItem.children[3].visible)
        }

        function test_clickable_card() {
            const c = createTemporaryObject(clickableCard, root, { width: 600 })
            const spy = createTemporaryObject(spyComp, root, { target: c, signalName: "clicked" })
            c.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Return)
            keyClick(Qt.Key_Space)
            compare(spy.count, 2)
            compare(c.Accessible.role, Accessible.Button)
            verify(c.contentItem.children[3].visible, "a chevron when it holds nothing")
        }

        // --- InfoBar ------------------------------------------------------

        function test_infobar() {
            const bar = createTemporaryObject(infoBar, root, { width: 700 })
            verify(bar.height >= 48)
            compare(bar.background.radius, 4)
            compare(bar.Accessible.role, Accessible.AlertMessage)
            compare(bar.Accessible.name, "Restart needed")
            near(bar.signColor, Tokens.accent, "information")
            bar.severity = InfoBar.Success
            near(bar.signColor, Tokens.statusSuccess, "success")
            bar.severity = InfoBar.Warning
            near(bar.signColor, Tokens.statusCaution, "warning")
            bar.severity = InfoBar.Error
            near(bar.signColor, Tokens.statusCritical, "error")
        }

        function test_infobar_close() {
            const bar = createTemporaryObject(infoBar, root, { width: 700 })
            const spy = createTemporaryObject(spyComp, root, { target: bar, signalName: "closeClicked" })
            const close = bar.contentItem.children[3]
            compare(close.Accessible.name, "Close")
            close.forceActiveFocus(Qt.TabFocusReason)
            keyClick(Qt.Key_Space)
            compare(spy.count, 1)
            bar.closable = false
            verify(!close.visible)
        }

        // --- Pill, Acrylic --------------------------------------------------

        function test_pill() {
            const p = createTemporaryObject(pillComp, root)
            compare(p.width, 3)
            tryCompare(p, "height", 16)
            compare(p.radius, 1.5)
            near(p.color, Tokens.accent, "pill")
            p.active = false
            tryCompare(p, "height", 0)
        }

        function test_acrylic() {
            const back = createTemporaryObject(sourceItem, root)
            const a = createTemporaryObject(acrylicComp, root, { backdrop: back, x: 20, y: 20 })
            const tint = findChild(a, "acrylicTint")
            verify(tint)
            near(tint.color, Tokens.acrylicTint, "tint over a backdrop")
            compare(tint.radius, 8)
            near(tint.border.color, Tokens.strokeSurface, "outline")
            a.backdrop = null
            near(tint.color, Tokens.acrylicTintSolid, "no backdrop: solid")
        }

        // --- Day and Night --------------------------------------------------

        function test_day_repaints_everything() {
            const c = createTemporaryObject(cardComp, root)
            const s = createTemporaryObject(switchComp, root)
            Tokens.forceMode = "night"
            const nightCard = c.background.color.toString()  // a copy, not a live value
            const nightText = s.contentItem.color.toString()
            Tokens.forceMode = "day"
            soon(() => c.background.color, Tokens.bgCard, "day card")
            verify(!Qt.colorEqual(c.background.color, nightCard))
            verify(!Qt.colorEqual(s.contentItem.color, nightText))
            near(s.contentItem.color, Tokens.textPrimary, "day text")
        }
    }

    Component { id: spyComp; SignalSpy {} }

    // MicaWindow needs the module's plugin (WindowEffects), which is
    // only there in the built module.
    TestCase {
        name: "RayeUiWindow"
        when: windowShown

        function effects() {
            try {
                return Qt.createQmlObject("import org.raye.ui.effects; WindowEffects {}", root)
            } catch (e) {
                return null
            }
        }

        function test_window_effects() {
            const fx = effects()
            if (!fx)
                skip("plugin not loaded")
            // no Raye key in kdeglobals: transparency is on
            verify(fx.transparencyEffects)
            // offscreen: no compositor, so no blur
            verify(!fx.blurAvailable)
            fx.destroy()
        }

        function test_mica_window_is_opaque_without_blur() {
            if (!effects())
                skip("plugin not loaded")
            const w = Qt.createQmlObject("import org.raye.ui; MicaWindow { width: 300; height: 200; visible: true }", root)
            verify(w)
            verify(!w.mica)
            verify(Qt.colorEqual(w.color, Tokens.bgBase), "opaque window: " + w.color)
            w.destroy()
        }
    }
}
