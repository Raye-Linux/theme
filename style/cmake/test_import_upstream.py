#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 techoraye
# SPDX-License-Identifier: GPL-2.0-or-later
"""Tests for import_upstream.py: renaming rules, paths and metadata."""

import gzip
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import import_upstream as imp  # noqa: E402


def _read(path, mode="r"):
    with open(path, mode) as f:
        return f.read()


class RenameText(unittest.TestCase):
    def test_namespace_and_macros(self):
        src = "namespace Breeze {\n#define BREEZE_HAVE_KSTYLE 1\nbreeze::Helper h;"
        self.assertEqual(
            imp.rename_text(src),
            "namespace Raye {\n#define RAYE_HAVE_KSTYLE 1\nraye::Helper h;",
        )

    def test_includes_and_generated_headers(self):
        src = '#include "breezestyleconfigdata.h"\n#include "config-breeze.h"'
        self.assertEqual(
            imp.rename_text(src),
            '#include "rayestyleconfigdata.h"\n#include "config-raye.h"',
        )

    def test_app_facing_properties_are_kept(self):
        # KDE apps set these on their widgets; the style has to keep reading them.
        src = 'const char x[] = "_breeze_force_frame";\nw->property("_breeze_statusbar_separator");'
        self.assertEqual(imp.rename_text(src), src)

    def test_only_quoted_property_names_are_kept(self):
        src = "set(kstyle_breeze_config_PART_SRCS a.cpp)"
        self.assertEqual(imp.rename_text(src),
                         "set(kstyle_raye_config_PART_SRCS a.cpp)")

    def test_internal_property_is_renamed(self):
        src = 'w->setProperty("breeze_has_toolsarea_palette", true);'
        self.assertEqual(
            imp.rename_text(src),
            'w->setProperty("raye_has_toolsarea_palette", true);',
        )

    def test_decoration_plugin_id(self):
        src = "set_target_properties(breezedecoration PROPERTIES\n    OUTPUT_NAME org.kde.breeze\n)"
        self.assertEqual(
            imp.rename_text(src),
            "set_target_properties(rayedecoration PROPERTIES\n    OUTPUT_NAME org.raye.decoration\n)",
        )

    def test_dbus_names(self):
        src = 'createSignal("/BreezeDecoration", "org.kde.Breeze.Style", "reparseConfiguration")'
        self.assertEqual(
            imp.rename_text(src),
            'createSignal("/RayeDecoration", "org.raye.Style", "reparseConfiguration")',
        )

    def test_style_and_decoration_share_one_config_file(self):
        # [Common] holds the shadow settings: the decoration KCM writes
        # them and the style reads them for menus and tooltips.
        self.assertEqual(imp.rename_text('<kcfgfile name="breezerc"/>'), '<kcfgfile name="rayerc"/>')
        self.assertEqual(
            imp.rename_text('openConfig(QStringLiteral("breezerc"))'),
            'openConfig(QStringLiteral("rayerc"))',
        )

    def test_translation_domains(self):
        self.assertEqual(
            imp.rename_text('add_definitions(-DTRANSLATION_DOMAIN="breeze_kwin_deco")'),
            'add_definitions(-DTRANSLATION_DOMAIN="raye_decoration")',
        )
        self.assertEqual(
            imp.rename_text('setApplicationDomain("breeze_style_config");'),
            'setApplicationDomain("raye_style_config");',
        )

    def test_common_library(self):
        src = "include_directories(${CMAKE_SOURCE_DIR}/libbreezecommon)\ntarget_link_libraries(x breezecommon6)"
        self.assertEqual(
            imp.rename_text(src),
            "include_directories(${CMAKE_SOURCE_DIR}/libraye-common)\ntarget_link_libraries(x rayecommon6)",
        )

    def test_settings_tool_does_not_clash_with_raye_settings(self):
        src = (
            'app.setApplicationName(i18n("Breeze Settings"));\n'
            'QIcon::fromTheme(QStringLiteral("breeze-settings"))\n'
            "set_target_properties(breeze-settings PROPERTIES OUTPUT_NAME breeze-settings6)"
        )
        self.assertEqual(
            imp.rename_text(src),
            'app.setApplicationName(i18n("Raye Style Settings"));\n'
            'QIcon::fromTheme(QStringLiteral("raye-style-settings"))\n'
            "set_target_properties(raye-style-settings PROPERTIES OUTPUT_NAME raye-style-settings)",
        )

    def test_french_catalog_strings(self):
        src = 'msgstr "Paramètres de Breeze"\nmsgstr "Nouvelle exception - Configuration de Breeze"'
        self.assertEqual(
            imp.rename_text(src),
            'msgstr "Paramètres du style Raye"\nmsgstr "Nouvelle exception - Configuration du style Raye"',
        )


class RenamePath(unittest.TestCase):
    def test_paths(self):
        self.assertEqual(imp.rename_path("libbreezecommon/breezeboxshadowrenderer.cpp"),
                         "libraye-common/rayeboxshadowrenderer.cpp")
        self.assertEqual(imp.rename_path("kdecoration/config/kcm_breezedecoration.json"),
                         "kdecoration/config/kcm_rayedecoration.json")
        self.assertEqual(imp.rename_path("kstyle/config/sc-apps-breeze-settings.svgz"),
                         "kstyle/config/sc-apps-raye-style-settings.svgz")
        self.assertEqual(imp.rename_path("kstyle/config-breeze.h.cmake"), "kstyle/config-raye.h.cmake")
        self.assertEqual(imp.rename_path("po/fr/breeze_kwin_deco.po"), "po/fr/raye_decoration.po")


class Metadata(unittest.TestCase):
    def test_json_keeps_english_and_french_only(self):
        data = {
            "KPlugin": {
                "Name": "Raye", "Name[fr]": "Raye", "Name[ar]": "x", "Name[zh_CN]": "y",
                "Description": "d", "Description[fr]": "f", "Description[de]": "g",
                "EnabledByDefault": True,
            },
            "X-KDE-Keywords": "raye", "X-KDE-Keywords[fr]": "raye", "X-KDE-Keywords[tr]": "z",
            "org.kde.kdecoration3": {"blur": False},
        }
        out = imp.trim_json_translations(data)
        self.assertEqual(out["KPlugin"], {
            "Name": "Raye", "Name[fr]": "Raye", "Description": "d", "Description[fr]": "f",
            "EnabledByDefault": True,
        })
        self.assertEqual(out["X-KDE-Keywords[fr]"], "raye")
        self.assertNotIn("X-KDE-Keywords[tr]", out)
        self.assertEqual(out["org.kde.kdecoration3"], {"blur": False})
        # the input is left alone
        self.assertIn("Name[ar]", data["KPlugin"])

    def test_translated_style_name(self):
        # the French file calls Breeze by its French name
        self.assertEqual(imp.rename_text("Name[fr]=Brise\n"), "Name[fr]=Raye\n")

    def test_desktop_style_file(self):
        src = "[Misc]\nName=Raye\nName[ar]=x\nName[fr]=Raye\nComment=c\nComment[de]=d\n\n[KDE]\nWidgetStyle=Raye\n"
        self.assertEqual(
            imp.trim_ini_translations(src),
            "[Misc]\nName=Raye\nName[fr]=Raye\nComment=c\n\n[KDE]\nWidgetStyle=Raye\n",
        )


class ImportTree(unittest.TestCase):
    def _write(self, root, rel, text, mode="w"):
        path = os.path.join(root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, mode) as f:
            f.write(text)

    def test_import_small_tree(self):
        with tempfile.TemporaryDirectory() as src, tempfile.TemporaryDirectory() as dst:
            self._write(src, "CMakeLists.txt", 'project(breeze)\nset(PROJECT_VERSION "6.6.5")\n')
            self._write(src, "kstyle/breeze.json", '{ "Keys": [ "Breeze" ] }')
            self._write(src, "kstyle/breezestyle.cpp", "namespace Breeze {}\n")
            self._write(src, "kstyle/config/sc-apps-breeze-settings.svgz",
                        gzip.compress(b'<svg sodipodi:docname="breeze-settings.svg"/>', mtime=0), "wb")
            self._write(src, "kstyle/breeze-logo.png", b"\x89PNG\x00breeze", "wb")
            self._write(src, "kstyle/config/Messages.sh", "$XGETTEXT -o $podir/breeze_style_config.pot\n")
            self._write(src, "kdecoration/breeze.json",
                        '{"KPlugin": {"Name": "Breeze", "Name[ar]": "x"}, "X-KDE-ConfigModule": "kcm_breezedecoration"}')
            self._write(src, "libbreezecommon/breezeboxshadowrenderer.h", "namespace Breeze {}\n")
            self._write(src, "LICENSES/MIT.txt", "MIT\n")
            self._write(src, "po/fr/breeze_kwin_deco.po", 'msgid "Breeze"\nmsgstr "Breeze"\n')
            self._write(src, "po/de/breeze_kwin_deco.po", 'msgid "Breeze"\n')
            self._write(src, "wallpapers/x.txt", "not imported\n")

            imp.import_tree(src, dst, "4:6.6.5-1")

            self.assertEqual(json.loads(_read(os.path.join(dst, "kstyle/raye.json"))), {"Keys": ["Raye"]})
            self.assertEqual(_read(os.path.join(dst, "kstyle/rayestyle.cpp")), "namespace Raye {}\n")
            # an icon is text inside gzip: renamed, still gzipped, same bytes every run
            icon = _read(os.path.join(dst, "kstyle/config/sc-apps-raye-style-settings.svgz"), "rb")
            self.assertEqual(gzip.decompress(icon), b'<svg sodipodi:docname="raye-style-settings.svg"/>')
            self.assertEqual(icon[4:8], b"\0\0\0\0")  # gzip mtime field
            # other binary files are copied byte for byte
            self.assertEqual(_read(os.path.join(dst, "kstyle/raye-logo.png"), "rb"), b"\x89PNG\x00breeze")
            # string extraction for KDE's translation server is left behind
            self.assertFalse(os.path.exists(os.path.join(dst, "kstyle/config/Messages.sh")))
            deco = json.loads(_read(os.path.join(dst, "kdecoration/raye.json")))
            self.assertEqual(deco, {"KPlugin": {"Name": "Raye"}, "X-KDE-ConfigModule": "kcm_rayedecoration"})
            self.assertTrue(os.path.exists(os.path.join(dst, "libraye-common/rayeboxshadowrenderer.h")))
            self.assertTrue(os.path.exists(os.path.join(dst, "LICENSES/MIT.txt")))
            self.assertTrue(os.path.exists(os.path.join(dst, "po/fr/raye_decoration.po")))
            self.assertFalse(os.path.exists(os.path.join(dst, "po/de")))
            self.assertFalse(os.path.exists(os.path.join(dst, "wallpapers")))
            upstream = _read(os.path.join(dst, "UPSTREAM"))
            self.assertIn("6.6.5", upstream)
            self.assertIn("4:6.6.5-1", upstream)

    def test_refuses_to_overwrite(self):
        with tempfile.TemporaryDirectory() as src, tempfile.TemporaryDirectory() as dst:
            self._write(src, "CMakeLists.txt", 'set(PROJECT_VERSION "6.6.5")\n')
            self._write(src, "kstyle/breeze.h", "x\n")
            self._write(dst, "kstyle/raye.h", "changed by hand\n")
            with self.assertRaises(SystemExit):
                imp.import_tree(src, dst, "v")
            self.assertEqual(_read(os.path.join(dst, "kstyle/raye.h")), "changed by hand\n")


if __name__ == "__main__":
    unittest.main()
