# raye-taskbar-sync.service: a user unit for people's sessions only.
#
# The sign-in screen runs KWin as the sddm user, KWin loads the
# org.raye.taskbarsync script there too, and that script starts this
# service: in the greeter's own user manager, where Plasma never comes.
# Measured on 1.0 Beta 2 (VM 130, 2026-11-20): the unit failed at every
# boot ("Plasma did not start"). It now does not start for system users.

import os
import re
import shutil
import subprocess
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
UNIT = os.path.join(HERE, "..", "raye-taskbar-sync.service")


def unit_section(text, name):
    found = re.search(r"^\[%s\]\n(.*?)(?=^\[|\Z)" % re.escape(name), text, re.S | re.M)
    return found.group(1) if found else ""


class GreeterUser(unittest.TestCase):
    def test_skipped_for_system_users(self):
        with open(UNIT) as f:
            unit = unit_section(f.read(), "Unit")
        self.assertRegex(unit, r"(?m)^ConditionUser=!@system$")

    def test_condition_is_one_systemd_understands(self):
        # as root (a system user) the condition must come out false, and not
        # be refused as a typo; skipped where systemd-analyze is missing
        analyze = shutil.which("systemd-analyze")
        if not analyze or os.geteuid() != 0:
            self.skipTest("needs systemd-analyze, run as root")
        run = subprocess.run([analyze, "condition", "ConditionUser=!@system"],
                             capture_output=True, text=True)
        out = run.stdout + run.stderr
        self.assertNotEqual(run.returncode, 0, out)
        self.assertRegex(out, r"ConditionUser=!@system\S* (failed|fails)", out)


if __name__ == "__main__":
    unittest.main()
