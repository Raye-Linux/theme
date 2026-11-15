# The two global themes (Raye Night, Raye Day) and the system kdeglobals:
# what a new account starts with, and what Settings switches between.
#
#   python3 -m pytest -q tests/lookandfeel/

import json
import os
import re
import struct

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LNF = os.path.join(ROOT, "lookandfeel")
PACKAGES = {"night": "org.raye.night.desktop", "day": "org.raye.day.desktop"}
SCHEMES = {"night": "RayeNight", "day": "RayeDay"}
ICONS = {"night": "Raye-Dark", "day": "Raye"}


def parse(path):
    """KConfig as Plasma reads it: [a][b] group paths, # comments."""
    out, group = {}, None
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if line.startswith("["):
                assert line.endswith("]"), "%s:%d: %r" % (path, n, line)
                group = line
                assert group not in out, "%s:%d: %s twice" % (path, n, group)
                out[group] = {}
                continue
            assert group is not None, "%s:%d: key outside a group" % (path, n)
            key, sep, value = line.partition("=")
            assert sep, "%s:%d: %r" % (path, n, line)
            assert key not in out[group], "%s:%d: %s twice in %s" % (path, n, key, group)
            out[group][key] = value
    return out


def defaults(mode):
    return parse(os.path.join(LNF, PACKAGES[mode], "contents", "defaults"))


def metadata(mode):
    with open(os.path.join(LNF, PACKAGES[mode], "metadata.json"), encoding="utf-8") as f:
        return json.load(f)


KDEGLOBALS = parse(os.path.join(ROOT, "config", "kdeglobals"))


# --- the system kdeglobals ---------------------------------------------

def test_kdeglobals_starts_on_raye_night():
    kde = KDEGLOBALS["[KDE]"]
    assert kde["LookAndFeelPackage"] == PACKAGES["night"]
    assert kde["widgetStyle"] == "Raye"
    assert KDEGLOBALS["[General]"]["ColorScheme"] == "RayeNight"
    assert KDEGLOBALS["[Icons]"]["Theme"] == "Raye-Dark"
    assert KDEGLOBALS["[Sounds]"]["Theme"] == "raye"


def test_kdeglobals_names_the_day_and_night_pair():
    kde = KDEGLOBALS["[KDE]"]
    assert kde["DefaultDarkLookAndFeel"] == PACKAGES["night"]
    assert kde["DefaultLightLookAndFeel"] == PACKAGES["day"]
    # Settings offers "Change with the time of day"; it starts off
    assert kde["AutomaticLookAndFeel"] == "false"
    assert kde["AnimationDurationFactor"] == "1"


FONT = re.compile(r"^(?P<family>[^,]+),(?P<pt>\d+(\.\d+)?),-1,5,(?P<weight>\d+),0,0,0,0,0,0,0,0,0,0,1$")


@pytest.mark.parametrize("group,key,family,pt,weight", [
    ("[General]", "font", "Inter", 10, 400),
    ("[General]", "menuFont", "Inter", 10, 400),
    ("[General]", "toolBarFont", "Inter", 9, 400),
    ("[General]", "smallestReadableFont", "Inter", 9, 400),
    ("[General]", "fixed", "Cascadia Mono", 10, 400),
    ("[WM]", "activeFont", "Inter", 9, 500),
])
def test_fonts_are_the_type_ramp(group, key, family, pt, weight):
    m = FONT.match(KDEGLOBALS[group][key])
    assert m, (key, KDEGLOBALS[group][key])
    assert m["family"] == family
    assert float(m["pt"]) == pt
    assert int(m["weight"]) == weight


def test_kdeglobals_hides_the_kde_menus_of_stock_apps():
    want = {"action/help_about_app", "action/help_about_kde", "action/help_report_bug",
            "action/help_donate", "action/help_contents", "action/help_whats_this",
            "action/switch_application_language", "ghns"}
    restrictions = KDEGLOBALS["[KDE Action Restrictions]"]
    assert set(restrictions) == want
    assert set(restrictions.values()) == {"false"}


def test_kdeglobals_turns_transparency_on():
    # read by org.raye.ui's WindowEffects (MicaWindow), written by Settings
    assert KDEGLOBALS["[Raye]"] == {"TransparencyEffects": "true"}


def test_kdeglobals_default_apps_and_file_dialog():
    general = KDEGLOBALS["[General]"]
    assert general["BrowserApplication"] == "firefox.desktop"
    # konsole, not raye-terminal, and this test is why the second key
    # was not forgotten. Both values named a program nothing provides:
    # the Raye terminal is not written, and its stanza left
    # apps/debian/control on 2026-11-06 because the package shipped
    # empty. Every "Open Terminal Here" did nothing, silently. These two
    # are kept in step with what the image installs by
    # iso/tools/tests/test_staged_packages.py, which reads raye-base's
    # Depends; put the Raye values back the day apps declares the
    # package again, and that check will say so.
    assert general["TerminalService"] == "org.kde.konsole.desktop"
    assert general["TerminalApplication"] == "konsole"
    dialog = KDEGLOBALS["[KFileDialog Settings]"]
    assert dialog == {"View Style": "Detail", "Breadcrumb Navigation": "true",
                      "Show Speedbar": "true",
                      "Places Icons Auto-resize": "false",
                      "Places Icons Static Size": "16",
                      "Speedbar Width": "200"}


def test_kdeglobals_colours_are_raye_night():
    """The whole scheme is written out, the same as the scheme file."""
    scheme = parse(os.path.join(ROOT, "colors", "RayeNight.colors"))
    for group, keys in scheme.items():
        if group == "[General]":
            for key in ("accentActiveTitlebar", "shadeSortColumn", "TitlebarIsAccentColored"):
                assert KDEGLOBALS[group][key] == keys[key], key
            continue
        for key, value in keys.items():
            assert KDEGLOBALS.get(group, {}).get(key) == value, (group, key)


# --- the look-and-feel pair ----------------------------------------------

@pytest.mark.parametrize("mode", PACKAGES)
def test_metadata_is_a_named_look_and_feel(mode):
    meta = metadata(mode)
    assert meta["KPackageStructure"] == "Plasma/LookAndFeel"
    plugin = meta["KPlugin"]
    assert plugin["Id"] == PACKAGES[mode]
    assert plugin["Name"] == {"night": "Raye Night", "day": "Raye Day"}[mode]
    assert plugin["Name[fr]"] == {"night": "Raye Nuit", "day": "Raye Jour"}[mode]
    assert plugin["Description[fr]"]
    for value in plugin.values():
        if isinstance(value, str):
            assert "KDE" not in value and "Breeze" not in value


@pytest.mark.parametrize("mode", PACKAGES)
def test_names_match_the_colour_schemes(mode):
    scheme = parse(os.path.join(ROOT, "colors", SCHEMES[mode] + ".colors"))["[General]"]
    plugin = metadata(mode)["KPlugin"]
    assert plugin["Name"] == scheme["Name"]
    assert plugin["Name[fr]"] == scheme["Name[fr]"]


# the night picture by night, a light one by day
WALLPAPERS = {"night": "Raye", "day": "Raye-Lumen"}

@pytest.mark.parametrize("mode", PACKAGES)
def test_defaults_set_every_raye_piece(mode):
    d = defaults(mode)
    assert d["[kdeglobals][KDE]"] == {"widgetStyle": "Raye"}
    assert d["[kdeglobals][General]"]["ColorScheme"] == SCHEMES[mode]
    # an AccentColor makes Plasma blend the accent over the tuned scheme
    assert "AccentColor" not in d["[kdeglobals][General]"]
    assert "AccentColor" not in KDEGLOBALS["[General]"]
    assert d["[kdeglobals][Icons]"] == {"Theme": ICONS[mode]}
    assert d["[kdeglobals][Sounds]"] == {"Theme": "raye"}
    assert d["[plasmarc][Theme]"] == {"name": "raye"}
    assert d["[kcminputrc][Mouse]"] == {"cursorTheme": "Raye", "cursorSize": "24"}
    assert d["[Wallpaper]"] == {"Image": WALLPAPERS[mode]}


@pytest.mark.parametrize("mode", PACKAGES)
def test_defaults_set_the_decoration_explicitly(mode):
    # Plasma's day/night switch drops a decoration the package does not
    # name, so both packages name it, with the buttons on the right only.
    deco = defaults(mode)["[kwinrc][org.kde.kdecoration2]"]
    assert deco == {"library": "org.raye.decoration", "theme": "Raye",
                    "ButtonsOnLeft": "", "ButtonsOnRight": "IAX"}


def test_new_accounts_get_the_night_decoration_and_pointer():
    # A new account never applies the look-and-feel: KWin and the pointer
    # read the system files, so they must say what Raye Night says.
    night = defaults("night")
    kwin = parse(os.path.join(ROOT, "config", "kwinrc"))
    assert kwin["[org.kde.kdecoration2]"] == night["[kwinrc][org.kde.kdecoration2]"]
    mouse = parse(os.path.join(ROOT, "config", "kcminputrc"))
    assert mouse["[Mouse]"] == night["[kcminputrc][Mouse]"]
    with open(os.path.join(ROOT, "debian", "raye-theme.install"), encoding="utf-8") as f:
        install = f.read().splitlines()
    assert "config/kcminputrc etc/xdg" in install


@pytest.mark.parametrize("mode", PACKAGES)
def test_each_package_names_its_own_splash(mode):
    # Plasma writes the package's own id when it applies a theme, so the
    # file says the same.
    assert defaults(mode)["[KSplash]"] == {"Theme": PACKAGES[mode]}
    splash = os.path.join(LNF, PACKAGES[mode], "contents", "splash", "Splash.qml")
    assert os.path.isfile(splash)


def test_rules_copy_every_shared_part():
    # A link that leaves its package is refused by Plasma, so a part shared
    # in the tree and not named in the copy loop of debian/rules is a part
    # the Day look does not have.
    contents = os.path.join(LNF, PACKAGES["day"], "contents")
    shared = sorted(p for p in os.listdir(contents) if os.path.islink(os.path.join(contents, p)))
    with open(os.path.join(ROOT, "debian", "rules"), encoding="utf-8") as f:
        rules = f.read()
    m = re.search(r"for p in ([a-z ]+); do \\\n\s*if \[ -L \$\$d/\$\$p", rules)
    assert m, "the copy loop of debian/rules moved"
    assert sorted(m.group(1).split()) == shared


def test_ksplashrc_shows_the_raye_splash():
    ks = parse(os.path.join(ROOT, "config", "ksplashrc"))["[KSplash]"]
    assert ks == {"Engine": "KSplashQML", "Theme": PACKAGES["night"]}


@pytest.mark.parametrize("part", ["splash", "logout"])
def test_day_shares_the_night_parts(part):
    # In the tree the day package links to the night package's splash and
    # logout screen.
    # The default layout is not shared here any more: it belongs to the
    # desktop's own shell package (raye/desktop), not to a look. Plasma refuses a file reached through a link that leaves its
    # package, so the package build puts real copies in their place.
    link = os.path.join(LNF, PACKAGES["day"], "contents", part)
    assert os.path.islink(link)
    target = os.readlink(link)
    assert not os.path.isabs(target)
    night = os.path.join(LNF, PACKAGES["night"], "contents", part)
    assert os.path.isdir(night) and not os.path.islink(night)
    assert os.path.realpath(link) == os.path.realpath(night)


def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    assert head[:8] == b"\x89PNG\r\n\x1a\n"
    return struct.unpack(">II", head[16:24])


def jpeg_size(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:2] == b"\xff\xd8"
    i = 2
    while i < len(data):
        marker, length = data[i + 1], struct.unpack(">H", data[i + 2:i + 4])[0]
        if marker in (0xC0, 0xC1, 0xC2):
            h, w = struct.unpack(">HH", data[i + 5:i + 9])
            return w, h
        i += 2 + length
    raise AssertionError("no frame header in " + path)


# The two pictures Settings' Appearance card shows for a look, at the
# sizes it draws them: modulehost.cpp and backends/appearance.cpp both
# read contents/previews/preview.png, and show the card with no picture
# when it is not there (QFileInfo::exists ? ... : QUrl()).
PREVIEWS = (("preview.png", png_size, (600, 337)),
            ("fullscreenpreview.jpg", jpeg_size, (1920, 1080)))

# Looks that ship NO preview picture. A ledger, not a waiver: the test
# below fails the day one of them grows previews/, so the set can only
# shrink, and test_previews_for_the_settings_cards fails the day a look
# outside it loses its own.
#
# Night is in it, and Night is the DEFAULT look -- config/kdeglobals
# names it (test_kdeglobals_starts_on_raye_night), so the blank card in
# Settings is the card of the look every new account starts on. Day, the
# secondary one, has its pictures.
#
# This replaces a pytest.skip("night previews not in the tree yet"),
# and that is the point. A skip answers the question by not asking it: it
# was green while the default look shipped without its picture, and it
# would have stayed green for ever -- the launcher's own "tests skipped
# inside a green suite" line was the only trace. Nothing here can MAKE
# the picture: a preview claims to show the desktop the user will get, so
# it is a real capture of Raye Night or it is a lie, and inventing one is
# worse than shipping none. What a test CAN do is pin the absence, bound
# it, and fail the moment it changes in either direction.
NO_PREVIEW = {"night"}

def test_the_ledger_names_only_looks_that_ship():
    """A ledger about a package that is gone measures nothing."""
    unknown = sorted(NO_PREVIEW - set(PACKAGES))
    assert not unknown, "NO_PREVIEW names looks this repository has not: %s" % unknown
    assert set(PACKAGES) - NO_PREVIEW, (
        "every look is in NO_PREVIEW: no size below is measured any more")


# One test over EVERY look, branching on the ledger, and not two tests
# over two slices of it. pytest reports an empty parametrize set as a
# SKIP ("got empty parameter set"), so a test parametrised over NO_PREVIEW
# would turn back into a silent skip on the good day -- the day Night gets
# its pictures and the ledger empties. Measured while writing this file.
# Both branches assert; neither skips; the set is never empty.
@pytest.mark.parametrize("mode", sorted(PACKAGES))
def test_previews_for_the_settings_cards(mode):
    previews = os.path.join(LNF, PACKAGES[mode], "contents", "previews")
    if mode in NO_PREVIEW:
        # lexists, not isdir: a broken link or an empty folder is still a
        # previews entry dh_install copies into the package.
        assert not os.path.lexists(previews), (
            "%s has previews/ now. Take %r out of NO_PREVIEW -- then the two"
            " sizes below are measured -- and tick the look off the beta plan."
            % (PACKAGES[mode], mode))
        return
    assert os.path.isdir(previews), (
        "%s ships no previews/ any more: its Settings card loses its picture."
        " If that is deliberate, put %r in NO_PREVIEW and say why."
        % (PACKAGES[mode], mode))
    for name, size_of, want in PREVIEWS:
        path = os.path.join(previews, name)
        assert os.path.exists(path), "%s has no previews/%s" % (PACKAGES[mode], name)
        assert size_of(path) == want, (
            "%s previews/%s is %s, Settings draws %s"
            % (PACKAGES[mode], name, size_of(path), want))
    # Nothing else: whatever is in there ships inside the package, and a
    # stray file here is a file in everyone's /usr/share.
    assert sorted(os.listdir(previews)) == sorted(n for n, _, _ in PREVIEWS), (
        "%s previews/ carries more than the two pictures: %s"
        % (PACKAGES[mode], sorted(os.listdir(previews))))
