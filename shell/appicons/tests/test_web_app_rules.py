#!/usr/bin/python3
"""raye-app-icons: the window rules that tie a site installed as an app
to its own entry, so the taskbar shows the site's icon, not the browser's."""

import importlib.machinery
import importlib.util
import os
import re
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
loader = importlib.machinery.SourceFileLoader(
    "raye_app_icons", os.path.join(HERE, "..", "raye-app-icons"))
spec = importlib.util.spec_from_loader("raye_app_icons", loader)
tool = importlib.util.module_from_spec(spec)
loader.exec_module(tool)

ID = "abcdefghijklmnopabcdefghijklmnop"
OTHER = "ponmlkjihgfedcbaponmlkjihgfedcba"


def web_entry(app=ID, name="Snapchat", exec_line=None):
    exec_line = exec_line or ("/opt/brave.com/brave/brave-browser "
                              "--profile-directory=Default --app-id=%s" % app)
    return ("#!/usr/bin/env xdg-open\n[Desktop Entry]\nVersion=1.0\nTerminal=false\n"
            "Type=Application\nName=%s\nExec=%s\nIcon=brave-%s-Default\n"
            "StartupWMClass=crx_%s\n" % (name, exec_line, app, app))


class Home:
    def __init__(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.apps = os.path.join(self.tmp.name, "applications")
        self.config = os.path.join(self.tmp.name, "config")
        os.makedirs(self.apps)
        os.makedirs(self.config)
        self.rc = os.path.join(self.config, "kwinrulesrc")

    def entry(self, name, text):
        p = os.path.join(self.apps, name)
        with open(p, "w") as f:
            f.write(text)
        return p

    def sync(self):
        return tool.sync_web_app_rules([self.apps], self.rc)

    def read(self):
        with open(self.rc) as f:
            return f.read()


def groups(text):
    """{group: {key: value}} of a KConfig file."""
    out = {}
    current = None
    for line in text.splitlines():
        if line.startswith("["):
            current = out.setdefault(line.strip()[1:-1], {})
        elif current is not None and "=" in line:
            k, v = line.split("=", 1)
            current[k] = v
    return out


def matches(pattern, resource_name, resource_class):
    # KWin compares "name class" when wmclasscomplete is set
    return re.search(pattern, resource_name + " " + resource_class) is not None


class WebApp(unittest.TestCase):
    def test_brave_entry(self):
        keys = tool.read_entry(web_entry())
        self.assertEqual(tool.web_app("brave-%s-Default" % ID, keys), ("brave", ID, "Default"))

    def test_other_profile(self):
        keys = tool.read_entry(web_entry())
        self.assertEqual(tool.web_app("chrome-%s-Profile_1" % ID, keys), ("chrome", ID, "Profile_1"))

    def test_sandboxed_browser_name_keeps_the_last_part(self):
        keys = tool.read_entry(web_entry(exec_line=(
            "flatpak run --command=brave com.brave.Browser --profile-directory=Default --app-id=" + ID)))
        self.assertEqual(tool.web_app("com.brave.Browser.flextop.brave-%s-Default" % ID, keys),
                         ("brave", ID, "Default"))

    def test_not_a_web_app(self):
        keys = tool.read_entry("[Desktop Entry]\nType=Application\nExec=gimp %U\n")
        self.assertIsNone(tool.web_app("gimp", keys))

    def test_file_name_without_the_id_is_left_alone(self):
        keys = tool.read_entry(web_entry())
        self.assertIsNone(tool.web_app("snapchat", keys))

    def test_odd_characters_are_left_alone(self):
        keys = tool.read_entry(web_entry())
        self.assertIsNone(tool.web_app("brave-%s-Prof[1]" % ID, keys))


class Pattern(unittest.TestCase):
    def setUp(self):
        self.p = tool.window_pattern("brave", ID, "Default")

    def test_no_backslash(self):
        # KConfig reads \s, \t... as escapes
        self.assertNotIn("\\", self.p)

    def test_windows_of_the_site(self):
        # X11 and XWayland: WM_CLASS name, with one or two underscores
        self.assertTrue(matches(self.p, "crx_" + ID, "brave-browser"))
        self.assertTrue(matches(self.p, "crx__" + ID, "brave-browser"))
        # Wayland: the executable, then the app id
        self.assertTrue(matches(self.p, "brave", "brave-%s-Default" % ID))
        self.assertTrue(matches(self.p, "brave", "brave-_%s-Default" % ID))
        self.assertTrue(matches(self.p, "brave", "com.brave.Browser.flextop.brave-_%s-Default" % ID))
        self.assertTrue(matches(self.p, "brave", "Brave-_%s-Default" % ID.upper()))

    def test_not_the_browser_itself(self):
        self.assertFalse(matches(self.p, "brave", "brave-browser"))
        self.assertFalse(matches(self.p, "brave", "com.brave.Browser"))
        self.assertFalse(matches(self.p, "brave-browser", "brave-browser"))

    def test_not_another_site_profile_or_browser(self):
        self.assertFalse(matches(self.p, "crx__" + OTHER, "brave-browser"))
        self.assertFalse(matches(self.p, "brave", "brave-_%s-Default" % OTHER))
        self.assertFalse(matches(self.p, "brave", "brave-_%s-Profile_1" % ID))
        self.assertFalse(matches(self.p, "brave", "brave-_%s-Default2" % ID))
        self.assertFalse(matches(self.p, "chrome", "chrome-_%s-Default" % ID))


class Rules(unittest.TestCase):
    def setUp(self):
        self.h = Home()
        self.addCleanup(self.h.tmp.cleanup)

    def test_site_gets_a_rule_that_forces_its_entry(self):
        self.h.entry("brave-%s-Default.desktop" % ID, web_entry())
        self.assertTrue(self.h.sync())
        g = groups(self.h.read())
        name = "raye-webapp-brave-%s-Default" % ID
        self.assertEqual(g["General"]["rules"], name)
        self.assertEqual(g["General"]["count"], "1")
        rule = g[name]
        self.assertEqual(rule["desktopfile"], "brave-%s-Default" % ID)
        self.assertEqual(rule["desktopfilerule"], "2")   # force
        self.assertEqual(rule["wmclassmatch"], "3")      # regular expression
        self.assertEqual(rule["wmclasscomplete"], "true")
        self.assertEqual(rule["Description"], "Snapchat")
        self.assertTrue(matches(rule["wmclass"], "crx__" + ID, "brave-browser"))

    def test_entry_put_there_by_a_link_counts(self):
        target = os.path.join(self.h.tmp.name, "portal.desktop")
        with open(target, "w") as f:
            f.write(web_entry())
        os.symlink(target, os.path.join(self.h.apps, "brave-%s-Default.desktop" % ID))
        self.h.sync()
        self.assertIn("raye-webapp-brave-%s-Default" % ID, groups(self.h.read()))

    def test_nothing_to_do_writes_nothing(self):
        self.h.entry("gimp.desktop", "[Desktop Entry]\nType=Application\nExec=gimp\n")
        self.assertFalse(self.h.sync())
        self.assertFalse(os.path.exists(self.h.rc))

    def test_user_file_is_not_touched_without_sites(self):
        text = "# mine\n[General]\nrules=a\ncount=1\n\n\n[a]\nwmclass=x\n"
        with open(self.h.rc, "w") as f:
            f.write(text)
        self.assertFalse(self.h.sync())
        self.assertEqual(self.h.read(), text)

    def test_second_run_changes_nothing(self):
        self.h.entry("brave-%s-Default.desktop" % ID, web_entry())
        self.assertTrue(self.h.sync())
        before = os.stat(self.h.rc).st_mtime_ns
        self.assertFalse(self.h.sync())
        self.assertEqual(before, os.stat(self.h.rc).st_mtime_ns)

    def test_user_rules_are_kept_first_and_untouched(self):
        mine = ("[General]\ncount=2\nrules=3f1c,7a2e\n\n"
                "[3f1c]\nDescription=Terminal\nabove=true\naboverule=3\n"
                "wmclass=konsole\nwmclassmatch=1\n\n"
                "[7a2e]\nDescription=Calc\nwmclass=calc\nwmclassmatch=1\n")
        with open(self.h.rc, "w") as f:
            f.write(mine)
        self.h.entry("brave-%s-Default.desktop" % ID, web_entry())
        self.h.sync()
        text = self.h.read()
        g = groups(text)
        self.assertEqual(g["General"]["rules"], "3f1c,7a2e,raye-webapp-brave-%s-Default" % ID)
        self.assertEqual(g["General"]["count"], "3")
        self.assertIn("[3f1c]\nDescription=Terminal\nabove=true\naboverule=3\n"
                      "wmclass=konsole\nwmclassmatch=1\n", text)
        self.assertEqual(g["7a2e"], {"Description": "Calc", "wmclass": "calc", "wmclassmatch": "1"})

    def test_old_numbered_rules_are_kept(self):
        # a file from before the rules list: groups 1..count
        with open(self.h.rc, "w") as f:
            f.write("[General]\ncount=1\n\n[1]\nwmclass=x\nwmclassmatch=1\n")
        self.h.entry("brave-%s-Default.desktop" % ID, web_entry())
        self.h.sync()
        g = groups(self.h.read())
        self.assertEqual(g["General"]["rules"], "1,raye-webapp-brave-%s-Default" % ID)
        self.assertEqual(g["1"], {"wmclass": "x", "wmclassmatch": "1"})

    def test_removed_site_loses_its_rule(self):
        p = self.h.entry("brave-%s-Default.desktop" % ID, web_entry())
        self.h.entry("brave-%s-Default.desktop" % OTHER, web_entry(OTHER, "Maps"))
        self.h.sync()
        os.remove(p)
        self.assertTrue(self.h.sync())
        g = groups(self.h.read())
        self.assertEqual(g["General"]["rules"], "raye-webapp-brave-%s-Default" % OTHER)
        self.assertNotIn("raye-webapp-brave-%s-Default" % ID, g)

    def test_last_site_gone_leaves_user_rules_only(self):
        with open(self.h.rc, "w") as f:
            f.write("[General]\ncount=1\nrules=u1\n\n[u1]\nwmclass=x\n")
        p = self.h.entry("brave-%s-Default.desktop" % ID, web_entry())
        self.h.sync()
        os.remove(p)
        self.h.sync()
        self.assertEqual(groups(self.h.read()),
                         {"General": {"count": "1", "rules": "u1"}, "u1": {"wmclass": "x"}})

    def test_name_cannot_break_the_file(self):
        self.h.entry("brave-%s-Default.desktop" % ID, web_entry(name="Snap\\chat [beta]"))
        self.h.sync()
        rule = groups(self.h.read())["raye-webapp-brave-%s-Default" % ID]
        self.assertEqual(rule["Description"], "Snapchat [beta]")

    def test_file_stays_private(self):
        self.h.entry("brave-%s-Default.desktop" % ID, web_entry())
        self.h.sync()
        self.assertEqual(os.stat(self.h.rc).st_mode & 0o077, 0)


if __name__ == "__main__":
    unittest.main()
