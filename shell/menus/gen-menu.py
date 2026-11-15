#!/usr/bin/env python3
"""Write raye-applications.menu from the category table below.

The XDG menu format has no notion of priority: an app whose categories
match two menus shows up in both. Raye wants every app in exactly one
category, so each category here excludes everything a category above it
already took. Writing those exclusions by hand is how mistakes happen,
so the menu file is generated and committed, and the tests check it is
current:

    python3 shell/menus/gen-menu.py            # rewrite the menu
    python3 shell/menus/gen-menu.py --check    # exit 1 when it is stale
"""

import sys
from pathlib import Path
from xml.sax.saxutils import escape

HERE = Path(__file__).resolve().parent
OUTPUT = HERE / "raye-applications.menu"

# Matched in this order, first match wins: an app goes to the menu of the
# first rule that names one of its XDG categories. A menu can have more
# than one rule, at different places in the order.
RULES = [
    ("Games", ["Game", "Emulator"]),
    ("Productivity", ["Office", "TextEditor", "WordProcessor", "Spreadsheet",
                      "Presentation", "Calendar", "ContactManagement", "Email",
                      "ProjectManagement", "Dictionary", "Education", "Science",
                      "Finance", "WebBrowser"]),
    ("Social", ["Chat", "InstantMessaging", "IRCClient", "VideoConference",
                "Telephony", "Feed", "News"]),
    ("Development", ["Development", "IDE", "Debugger", "RevisionControl",
                     "Building", "GUIDesigner", "Profiling", "Translation",
                     "WebDevelopment", "ParallelComputing"]),
    # Scanners say Graphics too, but scanning is a tool, not a creative app.
    ("Utilities", ["Scanning"]),
    ("Creativity", ["Graphics", "2DGraphics", "3DGraphics", "VectorGraphics",
                    "RasterGraphics", "Photography", "Publishing",
                    "AudioVideoEditing", "Midi", "Mixer", "Sequencer"]),
    ("Entertainment", ["AudioVideo", "Audio", "Video", "Player", "TV", "Music"]),
    ("Utilities", ["Utility", "Accessibility", "Archiving", "Compression",
                   "Calculator", "Clock", "Printing", "FileManager", "FileTools",
                   "PackageManager", "Settings", "Network", "RemoteAccess",
                   "FileTransfer", "Maps"]),
    ("System", ["System", "Monitor", "Security", "Filesystem", "TerminalEmulator",
                "HardwareSettings", "Documentation", "DesktopSettings"]),
]

DIRECTORIES = {
    "Productivity": "raye-productivity.directory",
    "Creativity": "raye-creativity.directory",
    "Entertainment": "raye-entertainment.directory",
    "Social": "raye-social.directory",
    "Games": "raye-games.directory",
    "Utilities": "raye-utilities.directory",
    "Development": "raye-development.directory",
    "System": "raye-system.directory",
    "Other": "raye-other.directory",
}

OTHER = "Other"

# The order the categories are shown in, the one Windows uses.
LAYOUT = ["Productivity", "Creativity", "Entertainment", "Social", "Games",
          "Utilities", "Development", "System", "Other"]

# Apps whose categories say something else than what they are.
PLACED = {
    "org.kde.krecorder.desktop": "Utilities",  # Sound recorder
    "org.kde.kcharselect.desktop": "System",   # Character map, with the tools
}

# Never in the menu. KDE System Settings: Raye Settings takes its place
# (the Settings area also moves its entry out). The emoji selector: the
# emoji panel (Win+.) replaces it, and Windows lists no such app.
HIDDEN = [
    "systemsettings.desktop",
    "kdesystemsettings.desktop",
    "org.kde.plasma.emojier.desktop",
]

# Kept out of Other, as Plasma's own menu does: core tools of other
# desktops and the old settings modules.
OTHER_SKIPPED = [
    "X-KDE-settings-hardware", "X-KDE-settings-accessibility",
    "X-KDE-settings-components", "X-KDE-settings-desktop",
    "X-KDE-settings-looknfeel", "X-KDE-settings-network",
    "X-KDE-settings-webbrowsing", "X-KDE-settings-peripherals",
    "X-KDE-settings-power", "X-KDE-settings-security",
    "X-KDE-settings-sound", "X-KDE-settings-system", "X-KDE-information",
]


def check_table():
    names = [menu for menu, _ in RULES] + [OTHER]
    assert sorted(set(names)) == sorted(LAYOUT), "LAYOUT must list every menu once"
    assert sorted(DIRECTORIES) == sorted(LAYOUT), "every menu needs a directory file"
    assert set(PLACED.values()) <= set(names), "PLACED names an unknown menu"
    seen = {}
    for menu, cats in RULES:
        for cat in cats:
            assert cat not in seen, f"{cat} is in {seen.get(cat)} and {menu}"
            seen[cat] = menu
    assert not set(PLACED) & set(HIDDEN), "an app is both placed and hidden"


def tag(name, value, indent):
    return f"{indent}<{name}>{escape(value)}</{name}>"


def block(name, children, indent):
    return [f"{indent}<{name}>", *children, f"{indent}</{name}>"]


def rule_block(index, name):
    i3, i4, i5 = "\t" * 3, "\t" * 4, "\t" * 5
    cats = RULES[index][1]
    taken = [c for _, earlier in RULES[:index] for c in earlier]
    elsewhere = sorted(f for f, where in PLACED.items() if where != name)
    rule = block("Or", [tag("Category", c, i5) for c in cats], i4)
    if taken:
        rule += block("Not", [tag("Category", c, i5) for c in taken], i4)
    if elsewhere:
        rule += block("Not", [tag("Filename", f, i5) for f in elsewhere], i4)
    return block("And", rule, i3)


def category_menu(name):
    i3 = "\t" * 3
    include = []
    for index, (menu, _) in enumerate(RULES):
        if menu == name:
            include += rule_block(index, name)
    include += [tag("Filename", f, i3) for f, where in sorted(PLACED.items()) if where == name]
    lines = [tag("Name", name, "\t\t"), tag("Directory", DIRECTORIES[name], "\t\t")]
    lines += block("Include", include, "\t\t")
    lines += block("Exclude", [tag("Filename", f, i3) for f in HIDDEN], "\t\t")
    return block("Menu", lines, "\t")


def other_menu():
    i3, i4, i5 = "\t" * 3, "\t" * 4, "\t" * 5
    skipped = [*block("And", [tag("Category", "Core", i5),
                              *block("Not", [tag("Category", "KDE", "\t" * 6)], i5)], i4),
               *[tag("Category", c, i4) for c in OTHER_SKIPPED]]
    lines = [tag("Name", OTHER, "\t\t"), tag("Directory", DIRECTORIES[OTHER], "\t\t"),
             "\t\t<OnlyUnallocated/>"]
    lines += block("Include", block("Not", skipped, i3), "\t\t")
    lines += block("Exclude", [tag("Filename", f, i3) for f in HIDDEN], "\t\t")
    return block("Menu", lines, "\t")


def render():
    check_table()
    out = [
        '<!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN"',
        '  "http://www.freedesktop.org/standards/menu-spec/1.0/menu.dtd">',
        "<!-- Generated by shell/menus/gen-menu.py: change the table there. -->",
        "<!-- The Raye application categories, used by Start and every app",
        "     picker of the session (XDG_MENU_PREFIX=raye-). -->",
        "<Menu>",
        "\t<Name>Applications</Name>",
        "\t<DefaultAppDirs/>",
        "\t<DefaultDirectoryDirs/>",
    ]
    out += block("Layout", [tag("Menuname", n, "\t\t") for n in LAYOUT]
                 + ['\t\t<Merge type="menus"/>', '\t\t<Merge type="files"/>'], "\t")
    for name in LAYOUT:
        if name != OTHER:
            out += category_menu(name)
    out += other_menu()
    out += ["\t<DefaultMergeDirs/>",
            "\t<MergeFile>applications-kmenuedit.menu</MergeFile>",
            "</Menu>"]
    return "\n".join(out) + "\n"


def main(argv):
    text = render()
    if "--check" in argv:
        if OUTPUT.read_text(encoding="utf-8") != text:
            print(f"{OUTPUT.name} is stale: run python3 {Path(__file__).name}", file=sys.stderr)
            return 1
        return 0
    OUTPUT.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
