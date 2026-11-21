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


def window_rules():
    return re.findall(r"windowrule \{[^}]*\}", CONF.read_text(), re.S)


def test_windows_tile_and_fill_the_screen():
    """Owner's Dell, 2026-11-21: Settings floated in the middle of the
    screen, not filling it, while Settings calls Hyprland the desktop where
    "windows line up side by side on their own". No rule floats every
    window any more, and tiled windows leave a small gap."""
    for rule in window_rules():
        if "float = yes" in rule:
            m = re.search(r"match:class\s*=\s*(.*)", rule)
            assert not m or m.group(1).strip() not in (".*", "^.*$", "^(.*)$"), rule
            assert "match:class" in rule or "match:title" in rule, rule
    general = re.search(r"general \{[^}]*\}", CONF.read_text(), re.S).group(0)
    assert re.search(r"layout\s*=\s*dwindle", general)
    gaps_in = int(re.search(r"gaps_in\s*=\s*(\d+)", general).group(1))
    gaps_out = int(re.search(r"gaps_out\s*=\s*(\d+)", general).group(1))
    assert 0 < gaps_in <= gaps_out <= 16


def test_dialogs_and_pickers_float_in_the_middle():
    rules = window_rules()
    centred = [r for r in rules if "match:float = true" in r]
    assert centred and "center = yes" in centred[0]
    pickers = [r for r in rules if "raye-pickers-float" in r][0]
    assert "Save As" in pickers and "float = yes" in pickers


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
    first = [i for i, c in enumerate(calls) if c.startswith("start-hyprland ")]
    assert len(first) == 1
    assert "systemctl --user stop raye-hyprland-session.target" in calls[first[0] + 1:]
    assert run.returncode == 3      # Hyprland's own exit status is kept


def fake_session_tools(tmp_path, other_sessions=""):
    """start-hyprland, systemctl and loginctl that only write down what
    they are asked; loginctl answers with other_sessions (lines of
    "id State=.. Type=.. Desktop=..")."""
    bindir = tmp_path / "bin"
    bindir.mkdir()
    log = tmp_path / "calls"
    for name in ("start-hyprland", "systemctl"):
        p = bindir / name
        p.write_text("#!/bin/sh\necho \"%s $*\" >> %s\nexit 0\n" % (name, log))
        p.chmod(0o755)
    table = tmp_path / "sessions"
    table.write_text(other_sessions)
    loginctl = bindir / "loginctl"
    loginctl.write_text("""#!/bin/sh
case "$1" in
show-user) echo "7 $(cut -d' ' -f1 %s | tr '\\n' ' ')" ;;
show-session) grep "^$2 " %s | cut -d' ' -f2- | tr ' ' '\\n' ;;
esac
""" % (table, table))
    loginctl.chmod(0o755)
    env = dict(os.environ, HOME=str(tmp_path), XDG_CONFIG_HOME=str(tmp_path / ".config"),
               RAYE_HYPRLAND_DATA=str(TOP / "config"), RAYE_HYPRLAND_KEYBOARD=str(START),
               PATH="%s:%s" % (bindir, os.environ["PATH"]), LANG="en_US.UTF-8",
               XDG_SESSION_ID="7")
    return env, log


def test_plasma_left_over_from_a_switch_is_stopped_first(tmp_path):
    """Owner's Dell, 2026-11-21, right after Settings switched it from
    Plasma: two Raye taskbars stacked at the bottom. The account's service
    manager outlives the switch, and plasmashell restarts by itself; on
    Hyprland it drew Plasma's taskbar next to Raye's."""
    env, log = fake_session_tools(tmp_path, "3 State=closing Type=wayland Desktop=KDE\n")
    subprocess.run(["sh", str(SESSION)], env=env, timeout=30, check=True)
    calls = log.read_text().splitlines()
    start = [i for i, c in enumerate(calls) if c.startswith("start-hyprland ")][0]
    before = calls[:start]
    assert any("stop" in c and "plasma-workspace.target" in c for c in before), calls
    assert any("stop plasma-*.service" in c for c in before), calls


def test_a_plasma_session_open_elsewhere_is_left_alone(tmp_path):
    env, log = fake_session_tools(tmp_path, "3 State=active Type=wayland Desktop=KDE\n")
    subprocess.run(["sh", str(SESSION)], env=env, timeout=30, check=True)
    calls = log.read_text().splitlines()
    assert not any("plasma-" in c for c in calls), calls


# --- the views the shell borrows from raye-shell's applets ------------------

SHELL_QML = TOP / "shell" / "qml"
PLASMOIDS = TOP.parent / "shell" / "plasmoids"


def views_installed():
    """The applets whose views raye-hyprland copies to raye/hyprland/views."""
    cmake = (TOP / "CMakeLists.txt").read_text()
    found = re.search(r"set\(RAYE_HYPR_VIEWS ([^)]*)\)", cmake)
    return set(found.group(1).split()) if found else set()


def test_every_view_of_a_compiled_applet_is_installed():
    """A compiled applet (shell/plasmoids/<id>/CMakeLists.txt) keeps its QML
    in its plugin, so Views.url finds nothing under plasma/plasmoids. Until
    2026-11-20 org.raye.clock was one: the Hyprland taskbar had no clock,
    and its centre and toasts never loaded either."""
    asked = set()
    for qml in SHELL_QML.glob("*.qml"):
        asked |= set(re.findall(r'Views\.url\("([^"]+)",\s*"([^"]+)"\)', qml.read_text()))
    assert ("org.raye.clock", "ClockButton.qml") in asked
    copied = views_installed()
    for applet, file in sorted(asked):
        assert (PLASMOIDS / applet / "contents" / "ui" / file).is_file(), (applet, file)
        if (PLASMOIDS / applet / "CMakeLists.txt").exists():
            assert applet in copied, (
                f"{applet} is compiled: {file} is not on disk unless raye-hyprland copies its views")


def test_views_are_installed_where_views_url_looks():
    cmake = (TOP / "CMakeLists.txt").read_text()
    assert re.search(r"DESTINATION \$\{KDE_INSTALL_DATADIR\}/raye/hyprland/views/\$\{_view\}/contents/ui", cmake)
    assert 'QStringLiteral("raye/hyprland/views/")' in (TOP / "shell" / "src" / "views.cpp").read_text()
    install = (TOP.parent / "debian" / "raye-hyprland.install").read_text()
    assert "usr/share/raye/hyprland/*" in install


# --- hyprland-guiutils ----------------------------------------------------

def test_no_banner_for_a_helper_ubuntu_does_not_ship():
    """Hyprland 0.53 shows "hyprland-guiutils not installed" at every start
    when the helper is missing, and Ubuntu has no such package. The check is
    turned off with Hyprland's own option, unless the package is depended on."""
    control = (TOP.parent / "debian" / "control").read_text()
    stanza = control.split("Package: raye-hyprland\n", 1)[1].split("\n\n", 1)[0]
    if "hyprland-guiutils" in stanza:
        return
    misc = re.search(r"^misc \{(.*?)^\}", CONF.read_text(), re.S | re.M).group(1)
    assert re.search(r"^\s*disable_hyprland_guiutils_check\s*=\s*true\s*$", misc, re.M), misc


def test_a_misspelt_option_is_caught():
    """The option name is checked by the Hyprland of the build: an option it
    does not know fails --verify-config, so the line above cannot be a typo
    that does nothing."""
    assert "disable_hyprland_guiutils_check" in CONF.read_text()
    with tempfile.NamedTemporaryFile("w", suffix=".conf", delete=False) as f:
        f.write(CONF.read_text().replace("disable_hyprland_guiutils_check", "disable_hyprland_guiutil_check"))
    try:
        code, out = verify(f.name)
    finally:
        os.unlink(f.name)
    assert code != 0 or "does not exist" in out, out


# --- the language and formats the user picked in Settings -----------------

def run_with_env_dump(tmp_path, localerc=None, lang="en_US.UTF-8"):
    bindir = tmp_path / "bin"
    bindir.mkdir(exist_ok=True)
    dump = tmp_path / "env"
    (bindir / "start-hyprland").write_text("#!/bin/sh\nenv > %s\n" % dump)
    (bindir / "systemctl").write_text("#!/bin/sh\nexit 0\n")
    for p in bindir.iterdir():
        p.chmod(0o755)
    config = tmp_path / ".config"
    config.mkdir(exist_ok=True)
    if localerc is not None:
        (config / "plasma-localerc").write_text(localerc)
    env = {k: v for k, v in os.environ.items() if not k.startswith("LC_")}
    env.update(HOME=str(tmp_path), XDG_CONFIG_HOME=str(config), RAYE_HYPRLAND_DATA=str(TOP / "config"),
               RAYE_HYPRLAND_KEYBOARD=str(START), PATH="%s:%s" % (bindir, os.environ["PATH"]),
               LANG=lang, LANGUAGE="")
    subprocess.run(["sh", str(SESSION)], env=env, timeout=30, check=True)
    return dict(l.split("=", 1) for l in dump.read_text().splitlines() if "=" in l)


def test_formats_picked_in_settings_reach_hyprland(tmp_path):
    """Raye Settings writes the region to plasma-localerc, which the Plasma
    session exports; the Hyprland session reads it the same way, so the
    clock is 12 or 24 hours as the user's region writes it."""
    env = run_with_env_dump(tmp_path, "[Formats]\nLANG=en_US.UTF-8\nLC_TIME=de_DE.UTF-8\n"
                                      "LC_NUMERIC=fr_CA.UTF-8\n\n[Translations]\nLANGUAGE=fr:en_US\n")
    assert env["LC_TIME"] == "de_DE.UTF-8"
    assert env["LC_NUMERIC"] == "fr_CA.UTF-8"
    assert env["LANG"] == "en_US.UTF-8"
    assert env["LANGUAGE"] == "fr:en_US"


def test_without_localerc_the_system_locale_stays(tmp_path):
    env = run_with_env_dump(tmp_path, None, lang="en_GB.UTF-8")
    assert env["LANG"] == "en_GB.UTF-8"
    assert "LC_TIME" not in env


def test_localerc_cannot_inject(tmp_path):
    env = run_with_env_dump(tmp_path, "[Formats]\nLC_TIME=de_DE.UTF-8; touch /tmp/x\n"
                                      "LD_PRELOAD=/tmp/evil.so\n[Other]\nLC_NUMERIC=xx_XX\n")
    assert "LC_TIME" not in env
    assert "LD_PRELOAD" not in env
    assert "LC_NUMERIC" not in env


# --- screen sizes follow Plasma's ---------------------------------------------

def kwin_outputs(tmp_path, outputs):
    f = tmp_path / "kwinoutputconfig.json"
    import json
    f.write_text(json.dumps([{"name": "outputs", "data": outputs},
                             {"name": "setups", "data": []}]))
    return f


def monitors(path):
    return subprocess.run(["sh", str(START), "--monitors", str(path)],
                          capture_output=True, text=True, check=True).stdout


def test_screen_scale_follows_plasma(tmp_path):
    """Hyprland picked its own scale on the owner's 1920x1080 Dell: the
    taskbar and the windows were not the size Plasma shows them."""
    f = kwin_outputs(tmp_path, [
        {"connectorName": "eDP-1", "scale": 1.25, "mode": {"width": 1920, "height": 1080}},
        {"connectorName": "HDMI-A-1", "scale": 1, "mode": {"width": 2560, "height": 1440}},
    ])
    lines = [l for l in monitors(f).splitlines() if l.startswith("monitor")]
    assert lines == ["monitor = eDP-1, preferred, auto, 1.25",
                     "monitor = HDMI-A-1, preferred, auto, 1"]


def test_a_scale_hyprland_cannot_use_is_moved_to_a_clean_one(tmp_path):
    f = kwin_outputs(tmp_path, [{"connectorName": "eDP-1", "scale": 1.15,
                                 "mode": {"width": 1920, "height": 1080}}])
    line = [l for l in monitors(f).splitlines() if l.startswith("monitor")][0]
    s = float(line.rsplit(",", 1)[1])
    assert abs(s - 1.15) <= 0.1
    assert abs(1920 / s - round(1920 / s)) < 0.001 and abs(1080 / s - round(1080 / s)) < 0.001


def test_screen_file_cannot_inject(tmp_path):
    f = kwin_outputs(tmp_path, [
        {"connectorName": "eDP-1\nexec-once = rm -rf ~", "scale": 1, "mode": {"width": 1920, "height": 1080}},
        {"connectorName": "DP-1", "scale": "2\nexec-once = x", "mode": {"width": 1920, "height": 1080}},
        {"connectorName": "DP-2", "scale": 99, "mode": {"width": 1920, "height": 1080}},
    ])
    out = monitors(f)
    assert "exec" not in out and "monitor" not in out
    bad = tmp_path / "bad.json"
    bad.write_text("{not json")
    assert "monitor" not in monitors(bad)
    assert "monitor" not in monitors(tmp_path / "none.json")


def test_raye_main_file_is_brought_up_to_date(tmp_path):
    """Accounts made before monitors.conf existed get it on their next start;
    a main file the user wrote stays untouched (test above)."""
    conf = tmp_path / ".config" / "raye-hyprland"
    conf.mkdir(parents=True)
    (conf / "hyprland.conf").write_text("# Made once by Raye. Put your own settings in user.conf: x\n"
                                        "source = /old/hyprland.conf\n")
    run_session(tmp_path)
    text = (conf / "hyprland.conf").read_text()
    assert "monitors.conf" in text and "/old/" not in text
    assert (conf / "monitors.conf").exists()
    lines = [l for l in text.splitlines() if l.startswith("source")]
    assert lines[-1].endswith("user.conf")


# --- the saved passwords (KDE wallet) ---------------------------------------
#
# Owner's Dell, Beta 2: on Hyprland, KDE's raw "KDE Wallet Service - enter
# the password for kdewallet" window. A password sign-in leaves the
# wallet's key with the wallet service (pam_kwallet), which opens it only
# once the session hands it its environment: Plasma does it with
# plasma-kwallet-pam.service, the autostart file of pam_kwallet_init says
# X-systemd-skip=true, so on Hyprland nobody did.

def run_start(tmp_path, extra_env):
    bindir = tmp_path / "bin"
    bindir.mkdir()
    log = tmp_path / "calls"
    for name in ("systemctl", "raye-hypr-shell", "hypridle"):
        p = bindir / name
        p.write_text("#!/bin/sh\necho \"%s $*\" >> %s\nexit 0\n" % (name, log))
        p.chmod(0o755)
    init = tmp_path / "pam_kwallet_init"
    init.write_text("#!/bin/sh\necho \"pam_kwallet_init $PAM_KWALLET5_LOGIN\" >> %s\n" % log)
    init.chmod(0o755)
    env = dict(os.environ, HOME=str(tmp_path), RAYE_PAM_KWALLET_INIT=str(init),
               PATH="%s:%s" % (bindir, os.environ["PATH"]))
    env.pop("PAM_KWALLET5_LOGIN", None)
    env.update(extra_env)
    subprocess.run(["sh", str(START)], env=env, timeout=30, check=True)
    return log.read_text().splitlines() if log.exists() else []


def test_a_password_sign_in_opens_the_wallet(tmp_path):
    calls = run_start(tmp_path, {"PAM_KWALLET5_LOGIN": "/run/user/7/kwallet5.socket"})
    assert "pam_kwallet_init /run/user/7/kwallet5.socket" in calls, calls
    target = [i for i, c in enumerate(calls) if "start raye-hyprland-session.target" in c]
    assert target, calls
    # before the session's apps, so they find the wallet open
    assert calls.index("pam_kwallet_init /run/user/7/kwallet5.socket") < target[0]


def test_no_wallet_step_without_a_password_sign_in(tmp_path):
    calls = run_start(tmp_path, {})
    assert not any(c.startswith("pam_kwallet_init") for c in calls), calls


def test_hyprland_display_is_not_left_to_the_next_desktop(tmp_path):
    """After Settings switches back to Plasma, the account's service manager
    is the same one: the apps D-Bus and systemd start for Plasma must not
    get Hyprland's display and socket, gone with it."""
    env, log = fake_session_tools(tmp_path)
    subprocess.run(["sh", str(SESSION)], env=env, timeout=30, check=True)
    calls = log.read_text().splitlines()
    start = [i for i, c in enumerate(calls) if c.startswith("start-hyprland ")][0]
    after = calls[start + 1:]
    unset = [c for c in after if "unset-environment" in c]
    assert unset and "HYPRLAND_INSTANCE_SIGNATURE" in unset[0] and "WAYLAND_DISPLAY" in unset[0], calls


def test_layer_surfaces_follow_their_own_screen():
    # each taskbar and wallpaper is made for one screen; without the layer
    # screen set, both taskbars showed on the first screen and none on the second
    import pathlib
    qml = pathlib.Path(__file__).resolve().parent.parent / "shell" / "qml"
    for name in ("Panel.qml", "Wallpaper.qml"):
        text = qml.joinpath(name).read_text()
        assert "LayerShell.Window.screen: screen" in text, name
