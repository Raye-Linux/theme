# SPDX-License-Identifier: GPL-3.0-or-later
"""Quick settings is called the same thing everywhere in French (Windows
says "Paramètres rapides", and the Settings app is "Paramètres"). The energy
saver tile keeps its short name: "Économiseur d'énergie" is 131 px in Inter
12 and the label under a tile is one line of 96 (tst_quicksettings checks it)."""
import json
import pathlib

root = pathlib.Path(__file__).resolve().parent.parent.parent / "shell"
po = (root / "po/fr/raye_shell_quicksettings.po").read_text(encoding="utf-8")
meta = json.loads((root / "plasmoids/org.raye.quicksettings/metadata.json").read_text(encoding="utf-8"))["KPlugin"]


def msgstr(msgid):
    lines = po.splitlines()
    i = lines.index(f'msgid "{msgid}"')
    assert lines[i + 1].startswith("msgstr "), msgid
    return lines[i + 1][len('msgstr "'):-1]


def test_energy_saver_tile_fits_under_the_tile():
    assert msgstr("Energy saver") == "Économie"


def test_the_panel_has_one_french_name():
    assert msgstr("Quick settings") == "Paramètres rapides"
    assert meta["Name[fr]"] == "Paramètres rapides"
    assert msgstr("Open quick settings") == "Ouvrir les paramètres rapides"
    assert msgstr("Edit quick settings") == "Modifier les paramètres rapides"
    assert "réglages rapides" not in po.lower() and "réglages rapides" not in meta["Description[fr]"].lower()
