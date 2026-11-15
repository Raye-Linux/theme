#!/usr/bin/env python3
#
# Writes the text editor colours of Raye: colors/editor/RayeNight.theme
# and colors/editor/RayeDay.theme, KSyntaxHighlighting themes used by
# Kate, KWrite and every program that shows text with KTextEditor.
#
#   python3 colors/editor.py          write them
#   python3 colors/editor.py --check  list the files that are out of
#                                     date, exit 1 if there are any
#
# Nothing has to name these themes: KTextEditor picks, among the
# installed themes, the one whose background is the window's text area
# colour (View BackgroundNormal of the colour scheme). So Raye Night
# windows get Raye Night text and Raye Day windows get Raye Day text,
# and the editor follows when Settings switches between the two.
#
# Every colour comes from files the tokens already make: the colour
# schemes (colors/Raye*.colors) for the surfaces and the selection, and
# the terminal schemes (konsole/Raye*.colorscheme) for the syntax, so
# code reads the same in the editor and in the terminal. The output is
# the same byte for byte on every run.

import configparser
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODES = {
    "night": ("Raye Night", "colors/RayeNight.colors", "konsole/RayeNight.colorscheme"),
    "day": ("Raye Day", "colors/RayeDay.colors", "konsole/RayeDay.colorscheme"),
}
OUT = "colors/editor/%s.theme"


def read_ini(path):
    cp = configparser.ConfigParser(interpolation=None, strict=False)
    cp.optionxform = str
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        cp.read_file(f)
    return cp


def rgb(text):
    return tuple(int(v) for v in text.split(",")[:3])


def hexstr(c):
    return "#%02x%02x%02x" % tuple(max(0, min(255, round(v))) for v in c)


def mix(a, b, t):
    """a moved toward b by t (0 = a, 1 = b)."""
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def palette(mode):
    _, colors_path, konsole_path = MODES[mode]
    scheme = read_ini(colors_path)
    term = read_ini(konsole_path)
    view = scheme["Colors:View"]
    p = {
        "bg": rgb(view["BackgroundNormal"]),
        "bg2": rgb(view["BackgroundAlternate"]),
        "fg": rgb(view["ForegroundNormal"]),
        "fg2": rgb(view["ForegroundInactive"]),
        "accent": rgb(view["DecorationFocus"]),
        "negative": rgb(view["ForegroundNegative"]),
        "neutral": rgb(view["ForegroundNeutral"]),
        "positive": rgb(view["ForegroundPositive"]),
        "link": rgb(view["ForegroundLink"]),
        "sel": rgb(scheme["Colors:Selection"]["BackgroundNormal"]),
        "selfg": rgb(scheme["Colors:Selection"]["ForegroundNormal"]),
    }
    # The terminal's eight colours. On the dark scheme the bright set
    # reads best on the editor background; on the light one the normal
    # set is the one drawn for a light background.
    names = ("black", "red", "green", "yellow", "blue", "magenta", "cyan", "white")
    for i, name in enumerate(names):
        p[name] = rgb(term["Color%d" % i]["Color"])
        p[name + "+"] = rgb(term["Color%dIntense" % i]["Color"])
    if mode == "night":
        for name in names:
            p[name], p[name + "+"] = p[name + "+"], p[name]
    p["comment"] = mix(p["fg2"], p["bg"], 0.3)
    return p


# Syntax roles: colour name, then style flags. The names are those of
# palette(); a "+" picks the other shade of the same terminal colour.
STYLES = [
    ("Normal", "fg", ""),
    ("Keyword", "magenta", "b"),
    ("Function", "blue", ""),
    ("Variable", "cyan", ""),
    ("ControlFlow", "magenta", "b"),
    ("Operator", "fg", ""),
    ("BuiltIn", "magenta+", ""),
    ("Extension", "blue+", "b"),
    ("Preprocessor", "yellow+", ""),
    ("Attribute", "blue", ""),
    ("Char", "red", ""),
    ("SpecialChar", "red+", ""),
    ("String", "green", ""),
    ("VerbatimString", "green+", ""),
    ("SpecialString", "red", ""),
    ("Import", "cyan+", ""),
    ("DataType", "cyan", ""),
    ("DecVal", "yellow", ""),
    ("BaseN", "yellow", ""),
    ("Float", "yellow", ""),
    ("Constant", "yellow+", ""),
    ("Comment", "comment", "i"),
    ("Documentation", "comment", ""),
    ("Annotation", "magenta+", ""),
    ("CommentVar", "magenta+", ""),
    ("RegionMarker", "blue", ""),
    ("Information", "positive", ""),
    ("Warning", "neutral", ""),
    ("Alert", "negative", "b"),
    ("Error", "negative", "u"),
    ("Others", "green+", ""),
]


def text_styles(p):
    out = {}
    for name, colour, flags in STYLES:
        style = {
            "text-color": hexstr(p[colour]),
            "selected-text-color": hexstr(p["selfg"]),
            "bold": "b" in flags,
            "italic": "i" in flags,
            "underline": "u" in flags,
            "strike-through": False,
        }
        if name == "RegionMarker":
            style["background-color"] = hexstr(mix(p["bg"], p["blue"], 0.15))
        if name == "Alert":
            style["background-color"] = hexstr(mix(p["bg"], p["negative"], 0.18))
        out[name] = style
    return out


def editor_colours(p):
    bg, fg = p["bg"], p["fg"]
    return {
        "BackgroundColor": hexstr(bg),
        "BracketMatching": hexstr(mix(bg, p["accent"], 0.4)),
        "CodeFolding": hexstr(mix(bg, p["accent"], 0.2)),
        "CurrentLine": hexstr(p["bg2"]),
        "CurrentLineNumber": hexstr(fg),
        "IconBorder": hexstr(bg),
        "IndentationLine": hexstr(mix(bg, fg, 0.12)),
        "LineNumbers": hexstr(mix(p["fg2"], bg, 0.35)),
        "MarkBookmark": hexstr(p["accent"]),
        "MarkBreakpointActive": hexstr(p["negative"]),
        "MarkBreakpointDisabled": hexstr(mix(bg, p["negative"], 0.45)),
        "MarkBreakpointReached": hexstr(p["neutral"]),
        "MarkError": hexstr(p["negative"]),
        "MarkExecution": hexstr(p["fg2"]),
        "MarkWarning": hexstr(p["neutral"]),
        "ModifiedLines": hexstr(p["neutral"]),
        "ReplaceHighlight": hexstr(mix(bg, p["positive"], 0.35)),
        "SavedLines": hexstr(p["positive"]),
        "SearchHighlight": hexstr(mix(bg, p["neutral"], 0.35)),
        "Separator": hexstr(mix(bg, fg, 0.15)),
        "SpellChecking": hexstr(p["negative"]),
        "TabMarker": hexstr(mix(bg, fg, 0.2)),
        "TemplateBackground": hexstr(p["bg2"]),
        "TemplateFocusedPlaceholder": hexstr(mix(bg, p["accent"], 0.4)),
        "TemplatePlaceholder": hexstr(mix(bg, p["accent"], 0.22)),
        "TemplateReadOnlyPlaceholder": hexstr(mix(bg, p["negative"], 0.22)),
        "TextSelection": hexstr(p["sel"]),
        "WordWrapMarker": hexstr(mix(bg, fg, 0.12)),
    }


def theme(mode):
    p = palette(mode)
    data = {
        "metadata": {
            "name": MODES[mode][0],
            "revision": 1,
            "license": "SPDX-License-Identifier: CC-BY-SA-4.0",
            "copyright": ["SPDX-FileCopyrightText: Raye Linux"],
        },
        "editor-colors": editor_colours(p),
        "text-styles": text_styles(p),
    }
    return json.dumps(data, indent=4, sort_keys=True) + "\n"


def outputs():
    return {OUT % ("RayeNight" if m == "night" else "RayeDay"): theme(m) for m in MODES}


def stale():
    old = []
    for path, text in sorted(outputs().items()):
        try:
            with open(os.path.join(ROOT, path), encoding="utf-8") as f:
                current = f.read()
        except FileNotFoundError:
            current = None
        if current != text:
            old.append(path)
    return old


def main(argv):
    if "--check" in argv:
        old = stale()
        for path in old:
            print("out of date: %s" % path)
        return 1 if old else 0
    for path, text in sorted(outputs().items()):
        full = os.path.join(ROOT, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("wrote %s" % path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
