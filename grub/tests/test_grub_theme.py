"""The GRUB theme of an installed Raye (grub/raye) and the script that
loads its fonts (grub/06_raye_fonts). Needs only sh, and grub-mkfont with
fonts-inter for the font names (both build dependencies)."""

import os
import re
import shlex
import shutil
import struct
import subprocess

import pytest

GRUB = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
THEME = os.path.join(GRUB, "raye")
REPO = os.path.dirname(GRUB)
FONT_SCRIPT = os.path.join(GRUB, "06_raye_fonts")
INTER = "/usr/share/fonts/opentype/inter"


def theme_text() -> str:
    with open(os.path.join(THEME, "theme.txt"), encoding="utf-8") as f:
        return f.read()


def png_header(path: str) -> tuple:
    """(width, height, bit depth, colour type) from the IHDR chunk."""
    with open(path, "rb") as f:
        head = f.read(26)
    assert head[:8] == b"\x89PNG\r\n\x1a\n", path
    return struct.unpack(">IIBB", head[16:26])


def pf2_name(path: str) -> str:
    with open(path, "rb") as f:
        data = f.read()
    pos = 12  # FILE section: "FILE", length 4, "PFF2"
    while pos < len(data):
        tag, size = data[pos:pos + 4], struct.unpack(">I", data[pos + 4:pos + 8])[0]
        if tag == b"NAME":
            return data[pos + 8:pos + 8 + size].rstrip(b"\0").decode()
        pos += 8 + size
    raise AssertionError(f"{path} has no NAME")


def rules_fonts() -> list:
    """The grub-mkfont command lines of debian/rules, as argument lists."""
    with open(os.path.join(REPO, "debian", "rules"), encoding="utf-8") as f:
        lines = [l.strip() for l in f if l.strip().startswith("grub-mkfont")]
    assert lines
    return [shlex.split(l.replace("$(INTER)", INTER)) for l in lines]


def test_every_picture_named_by_the_theme_exists():
    text = theme_text()
    for name in re.findall(r'(?:desktop-image|file)\s*[:=]\s*"([^"]+)"', text):
        assert os.path.isfile(os.path.join(THEME, name)), name
    for style in re.findall(r'(?:pixmap_style|bar_style|highlight_style)\s*=\s*"([^"]+)"', text):
        # a style needs at least its centre piece
        assert os.path.isfile(os.path.join(THEME, style.replace("*", "c"))), style


def test_pictures_are_ones_grub_can_read():
    # GRUB's PNG reader takes 8-bit RGB and RGBA only: a palette picture
    # (what an optimiser makes of a flat icon) is skipped without a word
    for top, _, files in os.walk(THEME):
        for f in files:
            if f.endswith(".png"):
                _, _, depth, colour = png_header(os.path.join(top, f))
                assert depth == 8 and colour in (2, 6), f


def test_icons_have_the_size_the_menu_gives_them():
    text = theme_text()
    w = int(re.search(r"icon_width\s*=\s*(\d+)", text).group(1))
    h = int(re.search(r"icon_height\s*=\s*(\d+)", text).group(1))
    icons = os.path.join(THEME, "icons")
    # update-grub writes --class raye for Raye, --class os for the others
    assert {"raye.png", "os.png"} <= set(os.listdir(icons))
    for f in os.listdir(icons):
        assert png_header(os.path.join(icons, f))[:2] == (w, h), f


def test_no_font_sits_in_the_theme_folder_itself():
    # update-grub loads those without condition: an error line per font
    # at every boot under Secure Boot
    assert not [f for f in os.listdir(THEME) if f.endswith(".pf2")]
    for args in rules_fonts():
        out = args[args.index("-o") + 1]
        assert os.path.dirname(out) == "grub/raye/fonts", out


def test_the_editor_keeps_the_built_in_font():
    assert re.search(r'terminal-font:\s*"GNU Unifont Regular 16"', theme_text())


@pytest.mark.skipif(not shutil.which("grub-mkfont") or not os.path.isdir(INTER),
                    reason="needs grub-mkfont and fonts-inter")
def test_fonts_built_are_the_ones_the_theme_names(tmp_path):
    names = []
    for args in rules_fonts():
        i = args.index("-o")
        args[i + 1] = str(tmp_path / os.path.basename(args[i + 1]))
        subprocess.run(args, check=True, capture_output=True)
        names.append(pf2_name(args[i + 1]))
    assert len(set(names)) == len(names), names
    for font in re.findall(r'item_font\s*=\s*"([^"]+)"', theme_text()):
        assert font in names, font


LIB = """
grub_probe=fake_probe
fake_probe() { echo /dev/fake; }
is_path_readable_by_grub() { return 0; }
prepare_grub_to_access_device() { echo "set root='fake'"; }
make_system_path_relative_to_its_root() { echo "$1"; }
"""


def run_font_script(tmp_path, fonts: list) -> str:
    (tmp_path / "grub-mkconfig_lib").write_text(LIB)
    theme = tmp_path / "boot" / "grub" / "themes" / "raye"
    (theme / "fonts").mkdir(parents=True)
    (theme / "theme.txt").write_text("")
    for f in fonts:
        (theme / "fonts" / f).write_bytes(b"")
    env = {"PATH": os.environ["PATH"], "pkgdatadir": str(tmp_path),
           "GRUB_THEME": str(theme / "theme.txt")}
    return subprocess.run(["sh", FONT_SCRIPT], env=env, check=True,
                          capture_output=True, text=True).stdout


def test_fonts_are_only_loaded_without_secure_boot(tmp_path):
    out = run_font_script(tmp_path, ["inter-regular-20.pf2", "inter-semibold-20.pf2"])
    lines = out.splitlines()
    guard = lines.index("if [ x$lockdown != xy ]; then")
    loads = [l.strip() for l in lines[guard + 1:]]
    assert loads[-1] == "fi"
    assert [l.rsplit("/", 1)[1] for l in loads[:-1]] == [
        "inter-regular-20.pf2", "inter-semibold-20.pf2"]
    assert all(l.startswith("loadfont ($root)") for l in loads[:-1])
    assert not [l for l in lines[:guard] if "loadfont" in l]


def test_nothing_is_written_without_fonts(tmp_path):
    assert run_font_script(tmp_path, []) == ""


def test_the_font_script_is_executable():
    # update-grub skips a /etc/grub.d script that can't be run
    assert os.access(FONT_SCRIPT, os.X_OK)
