# The neutral colours carry at most a hint of the violet: the accent is
# for what can be clicked, not for every surface and every line of text.
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate as g  # noqa: E402

TOKENS = g.load()

# greys of the surfaces, the strokes and the text
NEUTRAL = [
    "bg.base", "bg.secondary", "bg.tertiary", "bg.quarternary", "bg.layer",
    "bg.card", "bg.card.hover", "bg.tooltip",
    "fill.control", "fill.control.hover", "fill.control.pressed",
    "fill.control.disabled", "fill.input.active",
    "stroke.control", "stroke.control.bottom", "stroke.strong",
    "text.primary", "text.secondary", "text.tertiary", "text.disabled",
    "focus.outer", "acrylic.tint", "acrylic.taskbar",
]
# max - min of the RGB channels, out of 255; #130E1F was 17
MAX_CHROMA = 12


def chroma(hexstr):
    r, gr, b = g.rgb(hexstr)
    return max(r, gr, b) - min(r, gr, b)


@pytest.mark.parametrize("mode", ["night", "day"])
@pytest.mark.parametrize("name", NEUTRAL)
def test_neutral_is_nearly_grey(name, mode):
    assert chroma(g.solid(TOKENS, name, mode)) <= MAX_CHROMA


@pytest.mark.parametrize("mode", ["night", "day"])
def test_selected_row_is_not_painted_with_the_accent(mode):
    c = TOKENS["color"]["fill.subtle.selected"][mode]
    c = c["color"] if isinstance(c, dict) else c
    assert c.upper() != TOKENS["color"]["accent"][mode].upper()
    assert chroma(c) <= MAX_CHROMA


@pytest.mark.parametrize("mode", ["night", "day"])
def test_glow_is_the_accent_not_magenta(mode):
    c = TOKENS["color"]["glow"][mode]
    assert c["color"].upper() == TOKENS["color"]["accent"][mode].upper()
    assert c["alpha"] <= 0.25


@pytest.mark.parametrize("mode", ["night", "day"])
def test_terminal_background_is_nearly_grey(mode):
    t = TOKENS["terminal"][mode]
    for key in ("background", "background.intense", "foreground"):
        assert chroma(t[key]) <= MAX_CHROMA, key
