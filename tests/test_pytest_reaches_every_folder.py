#!/usr/bin/env python3
"""Every pytest folder of this repo must be reached by the package build.

    python3 -m pytest -q tests/test_pytest_reaches_every_folder.py

🚨 debian/rules used to name the folders it tested: plymouth, apps,
lookandfeel, colors, config. tests/terminal/ existed, with 20 tests, and
was in none of them -- and neither was art/, with 326. iso/tools/raye-tests
runs them; the PACKAGE BUILD, the gate that decides whether the ISO ships,
did not. An enumerated list goes stale the first time a folder is added,
and says nothing at all when it does.

⚠️ Some folders are reached by ctest instead of by debian/rules, and those
are listed below WITH the ctest that reaches them -- the registration is
checked too. An exemption nobody re-checks is the defect this file is
about, one level up.
"""
import pathlib
import re

REPO = pathlib.Path(__file__).resolve().parent.parent
RULES = REPO / "debian" / "rules"
PYTEST_LINE = re.compile(r"^\t\s*python3 -m pytest\s+(.*)$", re.M)
IGNORE = {"work", "out", "__pycache__", ".pytest_cache", ".git",
          "third-party", "debian"}

# folder -> (CMakeLists that registers it, the add_test name, the string
# that must still be in that file). Checked, not trusted.
BY_CTEST = {
    "shell/appicons/tests": ("shell/appicons/CMakeLists.txt",
                             "appicons", "test_app_icons.py"),
    "shell/taskbarsync/tests": ("shell/taskbarsync/tests/CMakeLists.txt",
                                "taskbarsync-pins", "test_pins.py"),
    "hyprland/tests": ("hyprland/tests/CMakeLists.txt",
                       "hyprland-session", "test_session.py"),
    "style/cmake": ("style/CMakeLists.txt", "raye-import-tools",
                    "unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/cmake"),
}


def no_comments(text):
    return "\n".join(l for l in text.splitlines()
                      if not l.lstrip().startswith("#"))


def build_pytest_paths():
    """Every path debian/rules hands to pytest, options dropped."""
    paths = []
    for args in PYTEST_LINE.findall(no_comments(RULES.read_text())):
        for a in args.split():
            if a.startswith("-") or a == "no:cacheprovider":
                continue
            paths.append(a.rstrip("/"))
    return paths


def folders_with_pytest_files():
    out = set()
    for pattern in ("test_*.py", "*_test.py"):
        for f in REPO.rglob(pattern):
            rel = f.relative_to(REPO)
            if any(part in IGNORE for part in rel.parts):
                continue
            out.add(rel.parent)
    return out


def test_the_build_runs_pytest_at_all():
    assert build_pytest_paths(), "debian/rules hands no path to pytest"


def test_every_pytest_folder_is_reached():
    given = [pathlib.PurePath(p) for p in build_pytest_paths()]
    exempt = {pathlib.PurePath(k) for k in BY_CTEST}
    missed = []
    for folder in sorted(folders_with_pytest_files()):
        if any(folder == g or g in folder.parents for g in given):
            continue
        if any(folder == e or e in folder.parents for e in exempt):
            continue
        missed.append(str(folder))
    assert not missed, (
        "the package build never runs the pytest files in %s. debian/rules"
        " hands pytest %s. Name a parent folder instead of listing folders,"
        " or the next one added is missed in silence."
        % (", ".join(missed), ", ".join(str(g) for g in given)))


def test_the_ctest_exemptions_are_still_registered():
    # Without this, BY_CTEST is a list of folders nobody tests and
    # nobody notices -- the same defect as the folder list in debian/rules.
    broken = []
    for folder, (where, name, needle) in sorted(BY_CTEST.items()):
        cml = REPO / where
        if not cml.is_file():
            broken.append("%s: %s is gone" % (folder, where))
            continue
        text = cml.read_text()
        if ("add_test" not in text or name not in text or needle not in text):
            broken.append("%s: %s no longer registers %s running %s"
                          % (folder, where, name, needle))
    assert not broken, (
        "these folders are exempt from the pytest check because a ctest"
        " reaches them, and it no longer does: " + "; ".join(broken))


def test_debian_rules_does_not_list_the_children_of_tests():
    # The exact shape that caused this: tests/apps tests/colors ... one by
    # one. Handing pytest "tests/" covers whatever is added later.
    named = [p for p in build_pytest_paths()
             if p.startswith("tests/") and p != "tests"]
    assert not named, (
        "debian/rules names folders inside tests/ (%s) instead of tests/"
        " itself, so a folder added next to them is tested by nothing"
        % ", ".join(named))
