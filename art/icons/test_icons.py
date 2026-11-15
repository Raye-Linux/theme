# Tests of the Raye icon themes: the files committed in icons/ are the
# ones art/icons/lib.py draws, every name of theming 3.8 is there at
# every size, and the theme files are what icon loaders expect.
#
#   python3 -m pytest art/icons/test_icons.py

import configparser
import filecmp
import os
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lib  # noqa: E402

THEMES = ("Raye", "Raye-Dark")

# theming 3.8, lists 1 and 2
PLACES = {
    "folder", "folder-open", "user-home", "user-desktop", "folder-documents",
    "folder-download", "folder-music", "folder-pictures", "folder-videos",
    "folder-templates", "folder-publicshare", "folder-games", "folder-cloud",
    "folder-network", "folder-remote", "folder-recent", "starred",
    "user-trash", "user-trash-full", "network-workgroup", "folder-windows",
    "folder-encrypted", "folder-locked", "bookmarks",
}
DEVICES = {
    "computer", "drive-harddisk", "drive-harddisk-solidstate",
    "drive-removable-media-usb", "media-optical", "media-flash-sd-mmc",
    "drive-harddisk-system", "phone",
}
MIMETYPES = {
    "text-plain", "text-x-generic", "application-pdf", "image-x-generic",
    "image-png", "image-jpeg", "image-svg+xml", "video-x-generic",
    "audio-x-generic", "application-zip", "application-x-tar",
    "application-x-7z-compressed", "application-vnd.oasis.opendocument.text",
    "application-vnd.openxmlformats-officedocument.wordprocessingml.document",
    "application-vnd.oasis.opendocument.spreadsheet",
    "application-vnd.openxmlformats-officedocument.spreadsheetml.sheet",
    "application-vnd.oasis.opendocument.presentation",
    "application-vnd.openxmlformats-officedocument.presentationml.presentation",
    "text-html", "text-x-script", "application-x-executable",
    "application-x-ms-dos-executable", "application-x-msi",
    "application-x-msdownload", "application-x-shellscript",
    "application-x-iso9660-image", "application-x-deb", "font-x-generic",
    "unknown", "inode-directory",
}
WANTED = {"places": PLACES, "devices": DEVICES, "mimetypes": MIMETYPES}


def theme_dir(theme: str) -> str:
    return os.path.join(lib.OUT, theme)


def read_index(theme: str) -> configparser.ConfigParser:
    cp = configparser.ConfigParser(interpolation=None)
    cp.optionxform = str
    cp.read(os.path.join(theme_dir(theme), "index.theme"))
    return cp


@pytest.mark.parametrize("theme", THEMES)
def test_index_theme(theme):
    cp = read_index(theme)
    head = cp["Icon Theme"]
    assert head["Name"] and head["Name[fr]"] and head["Comment[fr]"]
    assert head["Inherits"] == lib.THEMES[theme]["inherits"]
    assert head["Inherits"].split(",")[-1] == "hicolor"
    dirs = head["Directories"].split(",")
    assert dirs == sorted(set(dirs))
    for d in dirs:
        assert os.path.isdir(os.path.join(theme_dir(theme), d)), d
        sec = cp[d]
        assert sec["Context"] in lib.CONTEXTS.values()
        if d.endswith("/scalable"):
            assert sec["Type"] == "Scalable" and int(sec["MaxSize"]) >= int(sec["MinSize"])
        else:
            assert sec["Type"] == "Fixed" and sec["Size"] == d.split("/")[1]
    # every folder that holds icons is listed
    for ctx in lib.CONTEXTS:
        for sd in lib.size_dirs():
            if os.path.isdir(os.path.join(theme_dir(theme), ctx, sd)):
                assert f"{ctx}/{sd}" in dirs


def test_dark_inherits_dark_parents():
    assert "breeze-dark" in read_index("Raye-Dark")["Icon Theme"]["Inherits"]
    assert "breeze-dark" not in read_index("Raye")["Icon Theme"]["Inherits"]


@pytest.mark.parametrize("theme", THEMES)
def test_every_name_at_every_size(theme):
    for ctx, wanted in WANTED.items():
        for sd in lib.size_dirs():
            d = os.path.join(theme_dir(theme), ctx, sd)
            have = {f[:-4] for f in os.listdir(d)}
            missing = wanted - have
            assert not missing, (theme, ctx, sd, missing)


@pytest.mark.parametrize("theme", THEMES)
def test_links_stay_in_the_tree_and_resolve(theme):
    top = os.path.realpath(lib.OUT)
    for dirpath, _, files in os.walk(theme_dir(theme)):
        for fn in files:
            p = os.path.join(dirpath, fn)
            if os.path.islink(p):
                target = os.readlink(p)
                assert not os.path.isabs(target), p
                real = os.path.realpath(p)
                assert real.startswith(top + os.sep), p
                assert os.path.isfile(real), p


def test_dark_theme_links_to_light_unless_redrawn():
    sets = lib.load_sets()
    for st in sets:
        for ctx, icons in st.icons.items():
            for name in icons:
                for sd in lib.size_dirs():
                    p = os.path.join(theme_dir("Raye-Dark"), ctx, sd, name + ".svg")
                    if name in st.dark.get(ctx, {}):
                        assert not os.path.islink(p)
                    else:
                        assert os.readlink(p) == f"../../../Raye/{ctx}/{sd}/{name}.svg"


def _drawn_files():
    for dirpath, _, files in os.walk(theme_dir("Raye")):
        for fn in files:
            p = os.path.join(dirpath, fn)
            if fn.endswith(".svg") and not os.path.islink(p):
                yield p


def test_svg_files_are_clean():
    count = 0
    for p in _drawn_files():
        count += 1
        size_dir = os.path.basename(os.path.dirname(p))
        n = lib.px_of(size_dir)
        root = ET.parse(p).getroot()
        assert root.get("viewBox") == f"0 0 {n} {n}", p
        text = open(p).read()
        # nothing a loader would fetch, no text that depends on fonts
        assert "href" not in text and "<text" not in text and "<image" not in text, p
    assert count > 100


def test_committed_files_are_current(tmp_path):
    lib.write(str(tmp_path))
    for theme in THEMES:
        cmp = filecmp.dircmp(os.path.join(tmp_path, theme), theme_dir(theme))
        _assert_same(cmp, theme)
        for dirpath, _, files in os.walk(os.path.join(tmp_path, theme)):
            for fn in files:
                new = os.path.join(dirpath, fn)
                old = os.path.join(theme_dir(theme), os.path.relpath(new, os.path.join(tmp_path, theme)))
                assert os.path.islink(new) == os.path.islink(old), old
                if os.path.islink(new):
                    assert os.readlink(new) == os.readlink(old), old


def _assert_same(cmp: filecmp.dircmp, where: str) -> None:
    assert not cmp.left_only, (where, "missing, run art/icons/lib.py", cmp.left_only)
    # cursors/ installs in the same folder but is not in this tree
    assert not cmp.right_only, (where, "not drawn by the code", cmp.right_only)
    assert not cmp.diff_files, (where, "changed, run art/icons/lib.py", cmp.diff_files)
    for sub, c in cmp.subdirs.items():
        _assert_same(c, f"{where}/{sub}")


def test_small_sizes_are_drawn_on_the_pixel_grid():
    # the folder outline of the 16 px drawing sits on whole pixels
    b = lib.folder_box(16)
    for v in (b.left, b.right, b.tab_top, b.back_top, b.front_top, b.bottom):
        assert v == int(v)
    assert lib.half(3.2) == 3.5 and lib.snap(3.6) == 4.0


@pytest.mark.skipif(not shutil.which("gtk-update-icon-cache"), reason="needs gtk-update-icon-cache")
@pytest.mark.parametrize("theme", THEMES)
def test_icon_cache_builds_clean(theme, tmp_path):
    dest = tmp_path / theme
    shutil.copytree(theme_dir(theme), dest, symlinks=True)
    # the dark theme links to the light one next to it
    if theme == "Raye-Dark":
        shutil.copytree(theme_dir("Raye"), tmp_path / "Raye", symlinks=True)
    r = subprocess.run(["gtk-update-icon-cache", "--force", "--quiet", str(dest)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    assert not r.stderr.strip(), r.stderr
    assert (dest / "icon-theme.cache").stat().st_size > 1000


@pytest.mark.parametrize("theme", THEMES)
@pytest.mark.parametrize("name", ("unknown", "application-octet-stream"))
def test_unknown_files_are_a_plain_page_not_a_question(theme, name):
    # A file type nobody knows is not an error to show the user. It is a
    # blank sheet: nothing but what page() draws at this size.
    blank = lib.page(48)[0].text()
    with open(os.path.join(theme_dir(theme), "mimetypes", "48", name + ".svg")) as f:
        assert f.read().count("<path") == blank.count("<path")


def _chroma(hex_colour):
    rgb = [int(hex_colour[i:i + 2], 16) for i in (1, 3, 5)]
    return max(rgb) - min(rgb)


def test_paper_and_metal_are_neutral():
    # same rule as the Raye Night and Day greys in tokens/test_palette.py
    greys = (*lib.PAPER, lib.PAPER_EDGE, *lib.GRAPHITE, *lib.METAL)
    assert [c for c in greys if _chroma(c) > 12] == []
    import places
    assert [c for c in places.SCREEN[:2] if _chroma(c) > 12] == []
