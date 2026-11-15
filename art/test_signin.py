# Tests of art/signin.py: the drawing rules and that the committed
# pictures in assets/signin are the ones the script draws.

import os
import sys
import xml.etree.ElementTree as ET

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import signin  # noqa: E402


def test_blur_scales_with_height():
    assert signin.blur_sigma(1080) == 32
    assert signin.blur_sigma(2160) == 64


def test_blurred_wallpaper_carries_the_sign_in_tint():
    from PIL import Image
    white = Image.new("RGB", (1920, 1080), (255, 255, 255))
    img = signin.blurred_wallpaper(white, 640)
    assert img.size == (640, 360)
    # 255 * 0.38 + night * 0.62, give or take the dither
    want = [255 * (1 - signin.SIGNIN_TINT) + c * signin.SIGNIN_TINT
            for c in signin.NIGHT]
    for x, y in ((0, 0), (320, 180), (639, 359)):
        for got, w in zip(img.getpixel((x, y)), want):
            assert abs(got - w) <= 3


def test_dither_is_small_and_fixed():
    a = signin.dither((300, 20))
    b = signin.dither((300, 20))
    assert a.tobytes() == b.tobytes()
    lo, hi = zip(*a.getextrema())
    assert min(lo) >= 0 and max(hi) <= 4


def test_number_format():
    assert signin.f(2.0) == "2"
    assert signin.f(2.50) == "2.5"
    assert signin.f(-0.001) == "0"
    assert signin.f(10.123) == "10.12"


def test_on_circle_zero_is_up_and_clockwise():
    x, y = signin.on_circle(10, 10, 5, 0)
    assert (round(x, 6), round(y, 6)) == (10, 5)
    x, y = signin.on_circle(10, 10, 5, 90)
    assert (round(x, 6), round(y, 6)) == (15, 10)


def test_every_icon_is_a_20px_svg_in_the_grid():
    icons = signin.icons()
    for level in signin.BATTERY_LEVELS:
        assert f"battery-{level:03d}.svg" in icons
        assert f"battery-{level:03d}-charging.svg" in icons
    for level in signin.WIFI_LEVELS:
        assert f"network-wireless-{level}.svg" in icons
    for name in ("power", "sleep", "restart", "accessibility", "keyboard",
                 "text-larger", "contrast", "network-wired", "network-none",
                 "eye-reveal", "eye-hide", "arrow-right"):
        assert f"{name}.svg" in icons
    for name, text in icons.items():
        root = ET.fromstring(text)
        assert root.get("viewBox") == "0 0 20 20", name
        assert root.get("width") == "20" and root.get("height") == "20", name
        assert "nan" not in text and "inf" not in text, name


def test_wifi_lights_one_more_part_per_level():
    for level in signin.WIFI_LEVELS:
        text = signin.icon_wifi(level)
        dim = text.count('opacity="0.3"')
        assert dim == 4 - level


def test_battery_turns_red_when_low_on_battery_only():
    assert signin.ERROR in signin.icon_battery(10)
    assert signin.ERROR in signin.icon_battery(0)
    assert signin.ERROR not in signin.icon_battery(20)
    assert signin.ERROR not in signin.icon_battery(10, charging=True)


def test_empty_battery_has_no_fill_and_full_one_does():
    assert signin.icon_battery(0).count("<path") == 2
    assert signin.icon_battery(100).count("<path") == 3


def test_face_is_square_and_dark_in_the_corners():
    img = signin.face(64)
    assert img.size == (64, 64) and img.mode == "RGB"
    r, g, b = img.getpixel((0, 0))
    assert max(r, g, b) < 60
    # the silhouette is the lightest thing in the picture
    r, g, b = img.getpixel((32, 25))
    assert min(r, g, b) > 180


def test_check_finds_missing_and_extra_files(tmp_path):
    out = tmp_path / "signin"
    signin.write(str(out))
    assert signin.stale(str(out)) == []
    (out / "icons" / "power.svg").write_text("<svg/>")
    (out / "stray.png").write_bytes(b"x")
    (out / "face-256.png").unlink()
    bad = signin.stale(str(out))
    assert "face-256.png" in bad
    assert os.path.join("icons", "power.svg") in bad
    assert any(b.startswith("stray.png") for b in bad)


@pytest.mark.skipif(not os.path.isdir(signin.OUT), reason="not drawn yet")
def test_committed_files_are_current():
    assert signin.stale() == []


def _xs(path_d):
    """x of every point of a path made of M/L/H/V/A commands (good enough
    for the simple shapes here: H gives x, V gives none, A ends on x y)."""
    import re
    xs = []
    for cmd, args in re.findall(r"([MLHVA])([^MLHVAZ]*)", path_d):
        nums = [float(n) for n in args.split()]
        if cmd in "ML":
            xs += nums[0::2]
        elif cmd == "H":
            xs += nums
        elif cmd == "A":
            xs += nums[5::7]
    return xs


def test_charging_bolt_stands_clear_of_the_terminal():
    for level in signin.BATTERY_LEVELS:
        root = ET.fromstring(signin.icon_battery(level, charging=True))
        paths = [p.get("d") for p in root.iter("{http://www.w3.org/2000/svg}path")]
        terminal, bolt = paths[1], paths[-1]
        assert min(_xs(bolt)) - max(_xs(terminal)) >= 0.5, level
        assert max(_xs(bolt)) <= signin.GRID - 0.5, level
