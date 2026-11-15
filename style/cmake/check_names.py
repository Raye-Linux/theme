#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 techoraye
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Checks that the style tree only answers to Raye names: no Breeze
# plugin id, config file, namespace or translation domain left over,
# so installing raye-style can never shadow or change Breeze, and the
# ids other parts of Raye rely on are there.
#
#   python3 style/cmake/check_names.py style

import gzip
import json
import os
import re
import sys

# Places that may name Breeze: where the code comes from, the tools that
# import it, and the test that checks the style refuses the Breeze key.
SKIPPED = ("LICENSES", "UPSTREAM", "README.md", "cmake", "autotests")
# Property names KDE applications set on their widgets for the style.
KEPT = re.compile(r'"_breeze_[a-z_]+"')
WORD = re.compile(r"breeze", re.IGNORECASE)

# Comments say where the code comes from, which is allowed and useful;
# only code has to be Raye-only. Strip them before looking.
BLOCK = re.compile(r"/\*.*?\*/", re.DOTALL)
MARKUP = re.compile(r"<!--.*?-->", re.DOTALL)
HASH_SUFFIXES = (".txt", ".cmake", ".py", ".sh", ".po", ".kcfgc", ".themerc")
MARKUP_SUFFIXES = (".ui", ".kcfg", ".xml", ".qrc")
# Text inside gzip: an icon carries the name of the file it was drawn from.
GZIP_SUFFIXES = (".svgz",)

# A config group may live in one file only. The style and the decoration
# share [Common] (the shadow settings): the decoration KCM writes it and
# the style reads it to draw the shadows of menus and tooltips. Split in
# two files, the style would read a copy nobody ever writes.
KCFG_FILE = re.compile(r'<kcfgfile\s+name="([^"]+)"')
KCFG_GROUP = re.compile(r'<group\s+name="([^"]+)"')

# (file, text that must be in it, what it is)
REQUIRED = [
    ("kstyle/raye.kcfg", '<kcfgfile name="rayerc"/>', "style config file rayerc"),
    ("kstyle/raye.themerc", "WidgetStyle=Raye", "style name in raye.themerc"),
    ("kstyle/raye.themerc", "ConfigPage=kstyle_config/rayestyleconfig",
     "Configure button of the style KCM"),
    ("kstyle/config/CMakeLists.txt", "raye_style_config", "translation domain raye_style_config"),
    ("kdecoration/CMakeLists.txt", "OUTPUT_NAME org.raye.decoration", "decoration plugin id org.raye.decoration"),
    ("kdecoration/CMakeLists.txt", '"raye_decoration"', "translation domain raye_decoration"),
    ("kdecoration/rayesettingsdata.kcfg", '<kcfgfile name="rayerc"/>', "decoration config file rayerc"),
    ("kdecoration/rayesettingsprovider.cpp", '"rayerc"', "decoration reads rayerc"),
    ("kdecoration/config/rayeconfigwidget.cpp", '"rayerc"', "decoration KCM writes rayerc"),
    ("po/fr/raye_decoration.po", "msgid", "French catalog raye_decoration"),
    ("po/fr/raye_style_config.po", "msgid", "French catalog raye_style_config"),
]


def _cut(line, markers):
    """The line up to its comment, ignoring markers inside a string."""
    opened = ""
    for i, char in enumerate(line):
        if char in '"\'':
            if not opened:
                opened = char
            elif opened == char:
                opened = ""
        elif not opened and any(line.startswith(m, i) for m in markers):
            return line[:i]
    return line


def strip_comments(text, rel):
    """The same text without its comments, line numbers unchanged."""
    def blank(match):
        return "\n" * match.group(0).count("\n")

    if rel.endswith((".cpp", ".h")):
        text = BLOCK.sub(blank, text)
    if rel.endswith(MARKUP_SUFFIXES):
        text = MARKUP.sub(blank, text)

    markers = []
    if rel.endswith((".cpp", ".h")):
        markers.append("//")
    if rel.endswith(HASH_SUFFIXES):
        markers.append("#")
    if not markers:
        return text
    return "\n".join(_cut(line, markers) for line in text.splitlines())


def _is_build_folder(path):
    """A CMake build folder (cmake -B inside the tree): its output is not source."""
    return os.path.isfile(os.path.join(path, "CMakeCache.txt"))


def _files(root):
    for top in sorted(os.listdir(root)):
        if top in SKIPPED or top.startswith("."):
            continue
        path = os.path.join(root, top)
        if os.path.isfile(path):
            yield top
            continue
        for dirpath, dirs, files in os.walk(path):
            if _is_build_folder(dirpath):
                dirs[:] = []
                continue
            dirs.sort()
            for name in sorted(files):
                yield os.path.relpath(os.path.join(dirpath, name), root)


def _read(root, rel):
    """The text of a file, unzipping an icon; None if it is not text."""
    path = os.path.join(root, rel)
    try:
        if rel.endswith(GZIP_SUFFIXES):
            with gzip.open(path, "rt", encoding="utf-8") as f:
                return f.read()
        with open(path, encoding="utf-8") as f:
            return f.read()
    except (OSError, UnicodeDecodeError, EOFError, gzip.BadGzipFile):
        return None


def _bytes_problem(root, rel):
    """What a file that is not text says, read as bytes."""
    try:
        with open(os.path.join(root, rel), "rb") as f:
            raw = f.read()
    except OSError as error:
        return "%s: cannot be read (%s)" % (rel, error.strerror)
    if re.search(b"breeze", raw, re.IGNORECASE):
        return "%s: not text, and its bytes still say breeze" % rel
    return None


def _split_groups(root):
    """Config groups that are declared in more than one config file."""
    where = {}
    for rel in _files(root):
        if not rel.endswith(".kcfg"):
            continue
        text = _read(root, rel) or ""
        name = KCFG_FILE.search(text)
        if not name:
            continue
        for group in KCFG_GROUP.findall(text):
            where.setdefault(group, {}).setdefault(name.group(1), []).append(rel)
    found = []
    for group, files in sorted(where.items()):
        if len(files) > 1:
            found.append("group [%s] is declared in %s: one group, one config file"
                         % (group, ", ".join("%s (%s)" % (f, ", ".join(r)) for f, r in sorted(files.items()))))
    return found


def problems(root, binary=None):
    found = []
    binary = [] if binary is None else binary
    for rel in _files(root):
        if WORD.search(rel):
            found.append("%s: file name still says breeze" % rel)
        text = _read(root, rel)
        if text is None:
            problem = _bytes_problem(root, rel)
            if problem:
                found.append(problem)
            else:
                binary.append(rel)
            continue
        for number, line in enumerate(strip_comments(text, rel).splitlines(), 1):
            if WORD.search(KEPT.sub("", line)):
                found.append("%s:%d: %s" % (rel, number, line.strip()))

    for rel, needle, what in REQUIRED:
        text = _read(root, rel)
        if text is None or needle not in text:
            found.append("%s: missing %s" % (rel, what))

    text = _read(root, "kstyle/raye.json")
    try:
        keys = json.loads(text)["Keys"] if text else None
    except (ValueError, KeyError, TypeError):
        keys = None
    if keys != ["Raye"]:
        found.append("kstyle/raye.json: style key must be exactly [\"Raye\"], found %r" % (keys,))

    text = _read(root, "kdecoration/raye.json")
    try:
        meta = json.loads(text) if text else {}
    except ValueError:
        meta = {}
    if meta.get("KPlugin", {}).get("Name") != "Raye":
        found.append("kdecoration/raye.json: decoration theme name must be Raye")
    if meta.get("X-KDE-ConfigModule") != "kcm_rayedecoration":
        found.append("kdecoration/raye.json: config module must be kcm_rayedecoration")
    found.extend(_split_groups(root))
    return found


def main(argv):
    root = argv[1] if len(argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    binary = []
    found = problems(root, binary)
    for line in found:
        print(line)
    for rel in binary:
        print("%s: not text, read as bytes only" % rel)
    print("%d problem(s) in %s" % (len(found), root))
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
