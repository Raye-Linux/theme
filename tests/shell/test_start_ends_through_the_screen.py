# Start's Shut down, Restart and Sign out have to reach the screen that
# stays up while the session closes: lookandfeel/.../contents/logout/Logout.qml.
# With Skip the desktop is simply left showing, windows disappearing one by
# one, until the screen goes black.
#
#   python3 -m pytest -q tests/shell/test_start_ends_through_the_screen.py

import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BACKEND = os.path.join(ROOT, "shell", "plasmoids", "org.raye.start", "contents", "ui", "Backend.qml")


def calls():
    with open(BACKEND, encoding="utf-8") as f:
        return re.findall(r"sessions\.(request\w+)\(([^)]*)\)", f.read())


def test_every_session_end_goes_through_the_screen():
    found = dict(calls())
    assert set(found) == {"requestShutdown", "requestReboot", "requestLogout"}
    for name, mode in found.items():
        assert mode == "Sessions.SessionManagement.ForcePrompt", name
