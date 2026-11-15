# The controls of the Plasma style (art/plasmastyle_controls.py): the files
# are current, each holds the elements Plasma looks for under Plasma's own
# names, the hint sizes are Windows' (switch 40x20, radio 20, slider thumb
# 20, progress 3, scroll bar 12, 32 px buttons) and every colour comes from
# the colour scheme.
#
#   python3 -m pytest -q art/test_plasmastyle_controls.py

import os
import re
import sys
import xml.etree.ElementTree as ET

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import plasmastyle_controls as pc  # noqa: E402

SVG_NS = "{http://www.w3.org/2000/svg}"
PIECES = pc.PIECES
SIDES = pc.SIDES
FILES = pc.render_all()


def frame(prefix):
    return {prefix + p for p in PIECES}


def margins(prefix="", kind="margin"):
    return {"%shint-%s-%s" % (prefix, s, kind) for s in SIDES}


def framed(prefix, kind="margin"):
    return frame(prefix) | margins(prefix, kind)


# What each file must hold: the names the stock style uses and the shell's
# QML asks for (libplasma's PlasmaComponents and the applets).
REQUIRED = {
    "widgets/button.svg": (framed("normal-") | framed("pressed-") | framed("hover-") | framed("focus-")
                           | framed("toolbutton-hover-") | framed("toolbutton-pressed-") | framed("toolbutton-focus-")
                           | framed("shadow-") | frame("mask-normal-")
                           | {"normal-hint-compose-over-border", "pressed-hint-compose-over-border"}),
    "widgets/actionbutton.svg": {"%s%s" % (p, s) for p in ("", "16-16-", "22-22-", "24-24-")
                                 for s in ("normal", "hover", "pressed", "focus")},
    "widgets/lineedit.svg": (framed("base-") | framed("hover-") | framed("focus-") | framed("focusframe-")
                             | {"hint-focus-over-base", "base-hint-compose-over-border"}),
    "widgets/checkmarks.svg": {"checkbox", "radiobutton"},
    "widgets/radiobutton.svg": {"hint-size", "normal", "shadow", "checked", "hover", "focus", "symbol"},
    "widgets/switch.svg": {"hint-bar-size", "inactive-left", "inactive-center", "inactive-right", "active-left",
                           "active-center", "active-right", "handle", "handle-active", "handle-hover",
                           "handle-pressed", "handle-focus", "handle-shadow", "shadow"},
    "widgets/slider.svg": (framed("groove-") | framed("groove-highlight-") | {"hint-handle-size"}
                           | {"%s-slider-%s" % (o, s) for o in ("horizontal", "vertical")
                              for s in ("handle", "hover", "focus", "shadow")}),
    "widgets/bar_meter_horizontal.svg": {"hint-bar-size"} | framed("bar-inactive-") | framed("bar-active-"),
    "widgets/bar_meter_vertical.svg": {"hint-bar-size"} | framed("bar-inactive-") | framed("bar-active-"),
    "widgets/busywidget.svg": {"busywidget", "22-22-busywidget", "16-16-busywidget"},
    "widgets/scrollbar.svg": ({"hint-scrollbar-size"} | framed("background-vertical-", "inset")
                              | framed("background-horizontal-", "inset") | margins("background-vertical-")
                              | margins("background-horizontal-") | framed("slider-", "inset")
                              | framed("mouseover-slider-", "inset")),
    "widgets/scrollwidget.svg": frame("border-"),
    "widgets/viewitem.svg": (framed("normal-") | framed("hover-") | framed("selected-") | framed("selected+hover-")),
    "widgets/listitem.svg": (framed("normal-") | framed("hover-") | framed("pressed-") | framed("section-")
                             | {"separator"}),
    "widgets/tabbar.svg": set().union(*(framed("%s-active-tab-" % e) for e in ("north", "south", "east", "west"))),
    "widgets/menubaritem.svg": framed("normal-") | framed("hover-") | framed("pressed-"),
    "widgets/toolbar.svg": framed(""),
    "widgets/pager.svg": frame("normal-") | frame("hover-") | frame("active-"),
    "widgets/monitor.svg": frame(""),
    "widgets/picker.svg": framed("") | {"hint-compose-over-border"},
    "widgets/dragger.svg": (framed("") | {"background-vertical-topleft", "background-vertical-top",
                                          "background-vertical-topright", "hint-preferred-icon-size"}),
    "widgets/glowbar.svg": frame("") | {"hint-glow-radius"},
    "widgets/line.svg": {"horizontal-line", "vertical-line"},
    "widgets/margins-highlight.svg": {"fill", "topleft", "topright", "bottomleft", "bottomright"},
    "widgets/plot-background.svg": set(),
    "widgets/notes.svg": set(pc.NOTES),
    "widgets/calendar.svg": {"event"},
    "widgets/arrows.svg": {"up-arrow", "down-arrow", "left-arrow", "right-arrow"},
    "widgets/branding.svg": {"brilliant"},
    "widgets/clock.svg": {"ClockFace", "HourHand", "MinuteHand", "SecondHand", "HourHandShadow",
                          "MinuteHandShadow", "SecondHandShadow", "HandCenterScrew", "Glass",
                          "hint-hands-shadow-offset-to-south"},
    "widgets/analog_meter.svg": {"background", "foreground", "pointer", "pointer-shadow", "rotatecenter",
                                 "rotateminmax", "label0", "label1"},
    "widgets/configuration-icons.svg": {"menu", "configure", "rotate", "move", "size-vertical", "size-horizontal",
                                        "size-diagonal-tr2bl", "size-diagonal-tl2br", "maximize", "unmaximize",
                                        "status", "collapse", "return-to-source", "restore", "help", "delete",
                                        "add", "remove", "close", "showbackground"},
    "widgets/action-overlays.svg": {"%s-%s" % (k, s) for k in ("add", "remove", "open")
                                    for s in ("normal", "hover", "pressed")},
    "widgets/containment-controls.svg": ({"%s-%s" % (e, p) for e in ("south", "north") for p in ("center", "top", "bottom")}
                                         | {"%s-%s" % (e, p) for e in ("west", "east") for p in ("center", "left", "right")}
                                         | {"%s-%s" % (e, s) for e in ("south", "north", "west", "east")
                                            for s in ("minslider", "maxslider", "offsetslider")}
                                         | {"vertical-centerindicator", "horizontal-centerindicator"}),
    "widgets/media-delegate.svg": (framed("picture-") | framed("picture-selected-")
                                   | {"focus-left", "focus-topleft", "focus-bottomleft"}),
}

# the stock surfaces and the taskbar belong to art/plasmastyle.py and the shell
NOT_HERE = {"widgets/background.svg", "widgets/panel-background.svg", "widgets/tooltip.svg",
            "widgets/translucentbackground.svg", "widgets/frame.svg", "widgets/plasmoidheading.svg",
            "widgets/tasks.svg"}


def ids(text):
    return [n for n in re.findall(r'\bid="([^"]+)"', text)
            if n != "current-color-scheme" and not n.startswith("raye-gradient-")]


def boxes(text):
    """id -> (w, h) of every element: a hint's rect, or a group's sizer rect."""
    root = ET.fromstring(text)
    out = {}
    for node in root.iter():
        name = node.get("id")
        if not name:
            continue
        if node.tag == SVG_NS + "rect":
            out[name] = (float(node.get("width")), float(node.get("height")))
        elif node.tag == SVG_NS + "g":
            sizer = node.find(SVG_NS + "rect")
            out[name] = (float(sizer.get("width")), float(sizer.get("height")))
    return out


def test_every_file_is_listed():
    assert set(FILES) == set(REQUIRED)
    assert not set(FILES) & NOT_HERE


@pytest.mark.parametrize("path", sorted(FILES))
def test_files_are_current(path):
    assert path not in pc.stale(), "run python3 art/plasmastyle_controls.py"


@pytest.mark.parametrize("path", sorted(FILES))
def test_svg_parses_with_unique_names(path):
    root = ET.fromstring(FILES[path])
    assert root.tag == SVG_NS + "svg"
    names = ids(FILES[path])
    assert len(names) == len(set(names))


@pytest.mark.parametrize("path", sorted(FILES))
def test_plasma_names_and_nothing_else(path):
    names = set(ids(FILES[path]))
    missing = REQUIRED[path] - names
    assert not missing, sorted(missing)
    extra = names - REQUIRED[path] - {"hint-stretch-borders"}
    assert not extra, sorted(extra)


@pytest.mark.parametrize("path", sorted(FILES))
def test_colours_come_from_the_scheme(path):
    """Fills and strokes are scheme classes; fixed colours only where a
    colour cannot follow the scheme: shadows (black), the Raye glow and the
    sticky notes."""
    allowed = {"#000000", "#000", "#ff00ff", pc.GLOW} | set(pc.NOTES.values())
    text = FILES[path]
    body = text.split("</defs>", 1)[1]
    for colour in re.findall(r'(?:fill|stroke|stop-color)="(#[0-9a-fA-F]{3,6})"', body):
        assert colour.lower() in allowed, colour


def test_windows_sizes():
    b = {p: boxes(t) for p, t in FILES.items()}
    assert b["widgets/switch.svg"]["hint-bar-size"] == (40, 20)
    # the knob element is 20 x 20 with a 12 px knob: 4 px from the ends
    assert b["widgets/switch.svg"]["handle"] == (20, 20)
    assert b["widgets/radiobutton.svg"]["hint-size"] == (20, 20)
    assert b["widgets/radiobutton.svg"]["symbol"] == (12, 12)
    assert b["widgets/slider.svg"]["hint-handle-size"] == (20, 20)
    # a 4 px track: the groove's margins top + bottom
    groove = b["widgets/slider.svg"]
    assert groove["groove-hint-top-margin"][1] + groove["groove-hint-bottom-margin"][1] == 4
    assert b["widgets/bar_meter_horizontal.svg"]["hint-bar-size"][1] == 3
    assert b["widgets/bar_meter_vertical.svg"]["hint-bar-size"][0] == 3
    # the bar is 12 px across: the background's margins (PlasmaComponents
    # sizes the bar from them; the thumb hint is only its minimum)
    bar = b["widgets/scrollbar.svg"]
    assert bar["background-vertical-hint-left-margin"][0] + bar["background-vertical-hint-right-margin"][0] == 12
    assert bar["background-horizontal-hint-top-margin"][1] + bar["background-horizontal-hint-bottom-margin"][1] == 12
    # body text (18 px line) plus the button margins: 32 px
    button = b["widgets/button.svg"]
    assert 18 + button["normal-hint-top-margin"][1] + button["normal-hint-bottom-margin"][1] == 32
    assert button["normal-hint-left-margin"][0] == 11
    # 4 px corners: the corner tiles of buttons are 4 x 4
    assert button["normal-topleft"] == (4, 4)


def test_scroll_bar_widens_under_the_pointer():
    b = boxes(FILES["widgets/scrollbar.svg"])
    size = b["background-vertical-hint-left-margin"][0] + b["background-vertical-hint-right-margin"][0]
    rest = size - b["slider-hint-left-inset"][0] - b["slider-hint-right-inset"][0]
    hover = size - b["mouseover-slider-hint-left-inset"][0] - b["mouseover-slider-hint-right-inset"][0]
    assert rest == 2
    assert hover == 6


def test_selected_row_has_a_16_px_pill_in_a_32_px_row():
    """The pill lives in the stretched left piece: with 8 px corners it is
    32 - 2 x 8 = 16 px in a 32 px row."""
    b = boxes(FILES["widgets/viewitem.svg"])
    assert b["selected-topleft"] == (8, 8)
    assert 32 - b["selected-topleft"][1] - b["selected-bottomleft"][1] == 16
    assert "ColorScheme-Highlight" in re.search(r'<g id="selected-left">.*?</g>', FILES["widgets/viewitem.svg"]).group(0)
    assert "ColorScheme-Highlight" not in re.search(r'<g id="hover-left">.*?</g>', FILES["widgets/viewitem.svg"]).group(0)


def test_focused_field_draws_the_accent_line_at_the_bottom_only():
    text = FILES["widgets/lineedit.svg"]
    for piece in ("bottom", "bottomleft", "bottomright"):
        assert "ColorScheme-Highlight" in re.search(r'<g id="focus-%s">.*?</g>' % piece, text).group(0)
    for piece in ("top", "left", "right", "topleft"):
        assert "ColorScheme-Highlight" not in re.search(r'<g id="focus-%s">.*?</g>' % piece, text).group(0)


def test_clock_hands_turn_about_the_face_centre():
    """No rotation-centre hint: each hand turns about its element's centre,
    so the hands are as tall as the face."""
    b = boxes(FILES["widgets/clock.svg"])
    face = b["ClockFace"]
    for hand in ("HourHand", "MinuteHand", "SecondHand"):
        assert b[hand][1] == face[1]


def test_eye_is_raye_s():
    assert "image/png;base64," in FILES["widgets/branding.svg"]


def test_no_windows_or_other_brand_words():
    for path, text in FILES.items():
        for word in ("segoe", "fluent", "breeze", "microsoft"):
            assert word not in text.lower(), (path, word)
