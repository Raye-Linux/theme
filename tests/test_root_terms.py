"""Shown text speaks Linux: root access, sudo user, never administrator.

    python3 -m pytest -q tests/test_root_terms.py

Code comments, file names (org.raye.terminal-admin.desktop is pinned in
people's menus by that name) and ids keep the old word; what a person can
read does not.
"""
import pathlib
import re

REPO = pathlib.Path(__file__).resolve().parent.parent
OLD = re.compile(r"administrat|\badmin\b", re.I)
SKIP = {".git", "__pycache__", "work", "out", "debian"}


def files(*patterns):
    for pattern in patterns:
        for path in REPO.rglob(pattern):
            if path.is_file() and not SKIP.intersection(path.relative_to(REPO).parts):
                yield path


def hits(lines):
    return [line for line in lines if OLD.search(line)]


def test_translations():
    bad = []
    for po in files("*.po"):
        text = po.read_text(encoding="utf-8")
        bad += [f"{po.relative_to(REPO)}: {m}" for m in hits(re.findall(r'^msg(?:id|str) "(.*)"$', text, re.M))]
    assert not bad, bad


def test_qml_and_cpp_strings():
    call = re.compile(r'\bi18n[a-z]*\((?:"[^"]*",\s*)?"([^"]*)"')
    bad = []
    for path in files("*.qml", "*.js", "*.cpp"):
        if "tests" in path.parts:
            continue
        for found in call.findall(path.read_text(encoding="utf-8", errors="replace")):
            if OLD.search(found):
                bad.append(f"{path.relative_to(REPO)}: {found}")
    assert not bad, bad


def test_sign_in_strings():
    text = (REPO / "signin/org/raye/signin/Strings.qml").read_text(encoding="utf-8")
    values = re.findall(r'^\s+\w+: "(.*)",?$', text, re.M)
    assert "Root access needed" in values and "Accès root requis" in values
    assert not hits(values)


def test_desktop_entries():
    bad = []
    for path in files("*.desktop"):
        for line in path.read_text(encoding="utf-8").splitlines():
            if re.match(r"(Name|GenericName|Comment)(\[[^]]+\])?=", line) and OLD.search(line):
                bad.append(f"{path.relative_to(REPO)}: {line}")
    assert not bad, bad
    entry = (REPO / "shell/menus/org.raye.terminal-admin.desktop").read_text(encoding="utf-8")
    assert "Name=Terminal (root)" in entry


def test_package_and_unit_descriptions():
    control = (REPO / "debian/control").read_text(encoding="utf-8")
    descriptions = re.findall(r"^Description: .*(?:\n .*)*", control, re.M)
    assert not hits(descriptions)
    units = [line for path in files("*.service") for line in path.read_text().splitlines()
             if line.startswith("Description=")]
    assert units and not hits(units)
