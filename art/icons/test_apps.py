# Tests of the Raye app and category icons (art/icons/apps.py): every app
# of theming 3.8 (list 3) answers to the names its launcher asks for,
# every menu category of the Start menu has its tile, and no third-party
# brand is drawn over.
#
#   python3 -m pytest art/icons/test_apps.py

import configparser
import glob
import json
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import apps  # noqa: E402
import lib  # noqa: E402

THEMES = ("Raye", "Raye-Dark")
REPO = lib.ROOT
SRV = os.path.dirname(REPO)

# theming 3.8 list 3, by the name each app's launcher uses
WANTED_APPS = {
    "system-file-manager", "org.kde.dolphin", "utilities-terminal", "org.kde.konsole",
    "accessories-text-editor", "org.kde.kate", "accessories-calculator", "org.kde.kcalc",
    "accessories-screenshot", "org.kde.spectacle", "org.kde.gwenview", "org.kde.okular",
    "org.kde.haruna", "org.kde.elisa", "org.kde.ark", "org.kde.partitionmanager",
    "org.kde.plasma-systemmonitor", "preferences-system", "systemsettings", "help-browser",
    "org.kde.khelpcenter", "plasmadiscover", "kwalletmanager", "printer",
    "system-software-install", "org.raye.store", "system-software-update", "org.raye.update",
    "raye-installer", "wine", "raye-windows-apps", "preferences-desktop-theme",
    "preferences-desktop-wallpaper", "preferences-desktop-accessibility", "org.kde.ksystemlog",
    # the "Open with" dialog's window
    "kde", "raye-app-chooser",
}

# brands that keep their own pictures
BRANDS = ("firefox", "steam", "chrome", "chromium", "discord", "spotify", "thunderbird",
          "libreoffice", "vlc", "gimp", "obs", "telegram")


def names(theme: str, ctx: str, size: str = "48") -> set[str]:
    return {fn[:-4] for fn in os.listdir(os.path.join(lib.OUT, theme, ctx, size))}


@pytest.mark.parametrize("theme", THEMES)
def test_apps_at_every_size(theme):
    for sd in lib.size_dirs():
        missing = WANTED_APPS - names(theme, "apps", sd)
        assert not missing, (theme, sd, sorted(missing))


def _start_menu_categories() -> set[str]:
    out = set()
    for p in glob.glob(os.path.join(REPO, "shell", "menus", "desktop-directories", "*.directory")):
        cp = configparser.ConfigParser(interpolation=None)
        cp.optionxform = str
        cp.read(p)
        out.add(cp["Desktop Entry"]["Icon"])
    return out


@pytest.mark.parametrize("theme", THEMES)
def test_start_menu_categories_have_tiles(theme):
    wanted = _start_menu_categories() | set(apps.CATEGORIES)
    assert wanted, "no Start menu category found"
    for sd in lib.size_dirs():
        missing = wanted - names(theme, "categories", sd)
        assert not missing, (theme, sd, sorted(missing))


def test_launchers_of_raye_repos_find_an_icon():
    """Icon= of the Raye launchers that name a themed icon get a Raye one."""
    themed = names("Raye", "apps") | names("Raye", "categories") | names("Raye", "places")
    checked = 0
    for repo in ("settings", "store", "installer", "iso/packages/raye-base/release"):
        for p in glob.glob(os.path.join(SRV, repo, "**", "*.desktop"), recursive=True):
            rel = os.path.relpath(p, SRV).split(os.sep)
            if "tests" in rel or "build" in rel or "debian" in rel:
                continue
            for line in open(p, encoding="utf-8"):
                if line.startswith("Icon="):
                    icon = line.strip().split("=", 1)[1]
                    if icon in ("preferences-system", "system-software-install", "wine",
                                "system-file-manager", "system-software-update"):
                        assert icon in themed, (p, icon)
                        checked += 1
    if checked == 0:
        pytest.skip("no Raye repo next to this one")


def test_aliases_point_at_drawings():
    for alias, target in apps.APP_ALIASES.items():
        assert target in apps.APPS, (alias, target)
        assert alias not in apps.APPS, alias
    for alias, target in apps.CATEGORY_ALIASES.items():
        assert target in apps.CATEGORIES, (alias, target)


def test_no_name_drawn_by_another_set():
    mine = set(apps.APPS) | set(apps.APP_ALIASES) | set(apps.CATEGORIES) | set(apps.CATEGORY_ALIASES)
    others = set()
    for st in lib.load_sets():
        if st is apps.ICONS:
            continue
        for icons in list(st.icons.values()) + list(st.aliases.values()):
            others |= set(icons)
    assert not (mine & others), sorted(mine & others)


def test_no_brand_is_drawn_over():
    mine = set(apps.APPS) | set(apps.APP_ALIASES)
    for name in mine:
        assert not any(b in name.lower() for b in BRANDS), name


def test_small_sizes_are_drawn_not_scaled():
    # the 16 px drawing of a tile icon leaves out the eye the 48 px one has
    small = apps.settings(16).text()
    big = apps.settings(48).text()
    assert small.count("<path") < big.count("<path")
    assert 'viewBox="0 0 16 16"' in small


# ------------------------------------------------- one style in Start

def _shapes(name: str, n: int = 48) -> str:
    return apps.APPS[name](n).text()


@pytest.mark.parametrize("name", sorted(apps.APPS))
def test_every_app_sits_on_the_same_tile(name):
    """Start and the taskbar show these side by side: same outline, same
    size, same light. Only the colour and the white symbol change."""
    for n in (16, 22, 48):
        s, m, size = apps.tile(n)
        outline = lib.rrect(m, m, size, size, size * 0.24)
        assert f'd="{outline}"' in _shapes(name, n), (name, n)


@pytest.mark.parametrize("name", sorted(apps.APPS))
def test_every_app_symbol_is_white(name):
    """A symbol in its own colours is the mixed look this set replaces."""
    text = _shapes(name)
    body = text.split("<g transform", 1)
    assert len(body) == 2, name
    for colour in ("#3A3350", "#5A5270", "#F2EFF8", "#C9C2D8", "#F1ECFA"):
        assert colour not in body[1], (name, colour)


def test_the_set_keeps_apps_apart_by_colour():
    """Same tile for everyone would make them hard to tell apart."""
    tones = {apps.TONE_OF[name] for name in apps.TONE_OF}
    assert len(tones) >= 6
    assert set(apps.TONE_OF) <= set(apps.APPS)


# ------------------------------------------------- names and pins

IDENTITY = os.path.join(SRV, "apps", "identity", "identity.json")
SHELLRC = os.path.join(REPO, "shell", "lib", "rayeshellrc")


def _identity() -> dict:
    if not os.path.exists(IDENTITY):
        pytest.skip("the apps repo is not next to this one")
    with open(IDENTITY, encoding="utf-8") as fh:
        return json.load(fh)["apps"]


def _default_pins() -> list[str]:
    """[Start] Pinned=, the tiles of Start. The taskbar has a Pinned= too."""
    cp = configparser.ConfigParser(interpolation=None, strict=False)
    cp.optionxform = str
    cp.read(SHELLRC, encoding="utf-8")
    return cp["Start"]["Pinned"].split(",")


@pytest.mark.parametrize("theme", THEMES)
def test_every_renamed_app_has_a_raye_icon(theme):
    themed = names(theme, "apps")
    for desktop_id, app in _identity().items():
        if "icon" in app:
            assert app["icon"] in themed, (desktop_id, app["icon"])


def test_pins_never_show_an_upstream_name_or_a_hidden_app():
    table = _identity()
    for pin in _default_pins():
        if not pin.startswith("org.kde.") or pin == "org.kde.dolphin.desktop":
            continue
        assert pin in table, f"{pin} would show its stock name in Start"
        assert "name" in table[pin], pin
        assert not table[pin].get("hidden"), f"{pin} is pinned and hidden from Start"
