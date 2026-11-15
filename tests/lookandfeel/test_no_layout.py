# The two global themes carry no default layout, and must not grow one.
#
#   python3 -m pytest -q tests/lookandfeel/test_no_layout.py
#
# This file exists because a layout in a look-and-feel package decides
# the desktop of every new account, and libplasma reads it FIRST. Raye's
# used to live there, so the shape of the bar at the bottom travelled with
# the colours: switching Night for Day, or picking any other look, handed
# the account somebody else's desktop. It is now a file of the desktop's
# own shell package -- raye/desktop,
# patches/plasma-desktop/0003-the-default-layout-belongs-to-the-desktop.patch,
# installed as /usr/share/plasma/shells/org.kde.plasma.desktop/contents/layout.js
# -- which is the only path libplasma reads for a layout that belongs to
# the desktop itself.
#
# Nothing in the packaging says that, so the absence reads like an
# oversight: the next person to notice that a Raye look "has no layout"
# will add one back, in good faith, and every new account will again get
# its taskbar from whichever look is active. That is what these checks are
# for, and why they name the patch.
#
# The constraint holds in both directions, and the second half is now
# checkable here -- it was not when this file was written. The look-and-feel
# may not stop shipping layouts/ unless the patched shell package is installed
# beside it, or a new account gets no layout at all and starts with NO TASKBAR,
# which is worse than the upstream bar the Raye layout replaces.
#
# This header used to say that half could not be checked, for a reason that was
# true at the time: raye/desktop declared no virtual package, and its version is
# <ubuntu version>+raye<n>, which no Depends can name -- while a Depends on
# plain plasma-desktop-data would go green against the stock package, which
# ships no layout.js at all. A green that proves nothing is worse than no check.
#
# raye/desktop now declares `Provides: raye-desktop-layout` on the binary
# package that really ships the layout, and only Raye's build of it does: the
# stock archive cannot answer that name. So debian/control CAN require it, and
# test_the_look_requires_a_layout_from_somewhere below pins that it does.
#
# A second, independent guard remains, and it is not redundant: a Depends is
# resolved at install time by whatever archive is configured, while raye/iso's
# build.sh refuses to build
# an image without the Raye desktop packages while
#
#     ls -d theme/lookandfeel/*/contents/layouts
#
# matches nothing -- the theme tree, read directly, not a flag. The check
# named test_no_look_and_feel_package_ships_a_layout below is written
# against that same glob on purpose, so the predicate the image build acts
# on and the one this repository pins cannot drift apart: put a layout back
# here and both relax together.

import os
import re
import shutil
import subprocess

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LNF = os.path.join(ROOT, "lookandfeel")
PACKAGES = {"night": "org.raye.night.desktop", "day": "org.raye.day.desktop"}

# what libplasma looks for in a look-and-feel package, and what it would
# have been called here: KPackage's Plasma/LookAndFeel structure maps the
# "layouts" folder, and the file is named after the shell package
LAYOUTS = "layouts"
LAYOUT_SCRIPT = re.compile(r"layout\.js$|-layout\.js$")


def packages():
    """Every look-and-feel package in the tree, named or not."""
    return sorted(d for d in os.listdir(LNF) if os.path.isdir(os.path.join(LNF, d)))


# --- the tree -----------------------------------------------------------

@pytest.mark.parametrize("mode", PACKAGES)
def test_neither_package_ships_a_layout(mode):
    contents = os.path.join(LNF, PACKAGES[mode], "contents")
    # lexists, not exists: Day's was a symlink into Night's package, and a
    # broken link is still a layouts entry dh_install would copy
    assert not os.path.lexists(os.path.join(contents, LAYOUTS)), (
        "%s carries %s again; the layout belongs to the shell package of "
        "raye/desktop (patch 0003), which libplasma reads second"
        % (PACKAGES[mode], LAYOUTS))
    for here, dirs, files in os.walk(contents):
        for name in dirs + files:
            assert not LAYOUT_SCRIPT.search(name), os.path.join(here, name)


def test_no_look_and_feel_package_ships_a_layout():
    # The glob raye/iso's build.sh reads to decide whether an image may be
    # built without the Raye desktop packages. Every package, so a third
    # look added later is covered the day it appears.
    found = [p for p in packages()
             if os.path.lexists(os.path.join(LNF, p, "contents", LAYOUTS))]
    assert found == [], (
        "lookandfeel/*/contents/layouts matches %s: raye/iso would stop "
        "requiring the Raye desktop packages, and the layout would be part "
        "of a look again" % found)


# --- what the package really installs -----------------------------------
#
# The tree is not what ships. debian/raye-theme.install is a script: it
# filters its list through debian/install-built, which drops a "? " line
# whose pattern matches nothing. And debian/rules then replaces some of
# Day's symlinks with real copies. Both steps are checked.

def install_list():
    """debian/raye-theme.install as dh_install receives it: (src, dest)."""
    out = subprocess.run([os.path.join(ROOT, "debian", "raye-theme.install")],
                         cwd=ROOT, capture_output=True, text=True, check=True)
    pairs = []
    for line in out.stdout.splitlines():
        if line.strip():
            src, _, dest = line.partition(" ")
            pairs.append((src, dest.strip()))
    return pairs


def rules_text():
    with open(os.path.join(ROOT, "debian", "rules"), encoding="utf-8") as f:
        return f.read()


def day_copy_parts():
    """The parts debian/rules turns from a link into a real copy."""
    body = rules_text().partition("override_dh_install:")[2].partition("\noverride_")[0]
    assert body, "override_dh_install is gone from debian/rules"
    m = re.search(r"for p in ([^;]*);", body)
    assert m, "the Day copy loop is gone from override_dh_install"
    return m.group(1).split()


@pytest.fixture(scope="module")
def installed(tmp_path_factory):
    """The look-and-feel part of debian/tmp, as dh_install leaves it."""
    destdir = tmp_path_factory.mktemp("destdir")
    for src, dest in install_list():
        if not src.startswith("lookandfeel/"):
            continue
        target = destdir / dest / os.path.basename(src)
        target.parent.mkdir(parents=True, exist_ok=True)
        # dh_install copies with cp -a: a symlink stays a symlink
        shutil.copytree(os.path.join(ROOT, src), target, symlinks=True)
    return destdir / "usr/share/plasma/look-and-feel"


def test_the_install_list_ships_both_looks(installed):
    # Day is a "? " line: it would be dropped in silence if it ever left
    # the tree, and config/kdeglobals names it as the light theme.
    assert sorted(os.listdir(installed)) == sorted(PACKAGES.values())


def test_what_is_installed_carries_no_layout(installed):
    for here, dirs, files in os.walk(installed, followlinks=False):
        for name in dirs + files:
            assert name != LAYOUTS, os.path.join(here, name)
            assert not LAYOUT_SCRIPT.search(name), os.path.join(here, name)


def test_the_build_replaces_every_link_that_leaves_its_package(installed):
    """debian/rules' copy list is exactly the links that need copying.

    Plasma refuses a file reached through a link that leaves the package,
    so Day's shared parts have to become real copies. The list in
    debian/rules is written by hand: a part added to Day as a link and not
    added there ships as a link Plasma will not follow -- which is how the
    old layouts/ link was shaped. This compares the two.
    """
    escaping = []
    for package in sorted(os.listdir(installed)):
        root = os.path.realpath(installed / package)
        for name in sorted(os.listdir(installed / package / "contents")):
            link = installed / package / "contents" / name
            if not os.path.islink(link):
                continue
            inside = os.path.realpath(link).startswith(root + os.sep)
            if not inside:
                escaping.append(name)
    assert sorted(escaping) == sorted(day_copy_parts())


def test_the_build_adds_no_layout():
    body = rules_text().partition("override_dh_install:")[2].partition("\noverride_")[0]
    assert LAYOUTS not in body, "override_dh_install puts a layouts back"


# --- the one copy libplasma reads ---------------------------------------

def test_no_package_of_this_repository_claims_a_layout_path():
    """No install list targets a layout, in a look or in a shell package.

    raye-shell does ship into the shell package's folder (its lock screen),
    so the path is reachable from here. contents/layout.js is owned by the
    patched plasma-desktop-data; a second copy of it from this repository
    is a file conflict, not a configuration -- dpkg would refuse the
    install, or the two packages would overwrite each other's layout.
    """
    lists = [n for n in os.listdir(os.path.join(ROOT, "debian"))
             if n.endswith(".install")]
    assert lists
    for name in lists:
        with open(os.path.join(ROOT, "debian", name), encoding="utf-8") as f:
            for n, line in enumerate(f, 1):
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                pattern = line.lstrip("? ").split(" ")[0]
                assert not LAYOUT_SCRIPT.search(pattern), "%s:%d: %s" % (name, n, line)
                assert "/%s/" % LAYOUTS not in pattern + "/", "%s:%d: %s" % (name, n, line)


# ------------------------------------- the other half: it must REQUIRE a layout
# This is the check the header above said was impossible. It became
# possible the day raye/desktop declared a virtual package for the thing that
# really ships the layout, and it is the only thing standing between
# `apt install raye-theme` on an upstream plasma-desktop and a first login with
# no taskbar at all.
#
# It deliberately refuses a Depends on plasma-desktop-data. Ubuntu's own
# build of that package satisfies the real name and carries no layout.js: the
# dependency has to be one the stock archive CANNOT answer, or it goes green
# while the defect it exists to catch is fully present.

LAYOUT_PROVIDES = "raye-desktop-layout"
# the real package whose stock build would satisfy a naive dependency
NOT_ENOUGH = "plasma-desktop-data"


def _paragraphs(control):
    """debian/control as a list of dicts, comments and continuations dropped."""
    out = []
    for chunk in control.split("\n\n"):
        fields = {}
        for line in chunk.splitlines():
            if line.startswith("#") or line.startswith((" ", "\t")):
                continue
            key, sep, value = line.partition(":")
            if sep:
                fields[key.strip()] = value.strip()
        if "Package" in fields:
            out.append(fields)
    return out


def _depends_of(package):
    """Every dependency of one binary package, as a flat list of names.

    ⚠️ Written against the FIELD, not a grep of the file: a Depends is a
    comma list that wraps over continuation lines, and `grep raye-desktop-layout
    debian/control` would also match the comment that explains it -- a green
    from a comment is exactly the failure mode this file is about.
    """
    control = os.path.join(ROOT, "debian/control")
    with open(control, encoding="utf-8") as fh:
        text = fh.read()
    # rejoin continuations before splitting into paragraphs
    paragraphs = _paragraphs(re.sub(r"\n[ \t]+", " ", text))
    for fields in paragraphs:
        if fields["Package"] == package:
            raw = fields.get("Depends", "")
            return [d.strip().split()[0] for d in raw.split(",") if d.strip()]
    raise AssertionError(f"no binary package named {package} in debian/control")


def test_the_look_requires_a_layout_from_somewhere():
    deps = _depends_of("raye-theme")
    assert LAYOUT_PROVIDES in deps, (
        f"raye-theme does not Depends on {LAYOUT_PROVIDES}. This look ships no "
        "layouts/ of its own (the checks above pin that), so without this "
        "dependency `apt install raye-theme` on an upstream plasma-desktop "
        "gives a first login with NO TASKBAR -- apt has to refuse instead of "
        "handing over a desktop that cannot draw its own bar. raye/desktop "
        "declares the name on the package that really ships the layout."
    )


def test_the_requirement_is_not_one_the_stock_archive_can_answer():
    deps = _depends_of("raye-theme")
    assert NOT_ENOUGH not in deps, (
        f"raye-theme Depends on {NOT_ENOUGH}. Ubuntu's own build of it "
        "satisfies that name and ships no layout.js, so the dependency would "
        f"be satisfied by a package with no layout: depend on {LAYOUT_PROVIDES}, "
        "which only Raye's build declares."
    )
