# The text editor themes (colors/editor/): current, picked by KTextEditor
# for the Raye colour schemes, and readable.
#   python3 -m pytest -q -p no:cacheprovider tests/apps/

import importlib.util
import json
import os

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
spec = importlib.util.spec_from_file_location("editor", os.path.join(ROOT, "colors", "editor.py"))
editor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(editor)

FILES = {"night": "colors/editor/RayeNight.theme", "day": "colors/editor/RayeDay.theme"}

# Every colour role KSyntaxHighlighting 6 knows; a theme missing one gets
# the role's built-in default, which is Breeze's.
EDITOR_ROLES = {
    "BackgroundColor", "BracketMatching", "CodeFolding", "CurrentLine",
    "CurrentLineNumber", "IconBorder", "IndentationLine", "LineNumbers",
    "MarkBookmark", "MarkBreakpointActive", "MarkBreakpointDisabled",
    "MarkBreakpointReached", "MarkError", "MarkExecution", "MarkWarning",
    "ModifiedLines", "ReplaceHighlight", "SavedLines", "SearchHighlight",
    "Separator", "SpellChecking", "TabMarker", "TemplateBackground",
    "TemplateFocusedPlaceholder", "TemplatePlaceholder",
    "TemplateReadOnlyPlaceholder", "TextSelection", "WordWrapMarker",
}
TEXT_ROLES = {
    "Normal", "Keyword", "Function", "Variable", "ControlFlow", "Operator",
    "BuiltIn", "Extension", "Preprocessor", "Attribute", "Char",
    "SpecialChar", "String", "VerbatimString", "SpecialString", "Import",
    "DataType", "DecVal", "BaseN", "Float", "Constant", "Comment",
    "Documentation", "Annotation", "CommentVar", "RegionMarker",
    "Information", "Warning", "Alert", "Error", "Others",
}


def load(mode):
    with open(os.path.join(ROOT, FILES[mode]), encoding="utf-8") as f:
        return json.load(f)


def rgb_hex(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def contrast(a, b):
    def lum(c):
        def lin(v):
            v /= 255
            return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
        r, g, b_ = (lin(x) for x in c)
        return 0.2126 * r + 0.7152 * g + 0.0722 * b_
    hi, lo = sorted((lum(a), lum(b)), reverse=True)
    return (hi + 0.05) / (lo + 0.05)


def test_outputs_are_current():
    assert editor.stale() == []


def test_the_files_are_the_ones_the_generator_names():
    assert sorted(editor.outputs()) == sorted(FILES.values())


@pytest.mark.parametrize("mode", FILES)
def test_background_is_the_schemes_text_area(mode):
    # KTextEditor picks the theme whose background is the palette's Base
    # colour, which Qt takes from View BackgroundNormal. Off by one value
    # and Kate falls back to Breeze.
    scheme = editor.read_ini(editor.MODES[mode][1])
    base = editor.rgb(scheme["Colors:View"]["BackgroundNormal"])
    assert rgb_hex(load(mode)["editor-colors"]["BackgroundColor"]) == base


@pytest.mark.parametrize("mode", FILES)
def test_selection_is_the_schemes_selection(mode):
    scheme = editor.read_ini(editor.MODES[mode][1])
    sel = editor.rgb(scheme["Colors:Selection"]["BackgroundNormal"])
    assert rgb_hex(load(mode)["editor-colors"]["TextSelection"]) == sel


def test_the_two_backgrounds_differ():
    assert (load("night")["editor-colors"]["BackgroundColor"]
            != load("day")["editor-colors"]["BackgroundColor"])


@pytest.mark.parametrize("mode", FILES)
def test_every_role_is_set(mode):
    data = load(mode)
    assert set(data["editor-colors"]) == EDITOR_ROLES
    assert set(data["text-styles"]) == TEXT_ROLES


@pytest.mark.parametrize("mode", FILES)
def test_names_are_raye(mode):
    assert load(mode)["metadata"]["name"] == editor.MODES[mode][0]


@pytest.mark.parametrize("mode", FILES)
def test_text_reads_on_the_background(mode):
    data = load(mode)
    bg = rgb_hex(data["editor-colors"]["BackgroundColor"])
    for name, style in data["text-styles"].items():
        need = 3.0 if name in ("Comment", "Documentation") else 4.5
        ratio = contrast(rgb_hex(style["text-color"]), bg)
        assert ratio >= need, "%s %s: %.2f" % (mode, name, ratio)


@pytest.mark.parametrize("mode", FILES)
def test_selected_text_reads_on_the_selection(mode):
    data = load(mode)
    sel = rgb_hex(data["editor-colors"]["TextSelection"])
    for name, style in data["text-styles"].items():
        assert contrast(rgb_hex(style["selected-text-color"]), sel) >= 4.5, name


@pytest.mark.parametrize("mode", FILES)
def test_line_numbers_read_on_the_background(mode):
    colours = load(mode)["editor-colors"]
    bg = rgb_hex(colours["BackgroundColor"])
    assert contrast(rgb_hex(colours["LineNumbers"]), bg) >= 3.0
    assert contrast(rgb_hex(colours["CurrentLineNumber"]), bg) >= 4.5
