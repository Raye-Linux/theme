# Every name config/kdeglobals and the two look-and-feel packages write
# must designate something this repository really ships.
#
#   python3 -m pytest -q tests/lookandfeel/test_names_resolve.py
#
# These keys are names, not settings, and Plasma answers a name it
# cannot resolve by quietly using Breeze. config/kdeglobals says so
# itself, about widgetStyle: "Plasma falls back to Breeze when the style
# is not installed." A new account would then look almost right -- Raye
# colours, Raye icons, Breeze buttons -- and nothing anywhere would say
# which half was missing. test_lookandfeel.py pins the VALUES of these
# keys; this file asks whether the values point at anything.
#
# The five keys the day/night pair rests on:
#
#   [KDE] LookAndFeelPackage        what a new account starts as
#   [KDE] DefaultDarkLookAndFeel    the dark half of the Settings switch
#   [KDE] DefaultLightLookAndFeel   the light half
#   [KDE] AutomaticLookAndFeel      whether the switch is armed at all
#   [KDE] widgetStyle               who draws the buttons
#
# Four name a thing; the fifth is a boolean, and test_lookandfeel.py's
# test_kdeglobals_names_the_day_and_night_pair owns it. It is off, and
# that is exactly why the light half needs checking HERE rather than in a
# session: with the switch off, the day package is never loaded, so a
# dangling DefaultLightLookAndFeel shows up only the day someone turns
# "Change with the time of day" on -- months after the mistake.

import json
import os
import re
import subprocess

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LNF = os.path.join(ROOT, "lookandfeel")
PACKAGES = {"night": "org.raye.night.desktop", "day": "org.raye.day.desktop"}
STYLE = "Raye"


def parse(path):
    """KConfig as Plasma reads it: [a][b] group paths, # comments."""
    out, group = {}, None
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if line.startswith("["):
                group = line
                out.setdefault(group, {})
                continue
            key, _, value = line.partition("=")
            out[group][key] = value
    return out


KDEGLOBALS = parse(os.path.join(ROOT, "config", "kdeglobals"))


def defaults(mode):
    return parse(os.path.join(LNF, PACKAGES[mode], "contents", "defaults"))


def installed_sources(package):
    """The source paths debian/<package>.install really hands dh_install.

    The lists are scripts: a "? " line whose pattern matches nothing is
    dropped in silence (debian/install-built). Resolving them is the
    difference between "the file is in the tree" and "the file ships".
    """
    out = subprocess.run([os.path.join(ROOT, "debian", package + ".install")],
                         cwd=ROOT, capture_output=True, text=True, check=True)
    return [line.split(" ")[0] for line in out.stdout.splitlines() if line.strip()]


THEME_SOURCES = installed_sources("raye-theme")


def install_patterns(package):
    """The patterns debian/<package>.install LISTS, resolved or not.

    ⚠️ raye-style's list cannot be resolved the way raye-theme's can: its
    files are built, so install-built finds them in debian/tmp during a
    package build and nowhere at all outside one. Resolving it here would
    return an empty list and any check built on it would pass for the one
    reason that proves nothing. The patterns are read instead, and what
    makes them produce a file is checked separately, below.
    """
    out = []
    with open(os.path.join(ROOT, "debian", package + ".install"), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or line in ("LIST",) or line.startswith("exec "):
                continue
            if line.startswith("? "):
                line = line[2:]
            out.append(line.split(" ")[0])
    return out


STYLE_PATTERNS = install_patterns("raye-style")


def rules_text():
    with open(os.path.join(ROOT, "debian", "rules"), encoding="utf-8") as f:
        return f.read()


# --- the three look-and-feel names --------------------------------------

@pytest.mark.parametrize("key,mode", [
    ("LookAndFeelPackage", "night"),
    ("DefaultDarkLookAndFeel", "night"),
    ("DefaultLightLookAndFeel", "day"),
])
def test_each_look_and_feel_name_is_a_package_that_ships(key, mode):
    named = KDEGLOBALS["[KDE]"][key]
    assert named == PACKAGES[mode]
    # the folder, the id inside it, and the install line that carries it
    with open(os.path.join(LNF, named, "metadata.json"), encoding="utf-8") as f:
        assert json.load(f)["KPlugin"]["Id"] == named
    assert "lookandfeel/" + named in THEME_SOURCES, (
        "%s=%s, but debian/raye-theme.install does not install it" % (key, named))


def test_the_splash_each_package_names_is_its_own_package():
    # [KSplash] Theme is a look-and-feel id too, and config/ksplashrc
    # repeats Night's: three places that must name a package that ships.
    for mode in PACKAGES:
        assert defaults(mode)["[KSplash]"]["Theme"] == PACKAGES[mode]
    named = parse(os.path.join(ROOT, "config", "ksplashrc"))["[KSplash]"]["Theme"]
    assert named == PACKAGES["night"]
    assert "lookandfeel/" + named in THEME_SOURCES


# --- widgetStyle=Raye ---------------------------------------------------

def test_the_widget_style_is_named_in_all_three_places():
    assert KDEGLOBALS["[KDE]"]["widgetStyle"] == STYLE
    for mode in PACKAGES:
        assert defaults(mode)["[kdeglobals][KDE]"]["widgetStyle"] == STYLE


def test_the_widget_style_name_is_a_style_this_repository_declares():
    """QStyleFactory resolves widgetStyle by the plugin's Keys."""
    with open(os.path.join(ROOT, "style", "kstyle", "raye.json"), encoding="utf-8") as f:
        assert STYLE in json.load(f)["Keys"]
    # the json is only read if the plugin names it
    with open(os.path.join(ROOT, "style", "kstyle", "rayestyleplugin.h"), encoding="utf-8") as f:
        assert 'FILE "raye.json"' in f.read()
    # and Settings lists the style by its .themerc
    themerc = parse(os.path.join(ROOT, "style", "kstyle", "raye.themerc"))
    assert themerc["[KDE]"]["WidgetStyle"] == STYLE
    assert themerc["[Misc]"]["Name"] == STYLE


def test_the_widget_style_ships():
    plugins = [s for s in STYLE_PATTERNS if "/plugins/styles/" in s]
    assert plugins, ("debian/raye-style.install lists no Qt style: "
                     "widgetStyle=%s would fall back to Breeze" % STYLE)
    assert all(re.search(r"/raye[^/]*\.so$", s) for s in plugins), plugins


def test_the_style_tree_is_built_and_must_produce_the_style():
    """What turns raye-style's "? " patterns into real files.

    Every line of debian/raye-style.install is optional, so an empty
    raye-style is a valid package as far as dh_install is concerned. Two
    things stop that: the style tree is in RAYE_TREES and not held back,
    and override_dh_install then lists the two plugins, which fails the
    build when the tree produced neither.
    """
    rules = rules_text()
    trees = re.search(r"^RAYE_TREES := (.*)$", rules, re.M)
    assert trees, "RAYE_TREES is gone from debian/rules"
    assert "style" in trees.group(1).split(), trees.group(0)
    held = re.search(r"^RAYE_TREES_NOT_READY \?= *(.*)$", rules, re.M)
    assert held and "style" not in held.group(1).split(), held.group(0)
    body = rules.partition("override_dh_install:")[2].partition("\noverride_")[0]
    assert "plugins/styles/raye*.so" in body, body
    assert "org.raye.decoration.so" in body, body


def test_the_decoration_library_each_package_names_ships():
    # same shape as widgetStyle: KWin answers a library it cannot load
    # with Breeze's decoration, in silence
    library = defaults("night")["[kwinrc][org.kde.kdecoration2]"]["library"]
    assert defaults("day")["[kwinrc][org.kde.kdecoration2]"]["library"] == library
    with open(os.path.join(ROOT, "style", "kdecoration", "CMakeLists.txt"),
              encoding="utf-8") as f:
        assert "OUTPUT_NAME " + library in f.read()
    assert any(s.endswith("/" + library + ".so") for s in STYLE_PATTERNS), STYLE_PATTERNS


# --- the style has to be installed beside the theme ---------------------

def binary_packages():
    """debian/control as {package: [package names it depends on]}."""
    out, name = {}, None
    text = open(os.path.join(ROOT, "debian", "control"), encoding="utf-8").read()
    for stanza in text.split("\n\n"):
        fields, field = {}, None
        for line in stanza.splitlines():
            if line.startswith((" ", "\t")) and field:
                fields[field] += " " + line.strip()
            elif ":" in line:
                field, _, value = line.partition(":")
                fields[field] = value.strip()
        name = fields.get("Package")
        if not name:
            continue
        deps = []
        for dep in fields.get("Depends", "").split(","):
            dep = dep.strip().split("|")[0].strip().split(" ")[0]
            if dep and not dep.startswith("${"):
                deps.append(dep)
        out[name] = deps
    return out


def test_raye_theme_cannot_be_installed_without_the_raye_style():
    """config/kdeglobals ships with raye-theme; the style ships elsewhere.

    ⚠️ Nothing warns when the two come apart. raye-theme writes
    widgetStyle=Raye for every account; the plugin that answers that name
    is in raye-style, a different binary package. Reached today through
    raye-shell. Drop that link and every Raye desktop silently draws its
    buttons with Breeze.
    """
    control = binary_packages()
    assert "raye-theme" in control and "raye-style" in control
    seen, queue = set(), ["raye-theme"]
    while queue:
        package = queue.pop()
        for dep in control.get(package, []):
            if dep in seen:
                continue
            seen.add(dep)
            if dep in control:
                queue.append(dep)
    assert "raye-style" in seen, (
        "raye-theme no longer requires raye-style (reached: %s)"
        % sorted(d for d in seen if d.startswith("raye-")))


# --- everything else the two packages name ------------------------------

@pytest.mark.parametrize("mode", PACKAGES)
def test_every_other_name_of_the_defaults_is_a_file_in_the_tree(mode):
    d = defaults(mode)
    named = {
        os.path.join("colors", d["[kdeglobals][General]"]["ColorScheme"] + ".colors"),
        os.path.join("icons", d["[kdeglobals][Icons]"]["Theme"]),
        os.path.join("sounds", d["[kdeglobals][Sounds]"]["Theme"]),
        os.path.join("desktoptheme", d["[plasmarc][Theme]"]["name"]),
        os.path.join("cursors", d["[kcminputrc][Mouse]"]["cursorTheme"]),
        os.path.join("wallpaper", d["[Wallpaper]"]["Image"]),
    }
    for path in sorted(named):
        assert os.path.exists(os.path.join(ROOT, path)), path
