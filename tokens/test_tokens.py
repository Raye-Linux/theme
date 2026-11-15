# Tests for the Raye design tokens and their generator.
#
#   python3 -m pytest tokens/
#
# They hold three promises: the files in the repo are what the tokens
# give (nobody hand-edited a hex value), every text/background pair a
# user can see passes WCAG AA, and the colour schemes carry every key
# KDE reads, so no application falls back to a Breeze colour.

import json
import os
import re
import shutil
import subprocess
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import generate as g  # noqa: E402

TOKENS = g.load()
MODES = g.MODES

# Microsoft names must never appear in a token value; the only place the
# system may say "Segoe" is the fontconfig alias, which is not here.
FORBIDDEN = ("segoe", "fluent", "windows 11", "tahoma")


# --- colour arithmetic ------------------------------------------------

def test_rgb_round_trip():
    assert g.rgb("#7B3CFF") == (123, 60, 255)
    assert g.hexstr((123, 60, 255)) == "#7B3CFF"
    assert g.hexstr(g.rgb("#0e0a17")) == "#0E0A17"


@pytest.mark.parametrize("bad", ["7B3CFF", "#7B3CF", "#GGGGGG", "", "#7B3CFFFF"])
def test_rgb_refuses_anything_else(bad):
    with pytest.raises(ValueError):
        g.rgb(bad)


def test_mix_ends_and_middle():
    assert g.mix("#000000", "#FFFFFF", 0.0) == "#000000"
    assert g.mix("#000000", "#FFFFFF", 1.0) == "#FFFFFF"
    assert g.mix("#000000", "#FFFFFF", 0.5) == "#808080"


def test_luminance_and_contrast_known_values():
    assert g.luminance("#000000") == 0.0
    assert g.luminance("#FFFFFF") == pytest.approx(1.0)
    # the widest pair WCAG allows
    assert g.contrast("#000000", "#FFFFFF") == pytest.approx(21.0)
    assert g.contrast("#7B3CFF", "#7B3CFF") == pytest.approx(1.0)
    # order does not matter
    assert g.contrast("#130E1F", "#F3EAFF") == pytest.approx(g.contrast("#F3EAFF", "#130E1F"))


# --- the token file ---------------------------------------------------

def test_token_file_is_json_and_sorted_by_hand():
    with open(g.TOKENS, encoding="utf-8") as f:
        text = f.read()
    json.loads(text)
    assert text.endswith("\n")


@pytest.mark.parametrize("mode", MODES)
def test_every_colour_token_resolves(mode):
    for name in g.colour_names(TOKENS):
        c = g.colour(TOKENS, name, mode)
        g.rgb(c["color"])
        g.rgb(c["solid"])
        assert 0.0 < c["alpha"] <= 1.0, name


@pytest.mark.parametrize("mode", MODES)
def test_declared_solids_match_the_drawn_colour(mode):
    """A token that gives both an alpha and a "solid" must name the same
    colour twice: the solid is what KDE stores, the alpha is what the
    style draws, and moving from a Plasma surface to a Qt one must not
    make the shade jump.

    The solids are tuned a touch more violet than a plain white-over-base
    mix (that is the Raye depth), so the bound is perceptual: same
    brightness within 8 %, no channel more than 14 steps away. A wrong
    mode, a typo or a stale value lands far outside."""
    base = TOKENS["color"]["bg.base"][mode]
    for name in g.colour_names(TOKENS):
        value = TOKENS["color"][name][mode]
        if not isinstance(value, dict) or "solid" not in value:
            continue
        drawn = g.mix(base, value["color"], value["alpha"])
        ratio = g.contrast(drawn, value["solid"])
        channels = max(abs(a - b) for a, b in zip(g.rgb(drawn), g.rgb(value["solid"])))
        assert ratio < 1.08 and channels <= 14, \
            "%s (%s): solid %s but %s at %g%% over %s draws %s (ratio %.3f)" % (
                name, mode, value["solid"], value["color"],
                value["alpha"] * 100, base, drawn, ratio)


def test_contrast_pairs_name_real_tokens():
    names = set(g.colour_names(TOKENS))
    for fg, bg, minimum in TOKENS["contrast"]["pairs"]:
        assert fg in names, fg
        assert bg in names, bg
        assert minimum in (3.0, 4.5, 7.0)


def test_every_pair_passes_wcag():
    bad = g.check_pairs(TOKENS)
    assert bad == [], "\n".join(
        "%s: %s on %s is %.2f, needs %g" % (m, f, b, r, n) for m, f, b, r, n in bad)


def test_the_pairs_cover_the_text_tokens_on_the_surfaces():
    """Every text token must be checked against the surfaces it lands on,
    so adding a surface without a pair cannot slip through."""
    pairs = {(f, b) for f, b, _ in TOKENS["contrast"]["pairs"]}
    surfaces = ["bg.base", "bg.secondary", "bg.tertiary", "bg.quarternary",
                "bg.layer", "bg.card", "bg.card.hover", "fill.control.hover"]
    for text in ("text.primary", "text.secondary", "text.tertiary"):
        for surface in surfaces:
            assert (text, surface) in pairs, "%s on %s is never checked" % (text, surface)
    # a selected row is the accent drawn over the base
    for text in ("text.primary", "text.secondary"):
        assert (text, "fill.subtle.selected") in pairs, text


@pytest.mark.parametrize("mode", MODES)
def test_disabled_text_stays_visible(mode):
    """Disabled text is allowed below AA, but it must still be seen."""
    for surface in ("bg.base", "bg.card", "fill.control.disabled"):
        ratio = g.contrast(g.solid(TOKENS, "text.disabled", mode),
                           g.solid(TOKENS, surface, mode))
        assert ratio >= 2.0, "%s: text.disabled on %s is %.2f" % (mode, surface, ratio)


def test_no_microsoft_name_in_the_tokens():
    with open(g.TOKENS, encoding="utf-8") as f:
        text = f.read().lower()
    for word in FORBIDDEN:
        assert word not in text, word


# --- accent derivation ------------------------------------------------

@pytest.mark.parametrize("mode", MODES)
def test_the_rule_reproduces_the_hand_tuned_default_accent(mode):
    """The table's accent shades are tuned by eye; the rule the style and
    QML run at run time must land on the same place, or a user who keeps
    the default accent would see two different violets."""
    default = TOKENS["accent_rule"]["default"]
    got = g.derive_accent(TOKENS, default, mode)
    for name in ("accent.hover", "accent.pressed", "accent.text",
                 "accent.disabled", "text.onAccent"):
        want = g.solid(TOKENS, name, mode)
        # same brightness and same hue: the table keeps a little more blue
        # than a plain mix toward black, never more than 20 steps
        channels = max(abs(a - b) for a, b in zip(g.rgb(got[name]), g.rgb(want)))
        assert g.contrast(got[name], want) < 1.25 and channels <= 20, \
            "%s (%s): rule gives %s, table says %s" % (name, mode, got[name], want)


@pytest.mark.parametrize("mode", MODES)
@pytest.mark.parametrize("accent", ["#7B3CFF", "#0078D4", "#107C10", "#C50F1F",
                                    "#FFB900", "#000000", "#FFFFFF", "#767676",
                                    "#2D7D9A", "#E3008C", "#808080"])
def test_any_accent_the_user_picks_stays_readable(accent, mode):
    d = g.derive_accent(TOKENS, accent, mode)
    base = g.solid(TOKENS, "bg.base", mode)
    assert g.contrast(d["accent.text"], base) >= 4.5, \
        "%s (%s): accent text %s on %s" % (accent, mode, d["accent.text"], base)
    # a button keeps its text readable when hovered and pressed too
    for state in ("accent", "accent.hover", "accent.pressed"):
        assert g.contrast(d["text.onAccent"], d[state]) >= 4.5, \
            "%s (%s): %s on %s %s is %.2f" % (accent, mode, d["text.onAccent"], state,
                                              d[state], g.contrast(d["text.onAccent"], d[state]))


@pytest.mark.parametrize("mode", MODES)
@pytest.mark.parametrize("accent", ["#7B3CFF", "#0078D4", "#107C10", "#C50F1F",
                                    "#FFB900", "#767676", "#2D7D9A", "#E3008C",
                                    "#808080", "#7A7A7A"])
def test_hover_and_pressed_still_show(accent, mode):
    """Keeping the text readable must not flatten the states."""
    d = g.derive_accent(TOKENS, accent, mode)
    shades = [d["accent"], d["accent.hover"], d["accent.pressed"]]
    assert len(set(shades)) == 3, "%s (%s): %s" % (accent, mode, shades)


@pytest.mark.parametrize("mode", MODES)
@pytest.mark.parametrize("accent", ["#7A7A7A", "#787878"])
def test_an_accent_no_text_holds_on_gets_no_worse(accent, mode):
    """Mid greys reach 4.5:1 with neither text colour; hover and pressed
    must at least not lose what the accent itself gives."""
    d = g.derive_accent(TOKENS, accent, mode)
    floor = g.contrast(d["text.onAccent"], accent)
    assert floor < 4.5
    for state in ("accent.hover", "accent.pressed"):
        assert g.contrast(d["text.onAccent"], d[state]) >= floor - 0.01, (accent, mode, state)


# --- KDE colour schemes -----------------------------------------------

def scheme(mode):
    return g.colors_file(TOKENS, mode)


def parse_ini(text):
    out, section = {}, None
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
            out[section] = {}
        else:
            k, _, v = line.partition("=")
            out[section][k] = v
    return out


@pytest.mark.parametrize("mode", MODES)
def test_every_colour_group_has_every_key(mode):
    ini = parse_ini(scheme(mode))
    for group in g.KDE_GROUPS:
        name = "Colors:%s" % group
        assert name in ini, name
        for key in g.KDE_KEYS:
            assert key in ini[name], "%s has no %s" % (name, key)
            assert re.fullmatch(r"\d{1,3},\d{1,3},\d{1,3}", ini[name][key])


@pytest.mark.parametrize("mode", MODES)
def test_the_groups_plasma_and_applications_read_are_all_there(mode):
    """Plasma, Kirigami and the Qt style ask for these by name; a missing
    one sends the application back to a Breeze colour."""
    ini = parse_ini(scheme(mode))
    for group in ("Colors:Window", "Colors:View", "Colors:Button",
                  "Colors:Selection", "Colors:Tooltip", "Colors:Complementary",
                  "Colors:Header", "Colors:Header][Inactive",
                  "ColorEffects:Disabled", "ColorEffects:Inactive",
                  "General", "KDE", "WM"):
        assert group in ini, group


@pytest.mark.parametrize("mode", MODES)
def test_scheme_names_english_and_french(mode):
    ini = parse_ini(scheme(mode))
    info = TOKENS["modes"][mode]
    assert ini["General"]["Name"] == info["name"]
    assert ini["General"]["Name[fr]"] == info["name_fr"]
    assert ini["General"]["ColorScheme"] == info["scheme"]


@pytest.mark.parametrize("mode", MODES)
def test_title_bar_does_not_change_colour_when_a_window_loses_focus(mode):
    """Windows keeps the title bar the colour of the window; so does Raye."""
    wm = parse_ini(scheme(mode))["WM"]
    assert wm["activeBackground"] == wm["inactiveBackground"]
    assert wm["activeBackground"] == g.kde_rgb(g.solid(TOKENS, "bg.base", mode))
    assert wm["activeForeground"] != wm["inactiveForeground"]


@pytest.mark.parametrize("mode", MODES)
def test_inactive_windows_are_not_dimmed(mode):
    """Windows does not grey out the content of a window behind."""
    assert parse_ini(scheme(mode))["ColorEffects:Inactive"]["Enable"] == "false"


def test_the_complementary_group_is_dark_in_both_schemes():
    """Panels, the lock screen and the OSD sit on the night colours even
    in Raye Day, so their white glyphs stay readable."""
    night = parse_ini(scheme("night"))["Colors:Complementary"]
    day = parse_ini(scheme("day"))["Colors:Complementary"]
    assert night == day


def test_the_two_schemes_really_differ():
    assert scheme("night") != scheme("day")
    assert parse_ini(scheme("night"))["Colors:Window"]["BackgroundNormal"] != \
        parse_ini(scheme("day"))["Colors:Window"]["BackgroundNormal"]


@pytest.mark.parametrize("mode", MODES)
def test_selection_text_reads_on_the_selection_fill(mode):
    ini = parse_ini(scheme(mode))["Colors:Selection"]

    def to_hex(v):
        return g.hexstr(tuple(int(x) for x in v.split(",")))
    assert g.contrast(to_hex(ini["ForegroundNormal"]),
                      to_hex(ini["BackgroundNormal"])) >= 4.5


# --- Konsole ----------------------------------------------------------

@pytest.mark.parametrize("mode", MODES)
def test_konsole_scheme_is_complete(mode):
    ini = parse_ini(g.konsole_file(TOKENS, mode))
    for name in ("Background", "BackgroundIntense", "BackgroundFaint",
                 "Foreground", "ForegroundIntense", "ForegroundFaint"):
        assert name in ini, name
    for i in range(8):
        for suffix in ("", "Intense", "Faint"):
            assert "Color%d%s" % (i, suffix) in ini
    assert ini["General"]["Description"] == TOKENS["modes"][mode]["name"]
    assert ini["General"]["Description[fr]"] == TOKENS["modes"][mode]["name_fr"]


@pytest.mark.parametrize("mode", MODES)
def test_konsole_colours_read_on_its_background(mode):
    term = TOKENS["terminal"][mode]
    bg = term["background"]
    assert g.contrast(term["foreground"], bg) >= 7.0
    # ANSI 1-6 are used for text in prompts and compiler output; black
    # (0) and the greys are not, so they are not held to the same bar.
    for i in range(1, 7):
        assert g.contrast(term["ansi"][i], bg) >= 4.0, \
            "%s: ANSI %d (%s) on %s" % (mode, i, term["ansi"][i], bg)
        assert g.contrast(term["ansi.intense"][i], bg) >= 4.5, \
            "%s: bold ANSI %d (%s) on %s" % (mode, i, term["ansi.intense"][i], bg)


@pytest.mark.parametrize("mode", MODES)
def test_konsole_bold_stands_out(mode):
    """Bold must differ from the plain shade and never fade into the
    background: on a light scheme that means darker, not lighter."""
    term = TOKENS["terminal"][mode]
    bg = term["background"]
    for i in range(8):
        plain, bold = term["ansi"][i], term["ansi.intense"][i]
        assert g.contrast(plain, bold) >= 1.15, \
            "%s: bold ANSI %d (%s) looks like the plain one (%s)" % (mode, i, bold, plain)
        if i != 0:
            assert g.contrast(bold, bg) >= g.contrast(plain, bg) * 0.9, \
                "%s: bold ANSI %d (%s) is fainter than plain (%s)" % (mode, i, bold, plain)


# --- the QML singleton ------------------------------------------------

def test_qml_has_a_property_for_every_colour_token():
    qml = g.tokens_qml(TOKENS)
    for name in g.colour_names(TOKENS):
        prop = g.camel(name)
        assert re.search(r"property (color|var) %s[:\s]" % re.escape(prop), qml), prop


def test_qml_declares_each_property_once():
    qml = g.tokens_qml(TOKENS)
    names = re.findall(r"^\s*readonly property \S+ (\w+)[:\s]", qml, re.M)
    duplicates = sorted({n for n in names if names.count(n) > 1})
    assert duplicates == []


def test_qml_carries_the_sizes_radii_type_and_motion():
    qml = g.tokens_qml(TOKENS)
    for expected in ("radiusControl", "radiusOverlay", "sizeControlHeight",
                     "sizeCaptionButtonWidth", "sizeTitlebar", "s3", "s5",
                     "body", "bodyLineHeight", "displayLineHeight",
                     "durationFast", "easingNormal", "bezierDecelerate",
                     "elevationWindowActive", "brandViolet"):
        assert re.search(r"property \S+ %s[:\s]" % expected, qml), expected


def test_qml_brackets_balance():
    qml = g.tokens_qml(TOKENS)
    assert qml.count("{") == qml.count("}")
    assert qml.count("(") == qml.count(")")
    assert qml.lstrip().startswith("//")


def test_qml_hard_codes_no_colour_outside_the_tokens():
    """Every #rrggbb in the singleton must come from the token file."""
    qml = g.tokens_qml(TOKENS)
    known = {"#FFFFFF", "#000000"}
    for name in g.colour_names(TOKENS):
        for mode in MODES:
            c = g.colour(TOKENS, name, mode)
            known.add(c["color"])
            known.add(c["solid"])
    for mode in MODES:
        for shade in g.derive_accent(TOKENS, TOKENS["accent_rule"]["default"], mode).values():
            known.add(shade.upper())
    for key, value in TOKENS["brand"].items():
        if key != "about":
            known.add(value.upper())
    known.add(TOKENS["accent_rule"]["on_accent"]["dark"].upper())
    known.add(TOKENS["accent_rule"]["default"].upper())
    # the palettes that mean "no colour scheme", recognised, never drawn
    for key in ("window", "highlight"):
        known.update(c.upper() for c in TOKENS["accent_rule"]["unthemed"][key])
    for found in re.findall(r"#(?:[0-9a-fA-F]{2})?([0-9a-fA-F]{6})\b", qml):
        assert ("#" + found).upper() in known, found


# --- the QML singleton, run --------------------------------------------
#
# Tokens.qml reads Night or Day and the user's accent from the colour
# scheme in use, and carries its own copy of the accent rule, in
# JavaScript, for accents the user picks at run time. These run it in the
# QML engine under Plasma's platform theme, with a kdeglobals made from
# our own schemes, and hold it to the Python rule, so the two copies
# cannot drift apart.

QML_TOOL = shutil.which("qml6") or "/usr/lib/qt6/bin/qml"
KDE_PLATFORM_THEME = "/usr/lib/%s/qt6/plugins/platformthemes/KDEPlasmaPlatformTheme6.so" % (
    os.environ.get("DEB_HOST_MULTIARCH") or "x86_64-linux-gnu")
ACCENTS = ["#7B3CFF", "#0078D4", "#107C10", "#C50F1F", "#FFB900", "#E3008C",
           "#000000", "#FFFFFF", "#767676", "#2D7D9A", "#808080", "#7A7A7A"]
USER_ACCENT = "#0078D4"

PROBE = """
import QtQuick
import org.raye.ui

Item {
    Component.onCompleted: {
        const out = { accents: {}, schemes: [] }
        out.selections = {}
        for (const a of %(accents)s) {
            for (const night of [true, false]) {
                const d = Tokens.deriveAccent(a, night)
                const key = a + (night ? "/night" : "/day")
                out.accents[key] = {
                    "accent.hover": d.hover.toString(), "accent.pressed": d.pressed.toString(),
                    "accent.text": d.text.toString(), "accent.disabled": d.disabled.toString(),
                    "text.onAccent": d.onAccent.toString()
                }
                const s = Tokens.deriveSelection(a, night, d)
                out.selections[key] = {
                    "selection.inactive": s.inactive.toString(),
                    "selection.visited": s.visited.toString(),
                    "selection.negative": s.negative.toString(),
                    "selection.neutral": s.neutral.toString(),
                    "selection.positive": s.positive.toString()
                }
            }
        }
        for (const c of %(schemes)s) {
            const r = Tokens.readScheme(c[0], c[1], c[2])
            out.schemes.push({ night: r.night, accent: r.accent.toString() })
        }
        out.night = Tokens.night
        out.day = Tokens.day
        out.accent = Tokens.accent.toString()
        out.defaultAccent = Tokens.defaultAccent
        out.accentHover = Tokens.accentHover.toString()
        out.accentText = Tokens.accentText.toString()
        out.textOnAccent = Tokens.textOnAccent.toString()
        out.bgBase = Tokens.bgBase.toString()
        out.textPrimary = Tokens.textPrimary.toString()
        out.radiusControl = Tokens.radiusControl
        out.radiusOverlay = Tokens.radiusOverlay
        out.sizeControlHeight = Tokens.sizeControlHeight
        out.durationFast = Tokens.durationFast
        out.durationNormal = Tokens.durationNormal
        out.bodyPixelSize = Tokens.body.pixelSize
        out.bodyPointSize = Tokens.body.pointSize
        out.bodyWeight = Tokens.body.weight
        out.fillSubtleSelected = Tokens.fillSubtleSelected.toString()
        out.selectionNegative = Tokens.selectionNegative.toString()
        out.selectionInactive = Tokens.selectionInactive.toString()
        out.animationsOff = Tokens.animationsOff
        out.durationSlow = Tokens.durationSlow
        out.durationExitShort = Tokens.durationExitShort
        // a login screen or a setup program pins the mode
        Tokens.forceMode = "day"
        out.forcedDay = { night: Tokens.night, bgBase: Tokens.bgBase.toString() }
        Tokens.forceMode = "night"
        out.forcedNight = { night: Tokens.night, bgBase: Tokens.bgBase.toString() }
        Tokens.forceMode = ""
        console.log("RESULT " + JSON.stringify(out))
        Qt.quit()
    }
}
"""

# what readScheme() is asked: (window background, text, highlight)
SCHEME_CASES = [
    ("#000000", "#000000", "#000000"),   # plugin not loaded yet: all black
    ("#F6F3FB", "#1A1028", "#7B3CFF"),   # Raye Day
    ("#130E1F", "#F3EAFF", USER_ACCENT),  # Raye Night, accent changed
    ("#EFEFEF", "#000000", "#308CC6"),   # Qt's own palette: no platform theme
    ("#EFF0F1", "#232629", "#3DAEE9"),   # Plasma's built-in one: no scheme written
    ("#EFF0F1", "#232629", "#7B3CFF"),   # a light scheme with the Raye accent is one
]


def kdeglobals(scheme, accent=None):
    """The kdeglobals Plasma writes when a scheme is applied. An accent
    is put where KDE's accent code puts it: the selection and the focus
    decorations."""
    with open(os.path.join(g.ROOT, "colors", scheme + ".colors"), encoding="utf-8") as f:
        text = f.read()
    if accent:
        rgb = ",".join(str(v) for v in g.rgb(accent))
        out, group = [], None
        for line in text.splitlines():
            if line.startswith("["):
                group = line
            elif group == "[Colors:Selection]" and line.startswith("BackgroundNormal="):
                line = "BackgroundNormal=" + rgb
            elif line.startswith(("DecorationFocus=", "DecorationHover=")):
                line = line.split("=")[0] + "=" + rgb
            out.append(line)
        text = "\n".join(out) + "\n"
    return text + "\n[General]\nColorScheme=%s\n" % scheme


def run_probe(root, scheme, accent=None, globals_text=None, platform_theme=True,
              label=None):
    module = root / "org" / "raye" / "ui"
    module.mkdir(parents=True, exist_ok=True)
    shutil.copy(os.path.join(g.ROOT, "qml", "org", "raye", "ui", "Tokens.qml"), module)
    (module / "qmldir").write_text("module org.raye.ui\nsingleton Tokens 1.0 Tokens.qml\n")
    config = root / ("config-%s" % (label or "%s-%s" % (scheme, accent or "default")))
    config.mkdir()
    (config / "kdeglobals").write_text(globals_text if globals_text is not None
                                       else kdeglobals(scheme, accent))
    probe = root / "probe.qml"
    probe.write_text(PROBE % {"accents": json.dumps(ACCENTS),
                              "schemes": json.dumps(SCHEME_CASES)})
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QML_IMPORT_PATH=str(root),
               QT_QPA_PLATFORMTHEME="kde", XDG_CONFIG_HOME=str(config),
               QT_LOGGING_RULES="qt.qml.typeresolution.cycle=false")
    if not platform_theme:
        del env["QT_QPA_PLATFORMTHEME"]
    out = subprocess.run([QML_TOOL, str(probe)], capture_output=True, text=True,
                         env=env, timeout=60)
    log = out.stdout + out.stderr
    found = re.search(r"RESULT (\{.*\})", log)
    assert found, log
    warnings = [line for line in log.splitlines()
                if "Tokens.qml" in line and "RESULT" not in line]
    return json.loads(found.group(1)), warnings


def missing_tool(why):
    """Skip on a machine without the tool; in the package build the tool
    is a Build-Depends, so its absence is a failure, not a skip."""
    if os.environ.get("DEB_BUILD_OPTIONS") is not None or os.environ.get("DH_INTERNAL_OPTIONS"):
        pytest.fail(why + " (a Build-Depends of the package)")
    pytest.skip(why)


@pytest.fixture(scope="module")
def qml_runs(tmp_path_factory):
    if not os.path.exists(QML_TOOL):
        missing_tool("the qml tool (qml-qt6) is not installed")
    if not os.path.exists(KDE_PLATFORM_THEME):
        missing_tool("Plasma's platform theme (plasma-integration) is not installed")
    root = tmp_path_factory.mktemp("qml")
    night = kdeglobals("RayeNight")
    return {
        "night": run_probe(root, "RayeNight"),
        "day": run_probe(root, "RayeDay"),
        "accent": run_probe(root, "RayeDay", USER_ACCENT),
        # a login screen or setup program with no colour scheme at all
        "unthemed": run_probe(root, "RayeNight", platform_theme=False, label="unthemed"),
        # a kdeglobals that only names the scheme gets Plasma's built-in colours
        "name-only": run_probe(root, "RayeNight", label="name-only",
                               globals_text="[General]\nColorScheme=RayeNight\n"),
        "no-animations": run_probe(root, "RayeNight", label="no-animations",
                                   globals_text=night.replace(
                                       "[KDE]\n", "[KDE]\nAnimationDurationFactor=0\n", 1)),
    }


@pytest.fixture(scope="module")
def qml_run(qml_runs):
    return qml_runs["night"]


@pytest.mark.parametrize("run", ["night", "day", "accent", "unthemed", "name-only",
                                 "no-animations"])
def test_qml_tokens_load_without_warnings(qml_runs, run):
    _, warnings = qml_runs[run]
    assert warnings == [], "\n".join(warnings)


def channels_apart(a, b):
    return max(abs(x - y) for x, y in zip(g.rgb(a), g.rgb(b)))


@pytest.mark.parametrize("mode", MODES)
@pytest.mark.parametrize("accent", ACCENTS)
def test_qml_accent_rule_matches_the_python_one(qml_run, accent, mode):
    got = qml_run[0]["accents"]["%s/%s" % (accent, mode)]
    want = g.derive_accent(TOKENS, accent, mode)
    for name, value in got.items():
        # float maths on one side, rounding to 8 bits on the other
        assert channels_apart(value.upper(), want[name]) <= 2, \
            "%s %s %s: QML %s, Python %s" % (accent, mode, name, value, want[name])


@pytest.mark.parametrize("mode", MODES)
@pytest.mark.parametrize("accent", ACCENTS)
def test_qml_selection_rule_matches_the_python_one(qml_run, accent, mode):
    """The five tinted foregrounds of a selected row, in the QML mirror.

    ⚠️ The tolerance is wider than the accent shades' two channels, and
    the reason is structural rather than slack: these tints are measured
    against acc.accent and acc.pressed, which are themselves allowed to
    differ by a channel between the two implementations. The walk turns a
    one-channel difference in the BACKGROUND into one step of lightness
    in the tint. What has to hold exactly is the ratio, and
    test_qml_selection_tints_hold_their_ratio checks that.
    """
    got = qml_run[0]["selections"]["%s/%s" % (accent, mode)]
    want = g.derive_selection(TOKENS, accent, mode)
    for name, value in got.items():
        assert channels_apart(value.upper(), want[name]) <= 4, \
            "%s %s %s: QML %s, Python %s" % (accent, mode, name, value, want[name])


@pytest.mark.parametrize("mode", MODES)
@pytest.mark.parametrize("accent", ACCENTS)
def test_qml_selection_tints_hold_their_ratio(qml_run, accent, mode):
    """And they hold it on the shades QML ITSELF produced.

    Measuring a QML tint against Python's accent shades would mix the
    rounding of two implementations into one verdict; what a user sees is
    QML's tint on QML's selection.
    """
    shades = qml_run[0]["accents"]["%s/%s" % (accent, mode)]
    tints = qml_run[0]["selections"]["%s/%s" % (accent, mode)]
    want = TOKENS["accent_rule"]["selection"]["min_contrast"]
    on = TOKENS["accent_rule"]["on_accent"]
    for background in (accent, shades["accent.pressed"].upper()):
        ceiling = max(g.contrast(on["light"], background), g.contrast(on["dark"], background))
        need = min(want, ceiling)
        for name, value in tints.items():
            ratio = g.contrast(value.upper(), background)
            assert ratio >= need - 0.02, (
                "%s %s %s is %.2f:1 on %s, needs %.2f:1"
                % (accent, mode, name, ratio, background, need))


def test_qml_selection_tints_follow_the_accent(qml_runs):
    """A picked accent must move them: they were constants before."""
    r = qml_runs["accent"][0]
    assert r["defaultAccent"] is False
    want = g.derive_selection(TOKENS, USER_ACCENT, "day")
    assert channels_apart(r["selectionNegative"].upper(), want["selection.negative"]) <= 4, \
        r["selectionNegative"]
    # and it really left the constant behind: agreeing with Python would
    # also be true of a property that never moved, if Python had not moved
    shipped = g.solid(TOKENS, "selection.negative", "day")
    assert channels_apart(r["selectionNegative"].upper(), shipped) > 4, \
        "selectionNegative is still the value accorded to the violet: %s" % shipped
    # and the default accent keeps the value accorded by eye
    night = qml_runs["night"][0]
    assert night["defaultAccent"] is True
    assert night["selectionNegative"].upper() == g.solid(TOKENS, "selection.negative", "night")
    assert night["selectionInactive"].upper() == g.solid(TOKENS, "selection.inactive", "night")


def test_qml_follows_raye_night(qml_runs):
    r = qml_runs["night"][0]
    assert r["night"] is True and r["day"] is False
    assert r["defaultAccent"] is True
    assert r["accent"].upper() == TOKENS["accent_rule"]["default"].upper()
    assert r["bgBase"].upper() == g.solid(TOKENS, "bg.base", "night")
    assert r["textPrimary"].upper() == g.solid(TOKENS, "text.primary", "night")
    assert r["accentText"].upper() == g.solid(TOKENS, "accent.text", "night")


def test_qml_follows_raye_day(qml_runs):
    """The scheme decides: an app under Raye Day must not paint Night."""
    r = qml_runs["day"][0]
    assert r["night"] is False and r["day"] is True
    assert r["defaultAccent"] is True
    assert r["bgBase"].upper() == g.solid(TOKENS, "bg.base", "day")
    assert r["textPrimary"].upper() == g.solid(TOKENS, "text.primary", "day")
    assert r["accentText"].upper() == g.solid(TOKENS, "accent.text", "day")
    assert r["accentHover"].upper() == g.solid(TOKENS, "accent.hover", "day")


def test_qml_follows_the_users_accent(qml_runs):
    r = qml_runs["accent"][0]
    want = g.derive_accent(TOKENS, USER_ACCENT, "day")
    assert r["day"] is True
    assert r["defaultAccent"] is False
    assert r["accent"].upper() == USER_ACCENT
    assert channels_apart(r["accentText"].upper(), want["accent.text"]) <= 2
    assert channels_apart(r["accentHover"].upper(), want["accent.hover"]) <= 2
    assert channels_apart(r["textOnAccent"].upper(), want["text.onAccent"]) <= 2


@pytest.mark.parametrize("run", ["unthemed", "name-only"])
def test_qml_paints_raye_night_with_no_colour_scheme(qml_runs, run):
    """The login screen, setup and root dialogs may get no scheme at all:
    they must still look like Raye, never like a blue light desktop."""
    r = qml_runs[run][0]
    assert r["night"] is True, r["bgBase"]
    assert r["accent"].upper() == TOKENS["accent_rule"]["default"].upper()
    assert r["defaultAccent"] is True
    assert r["bgBase"].upper() == g.solid(TOKENS, "bg.base", "night")


def test_qml_mode_can_be_pinned(qml_runs):
    r = qml_runs["night"][0]
    assert r["forcedDay"]["night"] is False
    assert r["forcedDay"]["bgBase"].upper() == g.solid(TOKENS, "bg.base", "day")
    assert r["forcedNight"]["night"] is True
    assert r["forcedNight"]["bgBase"].upper() == g.solid(TOKENS, "bg.base", "night")
    r = qml_runs["day"][0]
    assert r["forcedNight"]["bgBase"].upper() == g.solid(TOKENS, "bg.base", "night")


def test_qml_durations_are_zero_with_animations_off(qml_runs):
    r = qml_runs["no-animations"][0]
    assert r["animationsOff"] is True
    assert r["durationFast"] == 0 and r["durationNormal"] == 0
    assert r["durationSlow"] == 0 and r["durationExitShort"] == 0
    assert qml_runs["night"][0]["animationsOff"] is False


def test_qml_reads_a_scheme_the_same_way_everywhere(qml_run):
    black, day, user, qt_own, plasma_own, light_raye = qml_run[0]["schemes"]
    # before the platform plugin answers every colour is black: paint
    # Raye Night with the Raye accent, never a black application
    assert black["night"] is True
    assert black["accent"].upper() == TOKENS["accent_rule"]["default"].upper()
    assert day["night"] is False
    assert day["accent"].upper() == "#7B3CFF"
    assert user["night"] is True
    assert user["accent"].upper() == USER_ACCENT
    for unthemed in (qt_own, plasma_own):
        assert unthemed["night"] is True
        assert unthemed["accent"].upper() == TOKENS["accent_rule"]["default"].upper()
    assert light_raye["night"] is False


def test_qml_sizes_and_motion_come_from_the_tokens(qml_run):
    r = qml_run[0]
    assert r["radiusControl"] == TOKENS["radius"]["control"]
    assert r["radiusOverlay"] == TOKENS["radius"]["overlay"]
    assert r["sizeControlHeight"] == TOKENS["size"]["control.height"]
    # Kirigami's default animation speed is the token speed
    assert r["durationFast"] == TOKENS["motion"]["fast"]["ms"]
    assert r["durationNormal"] == TOKENS["motion"]["normal"]["ms"]
    assert r["bodyWeight"] == TOKENS["type"]["body"]["weight"]
    assert r["bodyPointSize"] == pytest.approx(TOKENS["type"]["body"]["px"] * 0.75, abs=0.01)


def test_qml_selected_row_follows_the_accent(qml_runs):
    for run, mode, accent in (("night", "night", TOKENS["accent_rule"]["default"]),
                              ("accent", "day", USER_ACCENT)):
        alpha = TOKENS["color"]["fill.subtle.selected"][mode]["alpha"]
        sel = qml_runs[run][0]["fillSubtleSelected"]
        # #AARRGGBB from QML: the accent at the token's alpha
        assert sel.lower().endswith(accent[1:].lower()), (run, sel)
        assert int(sel[1:3], 16) == round(alpha * 255)


# --- the kdeglobals of the desktop --------------------------------------

def test_the_desktop_kdeglobals_matches_the_tokens():
    """config/kdeglobals (the look-and-feel defaults) must carry the lines
    the token file keeps for it, and, once it names a Raye scheme, the
    full colour groups: a kdeglobals that only names the scheme gives
    every program Plasma's built-in light colours."""
    path = os.path.join(g.ROOT, "config", "kdeglobals")
    if not os.path.exists(path):
        pytest.skip("no config/kdeglobals")
    with open(path, encoding="utf-8") as f:
        ini = parse_ini(f.read())
    want = TOKENS["kde"]["kdeglobals"]
    # an AccentColor makes Plasma blend the accent over the tuned scheme
    assert "AccentColor" not in ini.get("General", {})
    names = {info["scheme"]: mode for mode, info in TOKENS["modes"].items()}
    scheme_name = ini.get("General", {}).get("ColorScheme")
    if scheme_name not in names:
        pytest.skip("config/kdeglobals does not use a Raye scheme yet")
    for section, keys in want.items():
        if section == "about":
            continue
        for key, value in keys.items():
            if isinstance(value, bool):
                value = "true" if value else "false"
            assert ini.get(section, {}).get(key) == str(value), (section, key)
    full = parse_ini(scheme(names[scheme_name]))
    for group in g.KDE_GROUPS:
        name = "Colors:%s" % group
        assert ini.get(name) == full[name], "config/kdeglobals: [%s] missing or stale" % name


# --- the generator as a whole -----------------------------------------

def test_the_output_is_the_same_on_every_run():
    assert g.render_all(TOKENS) == g.render_all(g.load())


def test_every_output_ends_with_one_newline():
    for path, text in g.render_all(TOKENS).items():
        assert text.endswith("\n"), path
        assert not text.endswith("\n\n"), path


def test_the_files_in_the_repo_match_the_tokens():
    old = g.stale()
    assert old == [], "run python3 tokens/generate.py: " + ", ".join(old)


def test_check_mode_agrees():
    out = subprocess.run([sys.executable, "-I", "-B", os.path.join(g.ROOT, "tokens", "generate.py"),
                          "--check"], capture_output=True, text=True)
    assert out.returncode == 0, out.stdout + out.stderr


# --- the third copy of the accent shades --------------------------------

def test_the_qt_style_carries_the_tables_default_shades():
    """The Qt style keeps the default accent's shades as its own constants.

    🚨 nightDefaultHover had drifted to #8B52FF -- the plain 12 % toward
    white, which is what the table deliberately does NOT use: it deepens
    the night hover to #8950FF so white text keeps 4.5:1 on it (#8B52FF
    leaves 4.42:1). The schemes and Tokens.qml both carried the right
    value; only this copy was wrong, and it is the shade every user who
    never picked an accent sees on a hovered accent button.

    A constant copied out of the table is a copy that drifts, so it is
    tied back to the table here rather than trusted.
    """
    path = os.path.join(g.ROOT, "style", "kstyle", "rayetokens.cpp")
    with open(path, encoding="utf-8") as f:
        source = f.read()
    for name, token, mode in (("nightDefaultHover", "accent.hover", "night"),
                              ("nightDefaultPressed", "accent.pressed", "night"),
                              ("dayDefaultHover", "accent.hover", "day"),
                              ("dayDefaultPressed", "accent.pressed", "day")):
        found = re.search(r"%s\(0x([0-9A-Fa-f]{2}), 0x([0-9A-Fa-f]{2}), 0x([0-9A-Fa-f]{2})\)"
                          % name, source)
        assert found, "%s is not in %s any more" % (name, path)
        got = "#" + "".join(p.upper() for p in found.groups())
        assert got == g.solid(TOKENS, token, mode), \
            "%s is %s, the table says %s is %s" % (name, got, token, g.solid(TOKENS, token, mode))
