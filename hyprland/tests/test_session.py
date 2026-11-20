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


def env_lines():
    out = {}
    for line in CONF.read_text().splitlines():
        m = re.match(r"\s*env\s*=\s*([A-Z_]+)\s*,\s*(.*)$", line)
        if m:
            out[m.group(1)] = m.group(2).strip()
    return out


def test_qt_apps_draw_raye_title_bars():
    env = env_lines()
    assert env.get("QT_WAYLAND_DECORATION") == "raye"
    # Hyprland answers "server side" and draws nothing: Qt must not ask
    hidden = env.get("QT_WAYLAND_DISABLED_INTERFACES", "").split(",")
    assert "zxdg_decoration_manager_v1" in hidden
    assert "org_kde_kwin_server_decoration_manager" in hidden


def test_maximize_button_is_not_suppressed():
    # suppress_event = maximize would make the title bar's Maximize do nothing
    assert not re.search(r"suppress_event\s*=.*maximize", CONF.read_text())


# --- the stock Hyprland sign-in entries ------------------------------------

DEBIAN = TOP.parent / "debian"


@pytest.fixture
def divert_root(tmp_path):
    """A fake system with dpkg-divert pointed at it."""
    if not shutil.which("dpkg-divert"):
        pytest.skip("dpkg-divert is not installed")
    root = tmp_path / "root"
    admin = tmp_path / "admin"
    sessions = root / "usr/share/wayland-sessions"
    sessions.mkdir(parents=True)
    admin.mkdir()
    (admin / "diversions").write_text("")
    for name in ("hyprland.desktop", "hyprland-uwsm.desktop", "raye-hyprland.desktop", "sway.desktop"):
        (sessions / name).write_text("[Desktop Entry]\nName=" + name + "\n")
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    real = shutil.which("dpkg-divert")
    wrapper = bin_dir / "dpkg-divert"
    wrapper.write_text(f'#!/bin/sh\nexec {real} --instdir "{root}" --admindir "{admin}" "$@"\n')
    wrapper.chmod(0o755)
    env = dict(os.environ, PATH=f"{bin_dir}:{os.environ['PATH']}")
    return root, sessions, env


def maintainer(script, arg, env):
    return subprocess.run(["sh", str(DEBIAN / script), arg], capture_output=True, text=True, env=env)


def test_install_hides_the_stock_sessions(divert_root):
    root, sessions, env = divert_root
    r = maintainer("raye-hyprland.preinst", "install", env)
    assert r.returncode == 0, r.stderr
    left = sorted(p.name for p in sessions.glob("*.desktop"))
    assert left == ["raye-hyprland.desktop", "sway.desktop"]
    assert (sessions / "hyprland.desktop.stock").exists()
    # an upgrade leaves things as they are
    assert maintainer("raye-hyprland.preinst", "upgrade", env).returncode == 0
    assert sorted(p.name for p in sessions.glob("*.desktop")) == left


def test_remove_gives_them_back(divert_root):
    root, sessions, env = divert_root
    assert maintainer("raye-hyprland.preinst", "install", env).returncode == 0
    r = maintainer("raye-hyprland.postrm", "remove", env)
    assert r.returncode == 0, r.stderr
    assert sorted(p.name for p in sessions.glob("*.desktop")) == [
        "hyprland-uwsm.desktop", "hyprland.desktop", "raye-hyprland.desktop", "sway.desktop"]
    assert not list(sessions.glob("*.stock"))
    # a second remove (purge after remove) finds nothing to do
    assert maintainer("raye-hyprland.postrm", "purge", env).returncode == 0


def test_postrm_leaves_other_packages_diversions(divert_root):
    root, sessions, env = divert_root
    subprocess.run(["dpkg-divert", "--package", "someone-else", "--add", "--rename", "--divert",
                    "/usr/share/wayland-sessions/hyprland.desktop.other",
                    "/usr/share/wayland-sessions/hyprland.desktop"], env=env, check=True,
                   capture_output=True)
    r = maintainer("raye-hyprland.postrm", "remove", env)
    assert r.returncode == 0, r.stderr
    assert (sessions / "hyprland.desktop.other").exists()


# --- XDG autostart on Hyprland (2026-11-20, Beta 2 ISO test on VM 130) ---
# raye-hyprland-start asked systemd for xdg-desktop-autostart.target
# directly. That target says RefuseManualStart=yes, so systemd refused
# ("may be requested by dependency only"), the error went to /dev/null and
# no autostart app ever ran on Hyprland: not Settings' desktop-restore (the
# apps of a Plasma -> Hyprland switch did not come back), not the polkit or
# geoclue helpers. A target of our own pulls it in as a dependency, and the
# session takes it down when Hyprland ends.
TARGET = TOP / "session" / "raye-hyprland-session.target"


def test_autostart_comes_through_a_target_of_our_own():
    text = TARGET.read_text()
    assert re.search(r"^Wants=.*\bxdg-desktop-autostart\.target\b", text, re.M)
    assert re.search(r"^BindsTo=.*\bgraphical-session\.target\b", text, re.M)
    assert not re.search(r"^RefuseManualStart=yes", text, re.M)
    code = [l for l in START.read_text().splitlines() if not l.lstrip().startswith("#")]
    assert any("start raye-hyprland-session.target" in l for l in code)
    # systemd refuses this one; asking for it is the defect
    assert not any("start xdg-desktop-autostart.target" in l for l in code)


def test_target_is_installed_as_a_user_unit():
    cmake = (TOP / "CMakeLists.txt").read_text()
    assert re.search(r"session/raye-hyprland-session\.target[^)]*SYSTEMDUSERUNITDIR", cmake)
    install = (TOP.parent / "debian" / "raye-hyprland.install").read_text()
    assert "usr/lib/systemd/user/raye-hyprland-session.target" in install


def test_session_takes_the_target_down_when_hyprland_ends(tmp_path):
    bindir = tmp_path / "bin"
    bindir.mkdir()
    log = tmp_path / "calls"
    for name, rc in (("start-hyprland", 3), ("systemctl", 0)):
        p = bindir / name
        p.write_text("#!/bin/sh\necho \"%s $*\" >> %s\nexit %d\n" % (name, log, rc))
        p.chmod(0o755)
    env = dict(os.environ, HOME=str(tmp_path), XDG_CONFIG_HOME=str(tmp_path / ".config"),
               RAYE_HYPRLAND_DATA=str(TOP / "config"), RAYE_HYPRLAND_KEYBOARD=str(START),
               PATH="%s:%s" % (bindir, os.environ["PATH"]), LANG="en_US.UTF-8")
    run = subprocess.run(["sh", str(SESSION)], env=env, timeout=30)
    calls = log.read_text().splitlines()
    assert calls[0].startswith("start-hyprland ")
    assert "systemctl --user stop raye-hyprland-session.target" in calls[1:]
    assert run.returncode == 3      # Hyprland's own exit status is kept
