# Tests for tokens/accent.py: the accent a user picks, written as a whole
# colour scheme instead of Plasma's blend.
#
#   python3 -m pytest tokens/

import hashlib
import os
import subprocess
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import accent as a  # noqa: E402
import generate as g  # noqa: E402

TOKENS = g.load()
TOOL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "accent.py")
SWATCHES = ["#9450F0", "#2672D9", "#3F8F3A", "#D63C3C", "#8F7A0E", "#5F6B7D"]


def ini(text):
    out = {}
    for header, lines in a.parse(text):
        if header:
            out[header] = a.entries([(header, lines)], header)
    return out


def shipped(mode):
    name = TOKENS["modes"][mode]["scheme"]
    with open(os.path.join(g.ROOT, "colors", name + ".colors"), encoding="utf-8") as f:
        return f.read()


@pytest.mark.parametrize("value, want", [
    ("#2672d9", "#2672D9"), ("2672D9", "#2672D9"), ("38,114,217", "#2672D9"),
    ("38, 114, 217", "#2672D9"), ("123,60,255,255", "#7B3CFF"),
    ("blue", None), ("300,0,0", None), ("", None)])
def test_colours_are_read_as_kde_and_qt_write_them(value, want):
    assert a.normalise(value) == want


@pytest.mark.parametrize("mode", g.MODES)
def test_the_default_accent_is_the_shipped_scheme(mode):
    name, text = a.scheme(TOKENS, TOKENS["accent_rule"]["default"].lower(), mode)
    assert name == TOKENS["modes"][mode]["scheme"]
    assert text == shipped(mode)


@pytest.mark.parametrize("mode", g.MODES)
@pytest.mark.parametrize("accent", SWATCHES)
def test_a_picked_accent_takes_every_shade_from_the_rule(accent, mode):
    name, text = a.scheme(TOKENS, accent, mode)
    assert name == TOKENS["modes"][mode]["scheme"] + "Accent"
    d = g.derive_accent(TOKENS, accent, mode)
    rgb = {k: g.kde_rgb(v) for k, v in d.items()}
    s = ini(text)
    assert s["[Colors:Selection]"]["BackgroundNormal"] == rgb["accent"]
    # the alternate selected row is the accent's DARKER shade: the hover shade
    # is lighter than the accent on night and leaves no room for readable text
    assert s["[Colors:Selection]"]["BackgroundAlternate"] == rgb["accent.pressed"]
    assert s["[Colors:Selection]"]["ForegroundNormal"] == rgb["text.onAccent"]
    for group in ("[Colors:Window]", "[Colors:View]", "[Colors:Button]"):
        assert s[group]["DecorationFocus"] == rgb["accent"]
        assert s[group]["DecorationHover"] == rgb["accent.hover"]
        assert s[group]["ForegroundLink"] == rgb["accent.text"]
        assert s[group]["ForegroundActive"] == rgb["accent.text"]
    # nothing but the accent moved
    base = ini(shipped(mode))
    assert s["[Colors:View]"]["BackgroundNormal"] == base["[Colors:View]"]["BackgroundNormal"]
    assert s["[WM]"] == base["[WM]"]


@pytest.mark.parametrize("mode", g.MODES)
@pytest.mark.parametrize("accent", SWATCHES + ["#FFB900", "#7B3CFF"])
def test_text_on_a_selection_stays_readable(accent, mode):
    """The bug Plasma's blend left: the selection's link colour on the
    selection itself."""
    s = ini(a.scheme(TOKENS, accent, mode)[1])["[Colors:Selection]"]

    def hexed(v):
        return g.hexstr(tuple(int(x) for x in v.split(",")))
    bg = hexed(s["BackgroundNormal"])
    # a mid tone no text reaches 4.5:1 on keeps the best of the two
    best = min(4.5, max(g.contrast(c, bg) for c in ("#FFFFFF", TOKENS["accent_rule"]["on_accent"]["dark"])))
    assert g.contrast(hexed(s["ForegroundNormal"]), bg) >= best - 0.01, (accent, mode)
    for key in ("ForegroundLink", "ForegroundActive"):
        assert g.contrast(hexed(s[key]), bg) >= best - 0.01, (accent, mode, key)
    assert g.contrast(hexed(s["ForegroundInactive"]), bg) >= 3.0, (accent, mode)


def test_the_selection_tints_follow_the_accent():
    s = ini(a.scheme(TOKENS, "#2672D9", "night")[1])["[Colors:Selection]"]
    r, gr, b = (int(v) for v in s["ForegroundInactive"].split(","))
    # a light blue, not the lavender tuned for violet
    assert b > r and gr > r


GLOBALS = """\
[$Version]
update_info=filepicker.upd:filepicker-remove-old-previews-entry

[General]
AccentColor=123,60,255
ColorScheme=RayeNight
TerminalApplication[$e]=raye-terminal
font=Inter,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1

[Colors:Selection]
BackgroundNormal=94,48,190
ForegroundLink=94,0,213

[Colors:Header][Inactive]
BackgroundNormal=1,2,3

[Icons]
Theme=Raye-Dark
"""


def test_applying_puts_the_scheme_in_and_keeps_the_rest():
    name, text = a.scheme(TOKENS, "#7B3CFF", "night")
    out = a.apply(GLOBALS, name, text)
    got, want = ini(out), ini(text)
    assert "AccentColor" not in got["[General]"]
    assert got["[General]"]["ColorScheme"] == "RayeNight"
    assert len(got["[General]"]["ColorSchemeHash"]) == 40
    assert got["[General]"]["font"].startswith("Inter,10")
    assert got["[General]"]["TerminalApplication"] == "raye-terminal"
    assert "TerminalApplication[$e]=raye-terminal" in out
    assert got["[Icons]"] == {"Theme": "Raye-Dark"}
    assert got["[$Version]"]["update_info"].startswith("filepicker.upd")
    for header, keys in want.items():
        if header.startswith(("[Colors:", "[ColorEffects:", "[WM]")):
            assert got[header] == keys, header
    assert got["[Colors:Selection]"]["BackgroundNormal"] == "123,60,255"


def test_applying_twice_changes_nothing_more():
    name, text = a.scheme(TOKENS, "#2672D9", "day")
    once = a.apply(GLOBALS, name, text)
    assert a.apply(once, name, text) == once


@pytest.mark.parametrize("text, mode, want", [
    (GLOBALS, "night", "#7B3CFF"),
    ("[General]\nAccentColor=#2672d9\nColorScheme=RayeDay\n", "day", "#2672D9"),
    ("[General]\nAccentColor=38,114,217\naccentColorFromWallpaper=true\n", "night", None),
    ("[General]\nAccentColor=38,114,217\nColorScheme=BreezeDark\n", None, None),
    # the blend made from the system file's AccentColor, none in the account
    (GLOBALS.replace("AccentColor=123,60,255\n", ""), "night", "#7B3CFF"),
    ("[General]\nColorScheme=RayeNight\n", "night", None),
    ("[General]\nColorScheme=RayeNight\n\n[Colors:Selection]\nBackgroundNormal=123,60,255\n",
     "night", None),
    ("", "night", None)])
def test_repair_only_touches_an_accent_plasma_blends(text, mode, want):
    assert a.repair_accent(text, TOKENS, mode) == want


@pytest.mark.parametrize("name, want", [
    ("RayeNight", "night"), ("RayeDay", "day"), ("RayeNightAccent", "night"),
    ("RayeDayAccent", "day"), ("BreezeDark", None), ("", None)])
def test_only_raye_schemes_are_rewritten(name, want):
    assert a.mode_of(name, TOKENS) == want


# --- the program, as Settings and kconf_update run it -----------------

def run(tmp_path, *args):
    env = dict(os.environ, XDG_CONFIG_HOME=str(tmp_path / "config"),
               XDG_DATA_HOME=str(tmp_path / "data"),
               XDG_CONFIG_DIRS=str(tmp_path / "xdg"),
               DBUS_SESSION_BUS_ADDRESS="unix:path=/nonexistent",
               PATH="/nonexistent", PYTHONDONTWRITEBYTECODE="1")
    return subprocess.run([sys.executable, TOOL] + list(args), env=env,
                          capture_output=True, text=True, timeout=60)


def test_repair_of_an_existing_account(tmp_path):
    (tmp_path / "config").mkdir()
    (tmp_path / "config" / "kdeglobals").write_text(GLOBALS)
    assert run(tmp_path, "repair").returncode == 0
    got = ini((tmp_path / "config" / "kdeglobals").read_text())
    assert "AccentColor" not in got["[General]"]
    assert got["[Colors:Selection]"] == ini(shipped("night"))["[Colors:Selection]"]
    assert not (tmp_path / "data" / "color-schemes").exists()


def test_repair_keeps_an_accent_the_user_chose(tmp_path):
    (tmp_path / "config").mkdir()
    (tmp_path / "config" / "kdeglobals").write_text(
        GLOBALS.replace("AccentColor=123,60,255", "AccentColor=38,114,217"))
    assert run(tmp_path, "repair").returncode == 0
    got = ini((tmp_path / "config" / "kdeglobals").read_text())
    assert got["[General]"]["ColorScheme"] == "RayeNightAccent"
    assert got["[Colors:Selection]"]["BackgroundNormal"] == "38,114,217"
    own = tmp_path / "data" / "color-schemes" / "RayeNightAccent.colors"
    assert own.read_text() == a.scheme(TOKENS, "#2672D9", "night")[1]


def test_apply_reads_the_mode_from_the_system_file(tmp_path):
    (tmp_path / "xdg").mkdir()
    (tmp_path / "xdg" / "kdeglobals").write_text("[General]\nColorScheme=RayeDay\n")
    assert run(tmp_path, "apply", "#3f8f3a").returncode == 0
    got = ini((tmp_path / "config" / "kdeglobals").read_text())
    assert got["[General]"]["ColorScheme"] == "RayeDayAccent"
    # and back to the default: the shipped scheme, the own file gone
    assert run(tmp_path, "apply", "#7b3cff").returncode == 0
    got = ini((tmp_path / "config" / "kdeglobals").read_text())
    assert got["[General]"]["ColorScheme"] == "RayeDay"
    assert got["[Colors:Selection]"] == ini(shipped("day"))["[Colors:Selection]"]
    assert not (tmp_path / "data" / "color-schemes" / "RayeDayAccent.colors").exists()


def test_apply_refuses_what_is_not_a_colour(tmp_path):
    assert run(tmp_path, "apply", "violet").returncode == 2
    assert not (tmp_path / "config" / "kdeglobals").exists()


# --- accounts whose own scheme was written with older greys -------------

def older_greys(text):
    """A scheme as an older Raye wrote it: the same accent, other greys."""
    out = []
    group = None
    for line in text.splitlines():
        if line.startswith("["):
            group = line
        elif group in ("[Colors:Window]", "[Colors:View]") and line.startswith("BackgroundNormal="):
            line = "BackgroundNormal=" + ("19,14,31" if group == "[Colors:Window]" else "27,21,41")
        out.append(line)
    return "\n".join(out) + "\n"


def old_account(tmp_path, accent, mode):
    name, text = a.scheme(TOKENS, accent, mode)
    old = older_greys(text)
    plain = GLOBALS.replace("AccentColor=123,60,255\n", "")
    (tmp_path / "config").mkdir()
    (tmp_path / "config" / "kdeglobals").write_text(a.apply(plain, name, old))
    own = tmp_path / "data" / "color-schemes" / (name + ".colors")
    own.parent.mkdir(parents=True)
    own.write_text(old)
    return own


@pytest.mark.parametrize("mode", g.MODES)
def test_repair_finds_an_accent_scheme_with_older_greys(mode):
    name, text = a.scheme(TOKENS, "#2672D9", mode)
    plain = GLOBALS.replace("AccentColor=123,60,255\n", "")
    assert a.repair_accent(a.apply(plain, name, older_greys(text)), TOKENS, mode) == "#2672D9"
    assert a.repair_accent(a.apply(plain, name, text), TOKENS, mode) is None


@pytest.mark.parametrize("mode", g.MODES)
def test_repair_rewrites_an_accent_scheme_with_older_greys(tmp_path, mode):
    own = old_account(tmp_path, "#2672D9", mode)
    assert run(tmp_path, "repair").returncode == 0
    name, text = a.scheme(TOKENS, "#2672D9", mode)
    assert own.read_text() == text
    got = ini((tmp_path / "config" / "kdeglobals").read_text())
    want = ini(text)
    assert got["[General]"]["ColorScheme"] == name
    for group in ("[Colors:Window]", "[Colors:View]", "[Colors:Selection]"):
        assert got[group] == want[group], group
    # the hash Plasma checks at sign-in matches the file on disk
    assert got["[General]"]["ColorSchemeHash"] == hashlib.sha1(text.encode()).hexdigest()


def test_repair_leaves_an_up_to_date_accent_alone(tmp_path):
    name, text = a.scheme(TOKENS, "#3F8F3A", "night")
    (tmp_path / "config").mkdir()
    before = a.apply(GLOBALS.replace("AccentColor=123,60,255\n", ""), name, text)
    (tmp_path / "config" / "kdeglobals").write_text(before)
    assert run(tmp_path, "repair").returncode == 0
    assert (tmp_path / "config" / "kdeglobals").read_text() == before
    assert not (tmp_path / "data" / "color-schemes").exists()


def test_new_greys_reach_accounts_that_already_ran_the_repair():
    upd = os.path.join(g.ROOT, "config", "kconf_update", "raye-accent-repair.upd")
    with open(upd, encoding="utf-8") as f:
        ids = [line.split("=", 1)[1].strip() for line in f if line.startswith("Id=")]
    assert ids and ids[-1] != "raye-accent-repair-1"
