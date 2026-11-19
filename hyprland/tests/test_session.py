"""The Raye Hyprland session: its settings parse in the Hyprland of the
build, every Windows key is bound, and the session script writes what
Hyprland and the lock screen read."""

import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

TOP = Path(__file__).resolve().parent.parent
CONF = TOP / "config" / "hyprland.conf"
SESSION = TOP / "session" / "raye-hyprland-session"
START = TOP / "session" / "raye-hyprland-start"
DESKTOP = TOP / "session" / "raye-hyprland.desktop"


def verify(path):
    hyprland = shutil.which("Hyprland")
    if not hyprland:
        pytest.skip("Hyprland is not installed")
    args = [hyprland, "--verify-config", "-c", str(path)]
    if os.geteuid() == 0:
        args.insert(1, "--i-am-really-stupid")
    with tempfile.TemporaryDirectory() as runtime:
        os.chmod(runtime, 0o700)
        env = dict(os.environ, XDG_RUNTIME_DIR=runtime)
        run = subprocess.run(args, capture_output=True, text=True, env=env, timeout=60)
    return run.returncode, run.stdout + run.stderr


def test_config_parses():
    code, out = verify(CONF)
    assert code == 0 and "config ok" in out, out


def test_a_wrong_line_is_caught():
    # the check itself: an unknown dispatcher must fail
    with tempfile.NamedTemporaryFile("w", suffix=".conf", delete=False) as f:
        f.write(CONF.read_text() + "\nbind = SUPER, F12, notadispatcher,\n")
    try:
        code, out = verify(f.name)
    finally:
        os.unlink(f.name)
    assert code != 0, out


def test_shell_dispatchers_exist():
    # what raye-hypr-shell sends over the socket, checked as binds
    lines = ["fullscreenstate, 1 -1", "alterzorder, top", "movewindowpixel, exact 1 2",
             "resizewindowpixel, exact 1 2", "setfloating,", "focuswindow, address:0x1",
             "movetoworkspacesilent, special:raye-minimized", "closewindow, address:0x1",
             "workspace, 1"]
    with tempfile.NamedTemporaryFile("w", suffix=".conf", delete=False) as f:
        for i, line in enumerate(lines):
            f.write(f"bind = SUPER CTRL ALT, F{i + 1}, {line}\n")
    try:
        code, out = verify(f.name)
    finally:
        os.unlink(f.name)
    assert code == 0 and "config ok" in out, out


def binds():
    out = {}
    for line in CONF.read_text().splitlines():
        m = re.match(r"\s*bind[a-z]*\s*=\s*([^,]*),\s*([^,]*),\s*(.*)$", line)
        if m:
            out.setdefault((m.group(1).strip(), m.group(2).strip()), []).append(m.group(3).strip())
    return out


@pytest.mark.parametrize("mods,key,wanted", [
    ("SUPER", "SUPER_L", "--toggle start"),
    ("SUPER", "E", "--launch org.kde.dolphin.desktop"),
    ("SUPER", "D", "--desktop"),
    ("SUPER", "L", "lock-session"),
    ("ALT", "Tab", "cyclenext"),
    ("SUPER", "left", "--snap left"),
    ("SUPER", "right", "--snap right"),
    ("SUPER", "up", "--snap up"),
    ("SUPER", "down", "--snap down"),
    ("SUPER SHIFT", "S", "raye-hypr-screenshot region"),
    ("SUPER", "A", "--toggle quick"),
    ("SUPER", "S", "--toggle search"),
    ("SUPER", "I", "org.raye.settings.desktop"),
    ("ALT", "F4", "killactive"),
    ("SUPER", "1", "--task 1"),
])
def test_windows_keys(mods, key, wanted):
    got = binds().get((mods, key), [])
    assert any(wanted in b for b in got), (mods, key, got)


def test_windows_float_and_open_centred():
    text = CONF.read_text()
    rule = re.search(r"windowrule \{[^}]*name = raye-float[^}]*\}", text, re.S).group(0)
    assert "float = yes" in rule and "center = yes" in rule


def test_desktop_file():
    text = DESKTOP.read_text()
    assert "Exec=/usr/bin/raye-hyprland-session" in text
    assert "Name[fr]=" in text and "Comment[fr]=" in text
    assert "Hyprland" in re.search(r"DesktopNames=(.*)", text).group(1)


def run_session(home, lang="en_US.UTF-8", keyboard=None):
    env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / ".config"),
               RAYE_HYPRLAND_DATA=str(TOP / "config"), RAYE_HYPRLAND_KEYBOARD=str(START),
               RAYE_HYPRLAND_DRY_RUN="1", LANG=lang, LANGUAGE="", LC_ALL="", LC_MESSAGES="")
    if keyboard:
        env["RAYE_KEYBOARD_FILE"] = str(keyboard)
    subprocess.run(["sh", str(SESSION)], check=True, env=env, timeout=30)
    return home / ".config" / "raye-hyprland"


def test_session_writes_its_files(tmp_path):
    kb = tmp_path / "keyboard"
    kb.write_text('XKBMODEL="pc105"\nXKBLAYOUT="ca,us"\nXKBVARIANT="fr,"\nXKBOPTIONS="grp:alt_shift_toggle"\n')
    conf = run_session(tmp_path, keyboard=kb)
    main = (conf / "hyprland.conf").read_text()
    lines = [l for l in main.splitlines() if l.startswith("source")]
    assert lines[0].endswith("config/hyprland.conf")
    assert lines[-1].endswith("user.conf")
    keyboard = (conf / "keyboard.conf").read_text()
    assert "kb_layout = ca,us" in keyboard
    assert "kb_variant = fr," in keyboard
    assert "kb_options = grp:alt_shift_toggle" in keyboard
    assert "PIN or password" in (conf / "hyprlock.conf").read_text()


def test_session_keeps_the_users_file(tmp_path):
    conf = run_session(tmp_path)
    (conf / "user.conf").write_text("general { border_size = 3 }\n")
    (conf / "hyprland.conf").write_text("# mine\n")
    run_session(tmp_path)
    assert (conf / "user.conf").read_text() == "general { border_size = 3 }\n"
    assert (conf / "hyprland.conf").read_text() == "# mine\n"


def test_session_uses_the_raye_menu():
    assert "export XDG_MENU_PREFIX=raye-" in SESSION.read_text()


def test_lock_screen_in_french(tmp_path):
    conf = run_session(tmp_path, lang="fr_CA.UTF-8")
    text = (conf / "hyprlock.conf").read_text()
    assert "Code PIN ou mot de passe" in text
    assert "PIN or password" not in text


def test_keyboard_without_file_is_us(tmp_path):
    out = subprocess.run(["sh", str(START), "--keyboard", str(tmp_path / "none")],
                         capture_output=True, text=True, check=True).stdout
    assert "kb_layout = us" in out


def test_keyboard_line_cannot_inject(tmp_path):
    kb = tmp_path / "keyboard"
    kb.write_text('XKBLAYOUT="us\nexec-once = rm -rf ~"\nXKBOPTIONS="a;b}"\n')
    out = subprocess.run(["sh", str(START), "--keyboard", str(kb)],
                         capture_output=True, text=True, check=True).stdout
    assert "exec" not in out and ";" not in out and out.count("}") == 1


def test_whole_session_config_parses(tmp_path):
    conf = run_session(tmp_path)
    code, out = verify(conf / "hyprland.conf")
    assert code == 0 and "config ok" in out, out
