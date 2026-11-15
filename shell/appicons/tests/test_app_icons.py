#!/usr/bin/python3
"""raye-app-icons against a made-up home and system folders."""

import importlib.machinery
import importlib.util
import os
import stat
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
loader = importlib.machinery.SourceFileLoader(
    "raye_app_icons", os.path.join(HERE, "..", "raye-app-icons"))
spec = importlib.util.spec_from_loader("raye_app_icons", loader)
tool = importlib.util.module_from_spec(spec)
loader.exec_module(tool)


class Home:
    def __init__(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name
        self.data_home = self.path("home/.local/share")
        self.system = self.path("usr/share")
        self.apps = os.path.join(self.data_home, "applications")
        os.makedirs(self.apps)
        os.makedirs(os.path.join(self.system, "applications"))

    def path(self, rel):
        return os.path.join(self.root, rel)

    def icon(self, base, name, theme="hicolor"):
        d = os.path.join(base, "icons", theme, "48x48", "apps")
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, name + ".png"), "w") as f:
            f.write("png")

    def entry(self, name, text, base=None):
        p = os.path.join(base or self.apps, name)
        with open(p, "w") as f:
            f.write(text)
        return p

    def run(self):
        return tool.fix_all([self.apps], [self.data_home, self.system])

    def read(self, p):
        with open(p) as f:
            return f.read()


def app(extra):
    return "[Desktop Entry]\nType=Application\nName=Thing\n" + extra


class FixAll(unittest.TestCase):
    def setUp(self):
        self.h = Home()
        self.addCleanup(self.h.tmp.cleanup)

    def test_web_app_with_a_dead_path_gets_the_icon_it_installed(self):
        # what a browser in a sandbox writes: a path only it can see
        self.h.icon(self.h.data_home, "msedge-abc-Default")
        p = self.h.entry("web.desktop", app(
            "Exec=flatpak run --command=/app/bin/edge com.microsoft.Edge --app-id=abc\n"
            "Icon=/home/x/.var/app/com.microsoft.Edge/data/icons/hicolor/256x256/apps/msedge-abc-Default.png\n"))
        self.assertEqual(self.h.run(), [p])
        self.assertIn("\nIcon=msedge-abc-Default\n", self.h.read(p))

    def test_web_app_whose_icon_is_gone_falls_back_to_its_browser(self):
        self.h.icon(self.h.system, "com.microsoft.Edge")
        p = self.h.entry("web.desktop", app(
            "Exec=flatpak run com.microsoft.Edge --app-id=abc\n"
            "Icon=/gone/msedge-abc-Default.png\n"
            "X-Flatpak-Part-Of=com.microsoft.Edge\n"))
        self.h.run()
        self.assertIn("\nIcon=com.microsoft.Edge\n", self.h.read(p))

    def test_flatpak_run_names_the_app_when_nothing_else_does(self):
        self.h.icon(self.h.system, "org.example.App")
        p = self.h.entry("x.desktop", app(
            "Exec=flatpak 'run' '--branch=stable' 'org.example.App'\n"))
        self.h.run()
        self.assertIn("Icon=org.example.App\n", self.h.read(p))

    def test_entry_without_icon_takes_the_one_of_the_app_it_stands_for(self):
        # a browser's own "make default" entry
        self.h.icon(self.h.system, "firefox")
        p = self.h.entry("userapp-Firefox-AB12CD.desktop",
                         "[Desktop Entry]\nEncoding=UTF-8\nType=Application\nNoDisplay=true\n"
                         "Exec=/usr/lib/firefox/firefox-bin %u\nName=Firefox\n")
        self.h.run()
        self.assertIn("Icon=firefox\n", self.h.read(p))

    def test_any_browser_not_only_one_by_name(self):
        self.h.icon(self.h.system, "helium")
        p = self.h.entry("userapp-Other-1.desktop", app("Exec=/opt/x/helium %u\n"))
        self.h.run()
        self.assertIn("Icon=helium\n", self.h.read(p))

    def test_system_entry_with_the_same_program_decides(self):
        bin_dir = self.h.path("opt/app")
        os.makedirs(bin_dir)
        prog = os.path.join(bin_dir, "wrapper")
        open(prog, "w").close()
        self.h.icon(self.h.system, "fancy-name")
        self.h.entry("fancy.desktop", app("Exec=%s %%u\nIcon=fancy-name\n" % prog),
                     os.path.join(self.h.system, "applications"))
        p = self.h.entry("userapp-Thing-1.desktop", app("Exec=%s %%u\n" % prog))
        self.h.run()
        self.assertIn("Icon=fancy-name\n", self.h.read(p))

    def test_last_resort_is_a_program_icon_not_a_question_mark(self):
        p = self.h.entry("x.desktop", app("Exec=/opt/nothing/known\n"))
        self.h.run()
        self.assertIn("Icon=application-x-executable\n", self.h.read(p))

    def test_good_entries_are_left_byte_for_byte(self):
        self.h.icon(self.h.data_home, "chrome-abc-Default")
        real = self.h.path("icon.png")
        open(real, "w").close()
        by_name = self.h.entry("a.desktop", app("Exec=a\nIcon=chrome-abc-Default\n"))
        by_path = self.h.entry("b.desktop", app("Exec=b\nIcon=%s\n" % real))
        link = self.h.entry("c.desktop", "[Desktop Entry]\nType=Link\nURL=https://x/\n")
        before = [os.stat(p).st_mtime_ns for p in (by_name, by_path, link)]
        self.assertEqual(self.h.run(), [])
        self.assertEqual(before, [os.stat(p).st_mtime_ns for p in (by_name, by_path, link)])

    def test_a_second_pass_changes_nothing(self):
        self.h.icon(self.h.data_home, "msedge-abc-Default")
        self.h.entry("web.desktop", app("Exec=x\nIcon=/gone/msedge-abc-Default.png\n"))
        self.assertEqual(len(self.h.run()), 1)
        self.assertEqual(self.h.run(), [])

    def test_only_the_desktop_entry_group_is_edited(self):
        self.h.icon(self.h.data_home, "good")
        p = self.h.entry("x.desktop",
                         "[Desktop Entry]\nType=Application\nExec=x\nIcon=/gone/good.png\n"
                         "Actions=New;\n\n[Desktop Action New]\nName=New\nExec=x --new\n"
                         "Icon=/gone/action.png\n")
        self.h.run()
        text = self.h.read(p)
        self.assertIn("\nIcon=good\n", text)
        self.assertIn("Icon=/gone/action.png\n", text)
        self.assertEqual(text.count("Icon="), 2)

    def test_entry_with_no_icon_line_gets_it_inside_its_group(self):
        self.h.icon(self.h.system, "tool")
        p = self.h.entry("x.desktop",
                         "[Desktop Entry]\nType=Application\nExec=tool\n\n"
                         "[Desktop Action A]\nName=A\nExec=tool a\n")
        self.h.run()
        head, _, rest = self.h.read(p).partition("[Desktop Action A]")
        self.assertIn("Icon=tool\n", head)
        self.assertNotIn("Icon=", rest)

    def test_mode_is_kept(self):
        self.h.icon(self.h.system, "tool")
        p = self.h.entry("x.desktop", app("Exec=tool\n"))
        os.chmod(p, 0o600)
        self.h.run()
        self.assertEqual(stat.S_IMODE(os.stat(p).st_mode), 0o600)

    def test_a_link_to_a_file_elsewhere_is_not_followed(self):
        target = self.h.entry("t.txt", app("Exec=tool\n"), self.h.root)
        os.symlink(target, os.path.join(self.h.apps, "l.desktop"))
        self.assertEqual(self.h.run(), [])
        self.assertEqual(self.h.read(target), app("Exec=tool\n"))

    def test_a_broken_file_does_not_stop_the_others(self):
        self.h.icon(self.h.system, "tool")
        with open(os.path.join(self.h.apps, "bad.desktop"), "wb") as f:
            f.write(b"\xff\xfe\x00garbage")
        good = self.h.entry("good.desktop", app("Exec=tool\n"))
        self.h.run()
        self.assertIn("Icon=tool\n", self.h.read(good))

    def test_exec_that_does_not_parse_still_gets_an_icon(self):
        p = self.h.entry("x.desktop", app("Exec=\"unterminated\n"))
        self.h.run()
        self.assertIn("Icon=application-x-executable\n", self.h.read(p))

    def test_a_theme_is_read_through_its_index(self):
        theme = os.path.join(self.h.system, "icons", "Big")
        for d in ("apps/48", "apps/hidden"):
            os.makedirs(os.path.join(theme, d))
        with open(os.path.join(theme, "index.theme"), "w") as f:
            f.write("[Icon Theme]\nName=Big\nDirectories=apps/48\n")
        open(os.path.join(theme, "apps/48", "listed.svg"), "w").close()
        open(os.path.join(theme, "apps/hidden", "unlisted.svg"), "w").close()
        icons = tool.Icons([self.h.system])
        self.assertTrue(icons.has("listed"))
        self.assertFalse(icons.has("unlisted"))
        self.assertFalse(icons.has("../listed"))

    def test_missing_folder_is_not_an_error(self):
        self.assertEqual(tool.fix_all([self.h.path("nope")], [self.h.system]), [])


if __name__ == "__main__":
    unittest.main()
