#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 techoraye
# SPDX-License-Identifier: GPL-2.0-or-later
"""Tests for check_names.py on small made-up trees."""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_names  # noqa: E402

GOOD = {
    "kstyle/raye.json": '{ "Keys": [ "Raye" ] }',
    "kstyle/raye.kcfg": '<kcfgfile name="rayerc"/>\n<group name="Common"/>\n<group name="Style"/>',
    "kstyle/raye.themerc": "[KDE]\nWidgetStyle=Raye\nConfigPage=kstyle_config/rayestyleconfig\n",
    "kstyle/rayepropertynames.cpp": 'const char forceFrame[] = "_breeze_force_frame";',
    "kstyle/config/CMakeLists.txt": 'add_definitions(-DTRANSLATION_DOMAIN=\\"raye_style_config\\")',
    "kdecoration/CMakeLists.txt": 'add_definitions(-DTRANSLATION_DOMAIN="raye_decoration")\n'
                                  "    OUTPUT_NAME org.raye.decoration\n",
    "kdecoration/raye.json": '{"KPlugin": {"Name": "Raye"}, "X-KDE-ConfigModule": "kcm_rayedecoration"}',
    "kdecoration/rayesettingsdata.kcfg": '<kcfgfile name="rayerc"/>\n<group name="Common"/>\n<group name="Windeco"/>',
    "kdecoration/rayesettingsprovider.cpp": 'openConfig(QStringLiteral("rayerc"))',
    "kdecoration/config/rayeconfigwidget.cpp": 'openConfig(QStringLiteral("rayerc"))',
    "kdecoration/config/kcm_rayedecoration.json": '{"KPlugin": {"Name": "Raye Window Decoration"}}',
    "libraye-common/rayeboxshadowrenderer.h": "namespace Raye {}",
    "po/fr/raye_decoration.po": 'msgid "x"',
    "po/fr/raye_style_config.po": 'msgid "x"',
    # documentation may name the base it comes from
    "README.md": "Forked from Breeze.",
    "UPSTREAM": "Imported from KDE Breeze 6.6.5",
    "cmake/import_upstream.py": "breeze",
}


class CheckNames(unittest.TestCase):
    def tree(self, files):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        for rel, text in files.items():
            path = os.path.join(d.name, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
        return d.name

    def test_good_tree_passes(self):
        self.assertEqual(check_names.problems(self.tree(GOOD)), [])

    def test_leftover_word_in_code(self):
        files = dict(GOOD)
        files["kstyle/rayestyle.cpp"] = "namespace Breeze {}"
        found = check_names.problems(self.tree(files))
        self.assertEqual(len(found), 1)
        self.assertIn("kstyle/rayestyle.cpp:1", found[0])

    def test_leftover_word_in_file_name(self):
        files = dict(GOOD)
        files["kstyle/breezehelper.h"] = "namespace Raye {}"
        found = check_names.problems(self.tree(files))
        self.assertTrue(any("kstyle/breezehelper.h" in p for p in found))

    def test_unquoted_property_prefix_is_not_allowed(self):
        files = dict(GOOD)
        files["kstyle/x.cpp"] = "int _breeze_thing = 0;"
        self.assertEqual(len(check_names.problems(self.tree(files))), 1)

    def test_style_key_must_be_raye(self):
        files = dict(GOOD)
        files["kstyle/raye.json"] = '{ "Keys": [ "Fusion" ] }'
        self.assertTrue(any("style key" in p for p in check_names.problems(self.tree(files))))

    def test_decoration_reads_the_shared_config_file(self):
        files = dict(GOOD)
        files["kdecoration/rayesettingsprovider.cpp"] = 'openConfig(QStringLiteral("rayedecorationrc"))'
        self.assertTrue(any("decoration reads rayerc" in p for p in check_names.problems(self.tree(files))))

    def test_a_group_split_over_two_files_is_found(self):
        # the style would read shadow settings nobody ever writes
        files = dict(GOOD)
        files["kdecoration/rayesettingsdata.kcfg"] = (
            '<kcfgfile name="rayedecorationrc"/>\n<group name="Common"/>\n<group name="Windeco"/>')
        found = check_names.problems(self.tree(files))
        self.assertTrue(any(p.startswith("group [Common]") for p in found), found)
        self.assertFalse(any(p.startswith("group [Windeco]") for p in found), found)

    def test_missing_plugin_id(self):
        files = dict(GOOD)
        files["kdecoration/CMakeLists.txt"] = 'add_definitions(-DTRANSLATION_DOMAIN="raye_decoration")\n'
        self.assertTrue(any("org.raye.decoration" in p for p in check_names.problems(self.tree(files))))

    def test_comments_may_say_where_the_code_comes_from(self):
        files = dict(GOOD)
        files["kstyle/rayestyle.cpp"] = ("/* started from Breeze */\n"
                                         "namespace Raye {} // was Breeze\n")
        files["kstyle/CMakeLists.txt"] = "# Breeze builds this too\nadd_library(raye6)\n"
        files["kdecoration/config/ui/x.ui"] = "<!-- Breeze --><ui/>\n"
        self.assertEqual(check_names.problems(self.tree(files)), [])

    def test_code_after_a_comment_marker_is_still_checked(self):
        files = dict(GOOD)
        files["kstyle/rayestyle.cpp"] = "// note\nnamespace Breeze {}\n"
        found = check_names.problems(self.tree(files))
        self.assertEqual(len(found), 1)
        # the line number is the one in the file, comments included
        self.assertIn("kstyle/rayestyle.cpp:2", found[0])

    def test_a_marker_inside_a_string_does_not_hide_the_rest(self):
        files = dict(GOOD)
        files["kstyle/rayehelper.cpp"] = 'icon("a//b"); open("breezerc");\n'
        self.assertEqual(len(check_names.problems(self.tree(files))), 1)

    def test_the_word_in_a_file_name_is_found_even_in_a_comment_only_file(self):
        files = dict(GOOD)
        files["kstyle/breeze.cpp"] = "// nothing\n"
        self.assertTrue(any("kstyle/breeze.cpp" in p for p in check_names.problems(self.tree(files))))

    def test_a_cmake_build_folder_in_the_tree_is_left_out(self):
        # cmake -B build inside style/: the binaries keep the widget
        # property names, which is right, and must not fail the check.
        files = dict(GOOD)
        files["build/CMakeCache.txt"] = "CMAKE_PROJECT_NAME:STATIC=raye\n"
        files["build/bin/breeze-leftover.txt"] = "namespace Breeze {}"
        files["kstyle/out/CMakeCache.txt"] = ""
        files["kstyle/out/x.cpp"] = "namespace Breeze {}"
        self.assertEqual(check_names.problems(self.tree(files)), [])

    def test_a_folder_named_build_without_a_cmake_cache_is_still_checked(self):
        files = dict(GOOD)
        files["build/x.cpp"] = "namespace Breeze {}"
        found = check_names.problems(self.tree(files))
        self.assertEqual(len(found), 1)
        self.assertIn("build/x.cpp:1", found[0])


if __name__ == "__main__":
    unittest.main()
