# The terminal pieces of Raye: the Konsole profile and the fastfetch
# setup. Both must read well in Raye Night and in Raye Day.
#
#   python3 -m pytest -q tests/terminal/
#
# The profile's start command is run for real with /bin/sh, the real
# kreadconfig6 (skipped when it is not there) and a stand-in for qdbus6.

import json
import os
import re
import shlex
import shutil
import subprocess

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PROFILE = os.path.join(ROOT, "konsole", "Raye.profile")
FETCH_CONFIG = os.path.join(ROOT, "fetch", "config.jsonc")
FETCH_LOGO = os.path.join(ROOT, "fetch", "raye.txt")
TOKENS = os.path.join(ROOT, "tokens", "raye-tokens.json")
INSTALL = os.path.join(ROOT, "debian", "raye-theme.install")

OSC = b"\x1b]50;ColorScheme=%s\x07"
DARK_FGBG = "15;0"   # what Konsole puts in COLORFGBG for a dark scheme
LIGHT_FGBG = "0;15"


def unescape(value):
    """The escapes KConfig undoes when it reads a value."""
    out, i = [], 0
    while i < len(value):
        ch = value[i]
        if ch == "\\" and i + 1 < len(value):
            nxt = value[i + 1]
            out.append({"\\": "\\", "n": "\n", "t": "\t", "r": "\r", "s": " "}.get(nxt, "\\" + nxt))
            i += 2
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def parse(path):
    out, group = {}, None
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if line.startswith("["):
                assert line.endswith("]"), "%s:%d: %r" % (path, n, line)
                group = line[1:-1]
                assert group not in out, "%s:%d: [%s] twice" % (path, n, group)
                out[group] = {}
                continue
            assert group is not None, "%s:%d: key outside a group" % (path, n)
            key, sep, value = line.partition("=")
            assert sep, "%s:%d: %r" % (path, n, line)
            assert key not in out[group], "%s:%d: %s twice" % (path, n, key)
            out[group][key] = unescape(value)
    return out


def tokens():
    with open(TOKENS, encoding="utf-8") as f:
        return json.load(f)


def rgb(hexcolour):
    h = hexcolour.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def luminance(c):
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = c
    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b)


def contrast(a, b):
    la, lb = sorted((luminance(a), luminance(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


# --- Konsole profile ---------------------------------------------------

def test_profile_basics():
    p = parse(PROFILE)
    assert p["General"]["Name"] == "Raye"
    assert p["Appearance"]["ColorScheme"] == "RayeNight"
    assert p["Appearance"]["Font"].startswith("Cascadia Mono,")
    # Konsole reads the cursor shape from [Cursor Options] only:
    # 1 is the thin bar, the cursor of the Windows terminal
    assert p["Cursor Options"]["CursorShape"] == "1"
    assert "CursorShape" not in p["Appearance"]


def command():
    return shlex.split(parse(PROFILE)["General"]["Command"])


def test_command_is_sh_then_the_shell():
    argv = command()
    assert argv[:2] == ["/bin/sh", "-c"] and len(argv) == 3
    script = argv[2]
    assert script.rstrip().endswith('exec "${SHELL:-/bin/bash}"')
    # nothing in it is ever evaluated again
    assert "eval" not in script and "`" not in script
    # the choice runs in a subshell, so a failure there can never stop
    # the shell from starting
    assert script.startswith("(")


KREAD = shutil.which("kreadconfig6")
needs_kread = pytest.mark.skipif(KREAD is None, reason="kreadconfig6 not installed")


@pytest.fixture
def sandbox(tmp_path):
    """A home with its own kdeglobals, a fake qdbus6 and a fake shell."""
    home = tmp_path / "home"
    (home / ".config").mkdir(parents=True)
    (home / ".local/share/konsole").mkdir(parents=True)
    bindir = tmp_path / "bin"
    bindir.mkdir()
    qdbus = bindir / "qdbus6"
    qdbus.write_text('#!/bin/sh\n'
                     '[ "$3" = profile ] || exit 2\n'
                     'printf "%s\\n" "$FAKE_PROFILE_NAME"\n')
    qdbus.chmod(0o755)
    if KREAD:
        os.symlink(KREAD, bindir / "kreadconfig6")
    shell = tmp_path / "fakeshell"
    shell.write_text('#!/bin/sh\nprintf "SHELL-STARTED"\n')
    shell.chmod(0o755)

    class S:
        pass
    s = S()
    s.home, s.bindir, s.shell = home, bindir, shell

    def set_bg(r, g, b):
        (home / ".config/kdeglobals").write_text(
            "[Colors:Window]\nBackgroundNormal=%d,%d,%d\n" % (r, g, b))
    s.set_bg = set_bg

    def user_profile(name, scheme):
        text = "[General]\nName=%s\n" % name
        if scheme is not None:
            text += "[Appearance]\nColorScheme=%s\n" % scheme
        (home / (".local/share/konsole/%s.profile" % name)).write_text(text)
    s.user_profile = user_profile

    def run(fgbg=DARK_FGBG, profile="Raye", in_konsole=True, path=None, extra=None):
        env = {
            "HOME": str(home),
            "PATH": path if path is not None else "%s:/usr/bin:/bin" % bindir,
            "XDG_CONFIG_HOME": str(home / ".config"),
            "XDG_CONFIG_DIRS": str(tmp_path / "no-system-config"),
            "XDG_DATA_HOME": str(home / ".local/share"),
            "SHELL": str(shell),
            "FAKE_PROFILE_NAME": profile,
        }
        if fgbg is not None:
            env["COLORFGBG"] = fgbg
        if in_konsole:
            env["KONSOLE_DBUS_SERVICE"] = ":1.42"
            env["KONSOLE_DBUS_SESSION"] = "/Sessions/1"
        env.update(extra or {})
        r = subprocess.run(command(), env=env, stdin=subprocess.DEVNULL,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           timeout=30, cwd=str(home))
        assert r.returncode == 0, r.stderr
        assert r.stdout.endswith(b"SHELL-STARTED"), r.stdout
        return r.stdout[:-len(b"SHELL-STARTED")]
    s.run = run
    return s


NIGHT_WINDOW = (19, 14, 31)    # bg.base of Raye Night
DAY_WINDOW = (246, 243, 251)   # bg.base of Raye Day


@needs_kread
def test_night_system_night_terminal_says_nothing(sandbox):
    sandbox.set_bg(*NIGHT_WINDOW)
    assert sandbox.run(fgbg=DARK_FGBG) == b""


@needs_kread
def test_day_system_turns_the_terminal_to_day(sandbox):
    sandbox.set_bg(*DAY_WINDOW)
    assert sandbox.run(fgbg=DARK_FGBG) == OSC % b"RayeDay"


@needs_kread
def test_day_system_day_terminal_says_nothing(sandbox):
    sandbox.set_bg(*DAY_WINDOW)
    assert sandbox.run(fgbg=LIGHT_FGBG) == b""


@needs_kread
def test_night_system_turns_a_day_terminal_back(sandbox):
    sandbox.user_profile("Raye", "RayeDay")
    sandbox.set_bg(*NIGHT_WINDOW)
    assert sandbox.run(fgbg=LIGHT_FGBG) == OSC % b"RayeNight"


@needs_kread
def test_any_light_scheme_counts_as_day(sandbox):
    sandbox.set_bg(239, 240, 241)   # a stock light scheme
    assert sandbox.run(fgbg=DARK_FGBG) == OSC % b"RayeDay"


@needs_kread
def test_user_choice_of_another_scheme_is_kept(sandbox):
    sandbox.user_profile("Raye", "Solarized")
    sandbox.set_bg(*DAY_WINDOW)
    assert sandbox.run(fgbg=DARK_FGBG) == b""


@needs_kread
def test_users_own_profile_with_another_scheme_is_kept(sandbox):
    sandbox.user_profile("Mine", "DarkPastels")
    sandbox.set_bg(*DAY_WINDOW)
    assert sandbox.run(fgbg=DARK_FGBG, profile="Mine") == b""


@needs_kread
def test_users_own_profile_without_a_scheme_follows(sandbox):
    sandbox.user_profile("Mine", None)
    sandbox.set_bg(*DAY_WINDOW)
    assert sandbox.run(fgbg=DARK_FGBG, profile="Mine") == OSC % b"RayeDay"


@needs_kread
def test_unknown_profile_name_follows(sandbox):
    sandbox.set_bg(*DAY_WINDOW)
    assert sandbox.run(fgbg=DARK_FGBG, profile="") == OSC % b"RayeDay"


@needs_kread
def test_profile_name_is_never_run_or_walked_out_of_the_folder(sandbox, tmp_path):
    outside = tmp_path / "home/.local/share/x.profile"
    outside.write_text("[Appearance]\nColorScheme=Solarized\n")
    sandbox.set_bg(*DAY_WINDOW)
    marker = tmp_path / "ran"
    evil = '$(touch %s)"; touch %s; "' % (marker, marker)
    assert sandbox.run(fgbg=DARK_FGBG, profile=evil) == OSC % b"RayeDay"
    assert not marker.exists()
    # a name with a slash is not used as a path: ../x would read x.profile
    assert sandbox.run(fgbg=DARK_FGBG, profile="../x") == OSC % b"RayeDay"


@needs_kread
def test_outside_konsole_says_nothing(sandbox):
    sandbox.set_bg(*DAY_WINDOW)
    assert sandbox.run(fgbg=DARK_FGBG, in_konsole=False) == b""


@needs_kread
def test_odd_colour_values_say_nothing(sandbox):
    for value in ("", "white", "1,2", "019,014,031x", "1;2;3", "$(id),1,1"):
        (sandbox.home / ".config/kdeglobals").write_text(
            "[Colors:Window]\nBackgroundNormal=%s\n" % value)
        assert sandbox.run(fgbg=DARK_FGBG) == b"", value


def test_without_kreadconfig_the_shell_still_starts(sandbox):
    # only the fake qdbus6 on PATH: no kreadconfig6 at all
    kread = sandbox.bindir / "kreadconfig6"
    if kread.is_symlink():
        kread.unlink()
    assert sandbox.run(fgbg=DARK_FGBG, path=str(sandbox.bindir)) == b""


def test_without_shell_variable_bash_starts(sandbox):
    # no SHELL: the fallback is bash, which must not read our fake home's
    # rc files here, so only check the command line it would run
    r = subprocess.run(["/bin/sh", "-c", command()[2].replace("exec ", "echo ")],
                       env={"PATH": "/usr/bin:/bin", "HOME": str(sandbox.home)},
                       stdout=subprocess.PIPE, timeout=30)
    assert r.stdout == b"/bin/bash\n"


# --- fastfetch -------------------------------------------------------

def jsonc(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    # line comments only, never inside a string in this file
    text = re.sub(r'^\s*//.*$', "", text, flags=re.M)
    return json.loads(text)


ANSI = {"30": 0, "31": 1, "32": 2, "33": 3, "34": 4, "35": 5, "36": 6, "37": 7}
BRIGHT = {"90": 0, "91": 1, "92": 2, "93": 3, "94": 4, "95": 5, "96": 6, "97": 7}


def resolve(code, mode):
    """The colour a terminal running Raye's scheme draws for an SGR code."""
    t = tokens()["terminal"][mode]
    parts = code.split(";")
    assert "38" not in parts and "48" not in parts, \
        "%r is a fixed colour: it cannot follow Night and Day" % code
    colour, bold = None, "1" in parts
    for p in parts:
        if p in ANSI:
            colour = (t["ansi.intense"] if bold else t["ansi"])[ANSI[p]]
        elif p in BRIGHT:
            colour = t["ansi.intense"][BRIGHT[p]]
    assert colour is not None, code
    return rgb(colour), rgb(t["background"])


def fetch_colours():
    cfg = jsonc(FETCH_CONFIG)
    logo = cfg["logo"]["color"]
    disp = cfg["display"]["color"]
    return cfg, logo, disp


def test_fetch_logo_is_installed_where_the_config_points():
    cfg, _, _ = fetch_colours()
    with open(INSTALL, encoding="utf-8") as f:
        lines = [l.split() for l in f if l.strip() and not l.startswith("#")]
    assert ["fetch/raye.txt", "usr/share/raye/fetch"] in lines
    assert ["fetch/config.jsonc", "etc/xdg/fastfetch"] in lines
    assert cfg["logo"]["source"] == "/usr/share/raye/fetch/raye.txt"


@pytest.mark.parametrize("mode", ["night", "day"])
def test_fetch_colours_follow_the_terminal_and_read_well(mode):
    _, logo, disp = fetch_colours()
    for key, code in logo.items():
        fg, bg = resolve(code, mode)
        if key == "4":
            # the pupil: dark in both modes, a hole at night, ink by day
            assert luminance(fg) < 0.05, (mode, "pupil", code)
            continue
        # the eye is a picture: 3:1 like any graphic
        assert contrast(fg, bg) >= 3.0, (mode, "logo", key, code)
    for key in ("keys", "title"):
        fg, bg = resolve(disp[key], mode)
        assert contrast(fg, bg) >= 4.5, (mode, key, disp[key])


def test_fetch_logo_uses_only_its_four_colours():
    _, logo, _ = fetch_colours()
    assert sorted(logo) == ["1", "2", "3", "4"]
    with open(FETCH_LOGO, encoding="utf-8") as f:
        text = f.read()
    assert set(re.findall(r"\$(\S)", text)) == {"1", "2", "3", "4"}
    assert "\t" not in text and "\x1b" not in text
    lines = text.rstrip("\n").split("\n")
    assert 5 <= len(lines) <= 20
    widths = [len(re.sub(r"\$[0-9]", "", l)) for l in lines]
    assert max(widths) <= 48   # leaves room for the details in 120 columns
