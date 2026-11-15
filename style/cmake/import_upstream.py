#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 techoraye
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Imports the Breeze widget style and window decoration as the Raye
# style: copies kstyle/, kdecoration/ and libbreezecommon/ out of a
# Breeze source tree and renames every plugin id, config file, class
# namespace and translation domain, so the result installs next to
# Breeze without touching it.
#
#   apt-get source breeze
#   python3 style/cmake/import_upstream.py breeze-6.6.5 /tmp/raye-import 4:6.6.5-0ubuntu0.1
#
# It only ever writes into folders that do not exist yet. To move to a
# newer Breeze, import into an empty folder, diff it against style/ and
# carry the Raye changes over by hand.

import gzip
import json
import os
import re
import shutil
import sys

# Folders taken from the Breeze tree, with where they land.
IMPORTED = {
    "kstyle": "kstyle",
    "kdecoration": "kdecoration",
    "libbreezecommon": "libraye-common",
}
# Only English and French ship; the other catalogs say "Breeze".
LANGUAGES = ("fr",)
TEXT_SUFFIXES = (".cpp", ".h", ".cmake", ".txt", ".json", ".kcfg", ".kcfgc",
                 ".themerc", ".ui", ".sh", ".po", ".qtcurve")
# Text too, inside gzip: the icons carry the name of the file they were
# drawn from.
GZIP_SUFFIXES = (".svgz",)
# Left behind: these extract the strings for KDE's translation server,
# which Raye does not use. Its catalogs are kept by hand.
NOT_IMPORTED = ("Messages.sh",)

# Property names KDE applications set on their own widgets to ask the
# style for something (a frame, no separator...). They are part of the
# style's public behaviour, so they keep their names.
KEPT = re.compile(r'(?<=")_breeze_[a-z_]+(?=")')

# Specific names first, the plain word last.
RULES = [
    ("org.kde.Breeze.Style", "org.raye.Style"),
    ("org.kde.breeze", "org.raye.decoration"),
    ("breeze_kwin_deco", "raye_decoration"),
    ("breeze_style_config", "raye_style_config"),
    ("libbreezecommon", "libraye-common"),
    # "Raye Settings" is the name of the settings app, keep this one apart
    ("breeze-settings6", "raye-style-settings"),
    ("breeze-settings", "raye-style-settings"),
    ("Breeze Settings", "Raye Style Settings"),
    ("Paramètres de Breeze", "Paramètres du style Raye"),
    ("Configuration de Breeze", "Configuration du style Raye"),
    ("=Brise\n", "=Raye\n"),
    ("BREEZE", "RAYE"),
    ("Breeze", "Raye"),
    ("breeze", "raye"),
]

PO_RENAMES = {
    "breeze_kwin_deco.po": "raye_decoration.po",
    "breeze_style_config.po": "raye_style_config.po",
}


def rename_text(text):
    """Rename Breeze to Raye in the content of a file.

    The style and the decoration share one config file, "rayerc", the way
    they share "breezerc" upstream: its [Common] group holds the shadow
    settings, the decoration KCM writes them and the style reads them to
    draw the shadows of menus and tooltips. Two files would leave the
    style reading a copy nobody ever writes.
    """
    kept = []

    def hold(match):
        kept.append(match.group(0))
        return "\0%d\0" % (len(kept) - 1)

    text = KEPT.sub(hold, text)
    for old, new in RULES:
        text = text.replace(old, new)
    return re.sub("\0(\\d+)\0", lambda m: kept[int(m.group(1))], text)


def rename_path(path):
    """Where a file of the Breeze tree goes in the Raye tree."""
    parts = path.split("/")
    if parts[0] in IMPORTED:
        parts[0] = IMPORTED[parts[0]]
    name = PO_RENAMES.get(parts[-1], parts[-1])
    name = name.replace("breeze-settings", "raye-style-settings").replace("breeze", "raye")
    return "/".join(parts[:-1] + [name])


def _translated(key):
    """True for 'Name[de]' style keys in any language but the shipped ones."""
    m = re.fullmatch(r".+\[([^\]]+)\]", key)
    return bool(m) and m.group(1) not in LANGUAGES


def trim_json_translations(data):
    """Copy of plugin metadata without the translations Raye does not ship."""
    if isinstance(data, dict):
        return {k: trim_json_translations(v) for k, v in data.items() if not _translated(k)}
    if isinstance(data, list):
        return [trim_json_translations(v) for v in data]
    return data


def trim_ini_translations(text):
    """Same as trim_json_translations, for a desktop-entry style file."""
    keep = []
    for line in text.splitlines(keepends=True):
        key = line.split("=", 1)[0].strip()
        if "=" in line and not line.lstrip().startswith(("#", "[")) and _translated(key):
            continue
        keep.append(line)
    return "".join(keep)


def _upstream_version(src):
    with open(os.path.join(src, "CMakeLists.txt"), encoding="utf-8") as f:
        m = re.search(r'set\(PROJECT_VERSION "([^"]+)"\)', f.read())
    if not m:
        sys.exit("no PROJECT_VERSION in %s/CMakeLists.txt" % src)
    return m.group(1)


def _copy_gzip(src, dst):
    """Rename inside a gzipped text file (an icon), keeping it gzipped."""
    with gzip.open(src, "rt", encoding="utf-8") as f:
        text = rename_text(f.read())
    # mtime 0: the same input gives the same bytes every time.
    with gzip.GzipFile(dst, "wb", mtime=0) as f:
        f.write(text.encode("utf-8"))


def _copy_file(src, dst, rel_dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if rel_dst.endswith(GZIP_SUFFIXES):
        _copy_gzip(src, dst)
        shutil.copymode(src, dst)
        return
    if not rel_dst.endswith(TEXT_SUFFIXES):
        shutil.copy2(src, dst)
        return
    with open(src, encoding="utf-8") as f:
        text = rename_text(f.read())
    if rel_dst.endswith(".json"):
        text = json.dumps(trim_json_translations(json.loads(text)), indent=4, ensure_ascii=False) + "\n"
    elif rel_dst.endswith(".themerc"):
        text = trim_ini_translations(text)
    with open(dst, "w", encoding="utf-8") as f:
        f.write(text)
    shutil.copymode(src, dst)


def _sources(src):
    """(relative source path, relative destination path) of every imported file."""
    for top in IMPORTED:
        for root, dirs, files in os.walk(os.path.join(src, top)):
            dirs.sort()
            for name in sorted(files):
                if name in NOT_IMPORTED:
                    continue
                rel = os.path.relpath(os.path.join(root, name), src)
                yield rel, rename_path(rel)
    licenses = os.path.join(src, "LICENSES")
    if os.path.isdir(licenses):
        for name in sorted(os.listdir(licenses)):
            yield "LICENSES/" + name, "LICENSES/" + name
    for lang in LANGUAGES:
        for name in sorted(PO_RENAMES):
            rel = "po/%s/%s" % (lang, name)
            if os.path.exists(os.path.join(src, rel)):
                yield rel, "po/%s/%s" % (lang, PO_RENAMES[name])


def import_tree(src, dst, source_version):
    pairs = list(_sources(src))
    tops = sorted({d.split("/", 1)[0] for _, d in pairs} | {"UPSTREAM"})
    taken = [t for t in tops if os.path.exists(os.path.join(dst, t))]
    if taken:
        sys.exit("refusing to overwrite %s in %s: import into an empty folder"
                 % (", ".join(taken), dst))
    for rel_src, rel_dst in pairs:
        _copy_file(os.path.join(src, rel_src), os.path.join(dst, rel_dst), rel_dst)
    with open(os.path.join(dst, "UPSTREAM"), "w", encoding="utf-8") as f:
        f.write("Imported from KDE Breeze %s (source package version %s):\n"
                "kstyle/, kdecoration/, libbreezecommon/ (now libraye-common/),\n"
                "LICENSES/ and the French catalogs, renamed by cmake/import_upstream.py.\n"
                % (_upstream_version(src), source_version))


def main(argv):
    if len(argv) != 4:
        sys.exit("usage: %s <breeze source dir> <destination> <source version>" % argv[0])
    import_tree(argv[1], argv[2], argv[3])


if __name__ == "__main__":
    main(sys.argv)
