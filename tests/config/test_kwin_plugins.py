# SPDX-License-Identifier: GPL-3.0-or-later
"""config/kwinrc [Plugins]: every Raye plugin it turns on must exist.

🚨 This file exists because kwinrc turned on three plugins that were
nowhere in any Raye repository -- org.raye.desktoppill, org.raye.desktops
and org.raye.gestures. KWin ignores a plugin id it does not know, in
silence, so the lines read like working features and did nothing. One of
them even carried a comment saying it replaced a stock effect.

It also refuses a line that disables a stock effect which upstream already
ships disabled: desktopchangeosdEnabled=false was such a line. KWin reads
a plugin's default from its metadata and KPluginMetaData returns false
when EnabledByDefault is absent, so that line was turning off something
already off -- and making the file say the opposite of what it did.

⚠️ A third check was written and thrown away: that no comment in kwinrc
names a Raye plugin nothing ships. It cannot tell a comment that *claims*
a feature from a comment that *explains why one is missing*, so it failed
on the very note left in kwinrc to explain the removal. A test that
forbids the documentation it exists to protect is worse than no test.
"""
import json
import re
from pathlib import Path

TOP = Path(__file__).resolve().parents[2]
KWINRC = TOP / "config/kwinrc"


def plugins_section():
    """The [Plugins] group of config/kwinrc, as {plugin id: bool}."""
    out, inside = {}, False
    for line in KWINRC.read_text().splitlines():
        line = line.strip()
        if line.startswith("["):
            inside = line == "[Plugins]"
            continue
        if not inside or not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        assert key.endswith("Enabled"), f"[Plugins] key without Enabled: {key}"
        out[key[: -len("Enabled")]] = value.strip() == "true"
    return out


def shipped_plugin_ids():
    """Every plugin id this repository ships.

    Two kinds, because Raye ships both: a packaged plugin declares its id in
    a metadata.json, and a compiled one is named by the OUTPUT_NAME of its
    CMake target (the window decoration is the second kind, and reading only
    the first made this test call it missing).
    """
    ids = set()
    for cml in TOP.rglob("CMakeLists.txt"):
        if ".git" in cml.parts:
            continue
        ids.update(re.findall(r"OUTPUT_NAME\s+(org\.raye\.[\w.]+|raye_\w+)", cml.read_text()))
        ids.update(re.findall(r"kcoreaddons_add_plugin\(\s*(org\.raye\.[\w.]+|raye_\w+)", cml.read_text()))
    for meta in TOP.rglob("metadata.json"):
        if ".git" in meta.parts:
            continue
        try:
            data = json.loads(meta.read_text())
        except ValueError:
            continue
        # not every metadata.json in the tree is a plugin: some are lists
        if not isinstance(data, dict):
            continue
        plugin = data.get("KPlugin")
        plugin_id = plugin.get("Id") if isinstance(plugin, dict) else None
        if plugin_id:
            ids.add(plugin_id)
    return ids


def is_rayes(plugin_id):
    return plugin_id.startswith("org.raye.") or plugin_id.startswith("raye_")


def test_the_plugins_group_reads():
    assert plugins_section(), "no [Plugins] group in config/kwinrc"


def test_every_raye_plugin_turned_on_is_shipped():
    shipped = shipped_plugin_ids()
    missing = sorted(p for p, on in plugins_section().items() if on and is_rayes(p) and p not in shipped)
    assert not missing, (
        "kwinrc turns these on and no Raye repository ships them, so KWin "
        f"ignores the lines without a word: {missing}. Either the plugin is "
        "written, or the line goes -- a setting that names something absent "
        "reads like a feature and is not one.")


def test_no_raye_plugin_is_shipped_and_turned_off():
    shipped = shipped_plugin_ids()
    off = sorted(p for p, on in plugins_section().items() if not on and is_rayes(p) and p in shipped)
    assert not off, f"shipped by Raye and turned off in kwinrc: {off}"
