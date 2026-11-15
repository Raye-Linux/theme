# Tests of the action icons (art/icons/actions.py): the buttons of every
# app's toolbars and menus. They follow the colour scheme, so they stay
# readable on Raye Night and Raye Day.
#
#   python3 -m pytest art/icons/test_actions.py

import os
import sys
import xml.etree.ElementTree as ET

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lib  # noqa: E402

THEMES = ("Raye", "Raye-Dark")

# the buttons people meet first: toolbars of Files, Text Editor, Terminal,
# Screen Capture, Photos, and the common dialogs
WANTED = {
    "document-new", "document-open", "document-save", "document-save-as", "document-print",
    "document-properties", "document-share", "edit-copy", "edit-cut", "edit-paste", "edit-undo",
    "edit-redo", "edit-delete", "edit-find", "edit-clear", "edit-rename", "edit-select-all",
    "list-add", "list-remove", "go-previous", "go-next", "go-up", "go-down", "go-home",
    "view-refresh", "view-fullscreen", "view-list-details", "view-list-icons", "view-sort",
    "zoom-in", "zoom-out", "zoom-original", "zoom-fit-best", "configure", "settings-configure",
    "application-menu", "window-close", "window-minimize", "window-maximize", "window-restore",
    "dialog-ok", "dialog-cancel", "dialog-close", "help-about", "help-contents",
    "media-playback-start", "media-playback-pause", "media-playback-stop", "media-skip-forward",
    "media-skip-backward", "media-record", "process-stop", "system-search", "bookmark-new",
    "draw-freehand", "draw-rectangle", "draw-arrow", "draw-text", "draw-eraser", "transform-crop",
    "object-rotate-left", "object-rotate-right", "folder-new", "tab-new", "link", "lock", "unlock",
}


def path(theme, sd, name):
    return os.path.join(lib.OUT, theme, "actions", sd, name + ".svg")


@pytest.mark.parametrize("theme", THEMES)
def test_the_common_actions_exist_at_every_size(theme):
    for sd in lib.size_dirs():
        missing = sorted(n for n in WANTED if not os.path.exists(path(theme, sd, n)))
        assert not missing, (theme, sd, missing)


@pytest.mark.parametrize("theme", THEMES)
def test_every_action_answers_to_its_symbolic_name(theme):
    for name in WANTED:
        assert os.path.exists(path(theme, "16", name + "-symbolic")), name


@pytest.mark.parametrize("theme", THEMES)
def test_the_theme_declares_the_folder(theme):
    with open(os.path.join(lib.OUT, theme, "index.theme"), encoding="utf-8") as fh:
        text = fh.read()
    assert "[actions/16]" in text and "[actions/scalable]" in text
    assert "Context=Actions" in text


@pytest.mark.parametrize("theme", THEMES)
def test_actions_follow_the_colour_scheme(theme):
    """A fixed colour goes unreadable on one of the two themes."""
    for name in WANTED:
        text = open(path(theme, "22", name), encoding="utf-8").read()
        assert 'id="current-color-scheme"' in text, name
        assert 'currentColor' in text, name
        ET.fromstring(text)


def test_dark_and_light_have_their_own_ink():
    light = open(path("Raye", "22", "edit-copy"), encoding="utf-8").read()
    dark = open(path("Raye-Dark", "22", "edit-copy"), encoding="utf-8").read()
    assert light != dark


def test_no_two_actions_are_the_same_drawing():
    """A copy-paste drawing shows the wrong picture on a button."""
    seen = {}
    folder = os.path.join(lib.OUT, "Raye", "actions", "22")
    for fn in sorted(os.listdir(folder)):
        p = os.path.join(folder, fn)
        if os.path.islink(p):
            continue
        text = open(p, encoding="utf-8").read()
        assert text not in seen, (fn, seen.get(text))
        seen[text] = fn


# GTK fills every path of a symbolic icon and ignores strokes, so the
# buttons it draws in its own title bars (and Firefox's) must be shapes
WINDOW_BUTTONS = ("window-close", "window-minimize", "window-maximize", "window-restore")


@pytest.mark.parametrize("theme", THEMES)
def test_window_buttons_have_no_strokes(theme):
    for sd in lib.size_dirs():
        for name in WINDOW_BUTTONS:
            for n in (name, name + "-symbolic"):
                with open(path(theme, sd, n), encoding="utf-8") as fh:
                    text = fh.read()
                assert "stroke" not in text, (theme, sd, n)
                assert 'fill="currentColor"' in text, (theme, sd, n)
