# WCAG 2 contrast of every colour scheme this repository ships.
#
#   python3 -m pytest -q tests/colors/test_contrast.py
#
# tokens/test_tokens.py checks the PAIRS the token table lists. This file
# checks the schemes those tokens ASSEMBLE: a [Colors:*] group puts one
# token on another, and a pair nobody wrote down is a pair nobody reads.
# That is how five of the selection's foregrounds shipped below 4.5:1 --
# every token in them was legal, the group put them on the accent, and
# the pair list did not name the couple.
#
# The threshold is the role, not the file. A Foreground* key is text:
# WCAG 2 SC 1.4.3, 4.5:1. A Decoration* key is a focus or hover ring, a
# user-interface component: SC 1.4.11, 3:1. A foreground is measured on
# BOTH of its group's backgrounds -- BackgroundAlternate is the alternate
# row of a list, and text lands on it exactly as often. Measuring only
# BackgroundNormal is what let the night selection's alternate row keep
# the button-hover shade, which is LIGHTER than the accent and leaves
# 4.51:1 for white -- a surface no tinted text could ever sit on.
#
# What counts is what is INSTALLED. The schemes are found by running
# debian/raye-theme.install, the way tests/lookandfeel/test_names_resolve
# does, so a scheme added to that list is measured here without anyone
# remembering to add it, and a scheme dropped from the list stops being
# measured. config/kdeglobals is read too: it carries a full copy of the
# night groups for programs that read nothing else.

import colorsys
import os
import re
import subprocess
import sys

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# WCAG 2 minimums, by what the key is for.
TEXT_MIN = 4.5          # SC 1.4.3, normal-size text
UI_MIN = 3.0            # SC 1.4.11, non-text contrast

TEXT_ROLES = ("ForegroundNormal", "ForegroundInactive", "ForegroundActive",
              "ForegroundLink", "ForegroundVisited", "ForegroundNegative",
              "ForegroundNeutral", "ForegroundPositive")
UI_ROLES = ("DecorationFocus", "DecorationHover")
BACKGROUNDS = ("BackgroundNormal", "BackgroundAlternate")

# Structural, not an excuse: in the Selection group both Decoration
# keys and both Background keys are shades of the accent, so a focus ring
# of the accent on the accent draws nothing. Breeze ships the same
# property -- its [Colors:Selection] has DecorationFocus exactly equal to
# BackgroundNormal. There is no pair to measure here, in any scheme.
NO_DECORATION = ("Colors:Selection",)

# Pairs that are still short, each with its measured ratio. This is a
# ledger, not a waiver: test_every_known_gap_is_still_a_gap fails when
# one of these is fixed, so the list can only shrink, and
# test_no_pair_outside_the_ledger_is_short fails when a new one appears.
#
# All three are DecorationFocus = the accent itself on a night surface
# that is LIGHTER than the window background: the accent holds 3.54:1 on
# bg.base but 2.71-2.94:1 on a button fill or a tooltip. Fixing it means
# either moving the accent -- which the user picks, and which "accent
# colours not dulled" forbids -- or giving Decoration* a per-group shade.
# That is a design decision, not a defect to patch here. Raye's own
# widgets are unaffected: the Qt style draws focus from focus.outer
# (style/kstyle/rayehelper.cpp) and QML from Tokens.focusOuter
# (qml/org/raye/ui/FocusRing.qml); these keys are read by other toolkits.
KNOWN_GAPS = {
    ("colors/RayeNight.colors", "Colors:Button", "DecorationFocus", "BackgroundNormal"),
    ("colors/RayeNight.colors", "Colors:Button", "DecorationFocus", "BackgroundAlternate"),
    ("colors/RayeNight.colors", "Colors:Tooltip", "DecorationFocus", "BackgroundNormal"),
    ("config/kdeglobals", "Colors:Button", "DecorationFocus", "BackgroundNormal"),
    ("config/kdeglobals", "Colors:Button", "DecorationFocus", "BackgroundAlternate"),
    ("config/kdeglobals", "Colors:Tooltip", "DecorationFocus", "BackgroundNormal"),
}


# --- WCAG 2 -------------------------------------------------------------

def channel(value):
    """One sRGB channel, 0-255, as linear light (WCAG 2 relative luminance)."""
    c = value / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def luminance(rgb):
    r, g, b = (channel(v) for v in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(fg, bg):
    """(L1 + 0.05) / (L2 + 0.05), lighter colour first."""
    a, b = luminance(fg), luminance(bg)
    return (max(a, b) + 0.05) / (min(a, b) + 0.05)


def test_the_formula_matches_the_values_wcag_states():
    """Black on white is 21:1 and a colour on itself is 1:1, exactly."""
    assert contrast((0, 0, 0), (255, 255, 255)) == pytest.approx(21.0, abs=1e-9)
    assert contrast((123, 60, 255), (123, 60, 255)) == pytest.approx(1.0, abs=1e-9)
    # the pair WCAG's own examples give for #808080 on #FFFFFF
    assert contrast((128, 128, 128), (255, 255, 255)) == pytest.approx(3.95, abs=0.01)


# --- what ships ---------------------------------------------------------

def installed():
    """(source, destination) for every line debian/raye-theme.install runs.

    The list is a script and its "? " lines vanish in silence when the
    source is not there yet, so it is RUN, never read.
    """
    out = subprocess.run([os.path.join(ROOT, "debian", "raye-theme.install")],
                         cwd=ROOT, capture_output=True, text=True, check=True)
    pairs = []
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) == 2:
            pairs.append((parts[0], parts[1]))
    return pairs


def schemes():
    """Every file this package installs that carries [Colors:*] groups."""
    found = [src for src, dest in installed()
             if dest == "usr/share/color-schemes" and src.endswith(".colors")]
    found += [src for src, dest in installed()
              if dest == "etc/xdg" and os.path.basename(src) == "kdeglobals"]
    return sorted(found)


SCHEMES = schemes()


def test_the_schemes_were_found():
    """A sweep over nothing passes for the one reason that proves nothing."""
    assert SCHEMES, "debian/raye-theme.install installs no colour scheme"
    names = [os.path.basename(s) for s in SCHEMES]
    # both modes, because a couple that holds in the dark can fail in the light
    assert "RayeNight.colors" in names, names
    assert "RayeDay.colors" in names, names
    assert "kdeglobals" in names, names


def parse(path):
    """KConfig groups as {name: {key: value}}, comments dropped."""
    out, group = {}, None
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            head = re.match(r"^\[(.+)\]$", line)
            if head:
                group = head.group(1)
                out.setdefault(group, {})
                continue
            if "=" in line and group is not None:
                key, _, value = line.partition("=")
                out[group][key.strip()] = value.strip()
    return out


def rgb(value):
    """"r,g,b" as KDE writes a colour; None for anything else."""
    parts = [p.strip() for p in value.split(",")]
    if len(parts) == 3 and all(p.isdigit() and int(p) <= 255 for p in parts):
        return tuple(int(p) for p in parts)
    return None


def colour_groups(scheme):
    return {name: keys for name, keys in parse(scheme).items()
            if name.startswith("Colors:")}


def measured(scheme):
    """Every (group, role, background) pair of one scheme, with its ratio."""
    rows = []
    for name, keys in colour_groups(scheme).items():
        roles = [(r, TEXT_MIN) for r in TEXT_ROLES]
        if name not in NO_DECORATION:
            roles += [(r, UI_MIN) for r in UI_ROLES]
        for role, need in roles:
            fg = rgb(keys[role])
            for bg_key in BACKGROUNDS:
                bg = rgb(keys[bg_key])
                rows.append((name, role, bg_key, keys[role], keys[bg_key],
                             contrast(fg, bg), need))
    return rows


@pytest.mark.parametrize("scheme", SCHEMES)
def test_every_group_carries_every_role(scheme):
    """A missing key would quietly take its pairs out of the sweep."""
    groups = colour_groups(scheme)
    assert groups, "%s has no [Colors:*] group" % scheme
    for name, keys in sorted(groups.items()):
        for key in TEXT_ROLES + UI_ROLES + BACKGROUNDS:
            assert key in keys, "%s [%s] has no %s" % (scheme, name, key)
            assert rgb(keys[key]) is not None, (
                "%s [%s] %s = %r is not an r,g,b colour" % (scheme, name, key, keys[key]))


def describe(scheme, name, role, bg_key, fg, bg, ratio, need):
    return ("%s [%s] %s=%s on %s=%s is %.2f:1, needs %.1f:1"
            % (scheme, name, role, fg, bg_key, bg, ratio, need))


@pytest.mark.parametrize("scheme", SCHEMES)
def test_no_pair_outside_the_ledger_is_short(scheme):
    short = [describe(scheme, *row) for row in measured(scheme)
             if row[5] < row[6]
             and (scheme, row[0], row[1], row[2]) not in KNOWN_GAPS]
    assert not short, "contrast too low:\n  " + "\n  ".join(sorted(short))


@pytest.mark.parametrize("scheme", SCHEMES)
def test_every_known_gap_is_still_a_gap(scheme):
    """A ledger that outlives its gap is a lie about the scheme."""
    rows = {(row[0], row[1], row[2]): row for row in measured(scheme)}
    fixed = []
    for entry in sorted(e for e in KNOWN_GAPS if e[0] == scheme):
        key = entry[1:]
        assert key in rows, "KNOWN_GAPS names %s, which %s has no pair for" % (entry, scheme)
        row = rows[key]
        if row[5] >= row[6]:
            fixed.append("%s [%s] %s on %s now reaches %.2f:1"
                         % (scheme, row[0], row[1], row[2], row[5]))
    assert not fixed, ("fixed -- delete these from KNOWN_GAPS:\n  "
                       + "\n  ".join(fixed))


def test_the_ledger_names_only_schemes_that_ship():
    unknown = sorted({e[0] for e in KNOWN_GAPS} - set(SCHEMES))
    assert not unknown, "KNOWN_GAPS names files no longer installed: %s" % unknown


# --- the pair that started this -----------------------------------------

@pytest.mark.parametrize("scheme", SCHEMES)
def test_a_selected_link_is_readable_on_its_own_selection(scheme):
    """A link, a visited link and the secondary text of a selected row.

    ⚠️ On the selection's OWN background, never the window's: measuring a
    selected link on the window background is what makes dark-on-dark
    look fine. Both backgrounds, because the alternate row is a selection
    background too.
    """
    for name, keys in sorted(colour_groups(scheme).items()):
        if name != "Colors:Selection":
            continue
        for role in ("ForegroundLink", "ForegroundVisited", "ForegroundInactive",
                     "ForegroundNormal", "ForegroundActive"):
            for bg_key in BACKGROUNDS:
                ratio = contrast(rgb(keys[role]), rgb(keys[bg_key]))
                assert ratio >= TEXT_MIN, describe(
                    scheme, name, role, bg_key, keys[role], keys[bg_key], ratio, TEXT_MIN)


# --- the accent the USER picks -------------------------------------------
#
# Everything above measures files. This measures what tokens/accent.py
# WRITES when the user picks another accent: the same scheme machinery,
# with the accent shades and the five tinted foregrounds of a selected row
# derived at run time. That is where the hole was. Three of those five --
# ForegroundNegative, ForegroundNeutral, ForegroundPositive -- had no
# derivation rule at all. They were constants accorded by eye to the
# violet, and a constant on a background that moves holds no ratio: on the
# amber accent #E5A000, ForegroundNegative sat at 1.92:1. The other two
# were derived, but by a FRACTION of the way toward the accent, which
# guarantees a distance and not a ratio; they failed too.
#
# Why this sweep is not the test that was refused before. Hardening
# this file was turned down on the grounds that it would end up either
# falsely green or falsely red, and that objection was right about a flat
# threshold:
#
#   Falsely RED is asserting a bare 4.5:1. On a mid-tone accent --
#   #CC33CC, #7A7A7A -- NEITHER white nor the dark text reaches 4.5:1 on
#   the accent itself: the ceiling is 4.28:1. No colour of any hue can
#   clear 4.5:1 there, so a bare threshold would fail for ever, for a
#   reason no change to this repository could fix, and the test would
#   rightly be deleted.
#
#   Falsely GREEN is answering that with a blanket floor (">= 3:1") or a
#   ledger of excused accents. Both pass while a real regression hides.
#
# The threshold below is neither: it is min(4.5, the best ratio ANY colour
# reaches on that background). Where 4.5:1 is reachable it is demanded in
# full; where it is not, what is demanded is the readable ceiling, which
# cannot be satisfied by giving up -- only by picking the best colour
# that background allows. tokens/test_tokens.py already reasons this way
# about text on an accent (test_an_accent_no_text_holds_on_gets_no_worse);
# this applies it to the pairs the SCHEME assembles, which is this file's
# whole subject.
#
# Decoration* is out of the sweep, and that is a decision, not an
# excuse. DecorationFocus IS the accent, and the accent is the user's:
# holding it to 3:1 on every surface would forbid a pale yellow or a
# near-black accent outright. It is the same design decision KNOWN_GAPS
# already records for the shipped schemes, it is not text, and SC 1.4.3
# does not reach it.

sys.path.insert(0, os.path.join(ROOT, "tokens"))

import accent as accent_tool  # noqa: E402
import generate as tokens_gen  # noqa: E402

TOKENS = tokens_gen.load()


def accent_sweep():
    """Accents a user could actually pick: the hue circle, several
    lightnesses and saturations, and the cases that are hard on purpose.

    A sweep of three colours chosen after the fact proves nothing, so the
    hue goes round in full. #FFFF00 and #00FFFF are the bright corners,
    #F2F2F2 and #0A0A0A the two ends of lightness, and #808080, #7A7A7A
    and #CC33CC are the mid-tones where no text colour reaches 4.5:1 --
    the ones that make a bare threshold impossible.
    """
    out = []
    for hue in range(0, 360, 30):
        for saturation, lightness in ((1.0, 0.50), (1.0, 0.35), (1.0, 0.72), (0.45, 0.50)):
            r, g, b = colorsys.hls_to_rgb(hue / 360.0, lightness, saturation)
            out.append(tokens_gen.hexstr((r * 255, g * 255, b * 255)))
    out += ["#FFFF00", "#00FFFF", "#FF00FF", "#F2F2F2", "#0A0A0A",
            "#808080", "#7A7A7A", "#CC33CC", "#E5A000", "#FFB900",
            "#0078D4", "#107C10", "#C50F1F", "#E3008C", "#7B3CFF"]
    seen, unique = set(), []
    for colour in out:
        if colour not in seen:
            seen.add(colour)
            unique.append(colour)
    return unique


ACCENT_SWEEP = accent_sweep()


def test_the_accent_sweep_is_wide_and_holds_the_hard_cases():
    """A sweep over nothing, or over three friendly colours, proves nothing."""
    assert len(ACCENT_SWEEP) >= 50, len(ACCENT_SWEEP)
    hues = {round(colorsys.rgb_to_hls(*[v / 255.0 for v in tokens_gen.rgb(c)])[0] * 12)
            for c in ACCENT_SWEEP}
    assert len(hues) >= 12, "the sweep does not go round the hue circle: %s" % sorted(hues)
    # the accents that make a flat 4.5:1 impossible must be in it, or the
    # threshold below is never exercised and its reason is never tested
    for mid in ("#7A7A7A", "#CC33CC"):
        assert mid in ACCENT_SWEEP
        assert readable_ceiling(tokens_gen.rgb(mid)) < TEXT_MIN, mid
    assert TOKENS["accent_rule"]["default"] in ACCENT_SWEEP


def readable_ceiling(bg):
    """The best ratio ANY colour reaches on bg: white, or the dark text.

    Nothing beats it, so it is the only honest threshold on a background
    that nothing holds 4.5:1 on -- and the one thing a test can demand
    there without asking for the impossible.
    """
    on = TOKENS["accent_rule"]["on_accent"]
    return max(contrast(tokens_gen.rgb(on["light"]), bg),
               contrast(tokens_gen.rgb(on["dark"]), bg))


def parse_text(text):
    """A scheme held in memory, as parse() reads one from disk."""
    out, group = {}, None
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        head = re.match(r"^\[(.+)\]$", line)
        if head:
            group = head.group(1)
            out.setdefault(group, {})
            continue
        if "=" in line and group is not None:
            key, _, value = line.partition("=")
            out[group][key.strip()] = value.strip()
    return out


def picked(accent, mode):
    """[(group, role, background, fg, bg, ratio, needed)] for one accent.

    Only text roles: see the note above on Decoration*.
    """
    name, text = accent_tool.scheme(TOKENS, accent, mode)
    rows = []
    for group, keys in parse_text(text).items():
        if not group.startswith("Colors:"):
            continue
        for role in TEXT_ROLES:
            fg = rgb(keys[role])
            for bg_key in BACKGROUNDS:
                bg = rgb(keys[bg_key])
                need = min(TEXT_MIN, readable_ceiling(bg))
                rows.append((group, role, bg_key, keys[role], keys[bg_key],
                             contrast(fg, bg), need))
    return rows


@pytest.mark.parametrize("mode", sorted(TOKENS["modes"]))
@pytest.mark.parametrize("accent", ACCENT_SWEEP)
def test_no_text_of_a_picked_accent_falls_under_its_threshold(accent, mode):
    """The property the three constants broke, for every accent swept.

    ⚠️ Measured on the selection's OWN backgrounds, both of them: the
    alternate selected row is a selection background too, and that is
    exactly what hid five of these pairs the first time.
    """
    short = [describe(("accent %s" % accent), *row) for row in picked(accent, mode)
             if row[5] < row[6] - 0.01]
    assert not short, ("%s accent %s: contrast too low:\n  " % (mode, accent)
                       + "\n  ".join(sorted(short)))


# "still reads as its hue" is CHROMA, not HSL saturation. #FFFEFF has an
# HSL saturation of 1.0 and is white to the eye: at the extremes of
# lightness the saturation of a colour says nothing about whether anyone
# can see its hue. Chroma -- the spread between the strongest and the
# weakest channel -- says it directly. 12/255 is about 5 %, under which a
# tint is a grey with a rounding error in it.
MIN_CHROMA = 12


def hue_bearing(colour):
    """True while the colour still reads as a hue rather than as a grey."""
    channels = tokens_gen.rgb(colour)
    return max(channels) - min(channels) >= MIN_CHROMA


def admissible_hue_exists(seed, backgrounds):
    """Is there ANY lightness of the seed's hue that holds on both
    backgrounds and is still a colour rather than a grey?

    Brute force on purpose: this is the question the rule must answer
    well, so the test must answer it independently of the rule.
    """
    h, s, _ = tokens_gen.hsl(seed)
    needs = [min(TEXT_MIN, readable_ceiling(tokens_gen.rgb(bg))) for bg in backgrounds]
    for i in range(4001):
        candidate = tokens_gen.from_hsl(h, s, i / 4000.0)
        if not hue_bearing(candidate):
            continue
        if all(contrast(tokens_gen.rgb(candidate), tokens_gen.rgb(bg)) >= need - 1e-9
               for bg, need in zip(backgrounds, needs)):
            return candidate
    return None


@pytest.mark.parametrize("mode", sorted(TOKENS["modes"]))
@pytest.mark.parametrize("accent", ACCENT_SWEEP)
def test_a_signal_tint_loses_its_hue_only_when_no_hue_could_hold(accent, mode):
    """Red stays red, amber amber and green green -- or nothing could.

    🚨 This is the trade-off, stated as a test rather than as a promise.
    A negative that reads as the accent says nothing, so the rule keeps
    the token's own hue and moves only its lightness. But the set of
    lightnesses that hold 4.5:1 on BOTH selection backgrounds can be
    empty for every hue at once: on #0078D4 the accent's own ceiling is
    4.53:1, so anything readable on it is within a hair of white, and the
    pink of ForegroundNegative is not available at any lightness.
    Contrast wins there, and the tint goes white.
    
    What this test forbids is giving up EARLY: whenever a colour of the
    token's hue could have held, the rule must have found one. The
    question is settled by brute force over every lightness, so the test
    does not take the rule's word for it.
    """
    derived = tokens_gen.derive_accent(TOKENS, accent, mode)
    tints = tokens_gen.derive_selection(TOKENS, accent, mode, derived)
    backgrounds = [derived[name] for name in TOKENS["accent_rule"]["selection"]["on"]]
    for name in ("selection.negative", "selection.neutral", "selection.positive"):
        seed = tokens_gen.solid(TOKENS, name, mode)
        possible = admissible_hue_exists(seed, backgrounds)
        got = tints[name]
        if possible is None:
            assert not hue_bearing(got), (
                "%s %s %s: no lightness of this hue holds, so the tint should"
                " be the readable extreme, got %s" % (mode, accent, name, got))
            continue
        assert hue_bearing(got), (
            "%s %s %s gave up its hue at %s, but %s holds and keeps it"
            % (mode, accent, name, got, possible))
        want_hue = tokens_gen.hsl(seed)[0]
        apart = abs(want_hue - tokens_gen.hsl(got)[0]) % 1.0
        assert min(apart, 1.0 - apart) <= 0.05, (
            "%s %s %s drifted off its hue: %s -> %s" % (mode, accent, name, seed, got))


def test_the_default_accent_does_not_move():
    """The shipped schemes are accorded by eye; the rule must not touch them.

    If this fails, the 573 generated files of this repository would change
    for a user who never picked an accent at all.
    """
    for mode in sorted(TOKENS["modes"]):
        default = TOKENS["accent_rule"]["default"]
        name, text = accent_tool.scheme(TOKENS, default, mode)
        assert name == TOKENS["modes"][mode]["scheme"]
        with open(os.path.join(ROOT, "colors", name + ".colors"), encoding="utf-8") as f:
            assert text == f.read(), "%s is not the shipped scheme any more" % name
        # and the rule itself lands on the hand-tuned tints
        tints = tokens_gen.derive_selection(TOKENS, default, mode)
        for key, value in tints.items():
            shipped = tokens_gen.solid(TOKENS, key, mode)
            apart = max(abs(a - b) for a, b in
                        zip(tokens_gen.rgb(value), tokens_gen.rgb(shipped)))
            assert apart <= 2, ("%s (%s): the rule gives %s, the table says %s"
                                % (key, mode, value, shipped))
