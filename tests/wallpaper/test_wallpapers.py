# The Raye wallpapers: drawn by art/wallpapers.py, committed, installed by
# raye-theme. At least six, dark and light, none of them a violet haze.
import json
import pathlib
import re

import pytest
from PIL import Image

REPO = pathlib.Path(__file__).resolve().parents[2]
WALL = REPO / "wallpaper"
INSTALL = (REPO / "debian" / "raye-theme.install").read_text(encoding="utf-8")
FIXED = sorted(p for p in WALL.iterdir()
               if p.name.startswith("Raye") and (p / "contents" / "images").is_dir())


def pictures(pkg):
    return sorted((pkg / "contents" / "images").iterdir())


def brightness(img):
    return sum(img.convert("L").resize((64, 36)).getdata()) / (64 * 36)


def test_there_are_six_or_more():
    assert len(FIXED) >= 6


def test_dark_and_light_ones():
    big = [Image.open(pictures(p)[-1]) for p in FIXED]
    dark = [b for b in big if brightness(b) < 70]
    light = [b for b in big if brightness(b) > 180]
    assert len(dark) >= 3 and len(light) >= 3


@pytest.mark.parametrize("pkg", FIXED, ids=lambda p: p.name)
def test_4k_and_1080p(pkg):
    sizes = {Image.open(f).size for f in pictures(pkg)}
    assert {(3840, 2160), (1920, 1080)} <= sizes


@pytest.mark.parametrize("pkg", FIXED, ids=lambda p: p.name)
def test_named_in_both_languages(pkg):
    meta = json.loads((pkg / "metadata.json").read_text(encoding="utf-8"))["KPlugin"]
    assert meta["Id"] == pkg.name
    assert meta["Name"] and meta["Name[fr]"]


@pytest.mark.parametrize("pkg", FIXED, ids=lambda p: p.name)
def test_installed(pkg):
    assert re.search(r"^wallpaper/%s usr/share/wallpapers$" % re.escape(pkg.name), INSTALL, re.M)


@pytest.mark.parametrize("pkg", FIXED, ids=lambda p: p.name)
def test_violet_is_a_detail(pkg):
    """Saturated violet on at most 3 % of the picture: a line, not a haze."""
    img = Image.open(pictures(pkg)[0]).convert("HSV").resize((480, 270))
    violet = sum(1 for h, s, v in img.getdata() if 180 <= h <= 215 and s > 60 and v > 60)
    assert violet / (480 * 270) <= 0.03


def test_default_keeps_the_names_the_sign_in_screen_reads():
    names = {f.name for f in pictures(WALL / "Raye")}
    assert {"3840x2160.png", "1920x1080.png"} <= names
