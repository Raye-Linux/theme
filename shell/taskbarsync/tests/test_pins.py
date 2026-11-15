# raye-taskbar-pins: which orders it copies from the main Start button to
# the others.

import importlib.machinery
import importlib.util
import os
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
_loader = importlib.machinery.SourceFileLoader("pins", os.path.join(HERE, "..", "raye-taskbar-pins"))
_spec = importlib.util.spec_from_loader("pins", _loader)
pins = importlib.util.module_from_spec(_spec)
_loader.exec_module(pins)

MAIN = pins.client(5)
TWO = pins.client(30)


class Orderings(unittest.TestCase):
    def test_copies_each_activity(self):
        groups = {"Favorites-%s-global" % MAIN: "a,b",
                  "Favorites-%s-1bd5" % MAIN: "b,a"}
        self.assertEqual(pins.orderings(groups, MAIN, [TWO]),
                         {"Favorites-%s-global" % TWO: "a,b", "Favorites-%s-1bd5" % TWO: "b,a"})

    def test_same_order_is_not_written(self):
        groups = {"Favorites-%s-global" % MAIN: "a,b", "Favorites-%s-global" % TWO: "a,b"}
        self.assertEqual(pins.orderings(groups, MAIN, [TWO]), {})

    def test_an_id_that_starts_the_same_is_another_applet(self):
        groups = {"Favorites-%s-global" % pins.client(55): "z"}
        self.assertEqual(pins.orderings(groups, MAIN, [TWO]), {})


class Files(unittest.TestCase):
    def test_reads_kconfig_groups(self):
        import tempfile
        with tempfile.NamedTemporaryFile("w", suffix="rc", delete=False) as f:
            f.write("[Favorites-%s-global]\nordering=applications:a.desktop,applications:b.desktop\n\n[Other]\nx=1\n" % MAIN)
        try:
            self.assertEqual(pins.read_orderings(f.name),
                             {"Favorites-%s-global" % MAIN: "applications:a.desktop,applications:b.desktop"})
        finally:
            os.unlink(f.name)

    def test_no_file_no_orderings(self):
        self.assertEqual(pins.read_orderings("/nonexistent/statsrc"), {})

    def test_writes_the_copies(self):
        import tempfile
        import subprocess
        from unittest import mock
        with tempfile.TemporaryDirectory() as home:
            with open(os.path.join(home, pins.STATSRC), "w") as f:
                f.write("[Favorites-%s-global]\nordering=x,y\n" % MAIN)
            calls = []
            with mock.patch.dict(os.environ, {"XDG_CONFIG_HOME": home}), \
                    mock.patch.object(subprocess, "run", side_effect=lambda a: calls.append(a) or mock.Mock(returncode=0)):
                self.assertEqual(pins.main(["5", "30"]), 0)
            self.assertEqual(calls, [["kwriteconfig6", "--file", pins.STATSRC, "--group",
                                      "Favorites-%s-global" % TWO, "--key", "ordering", "x,y"]])

    def test_one_start_button_writes_nothing(self):
        self.assertEqual(pins.main(["5"]), 0)


if __name__ == "__main__":
    unittest.main()
