# The login script that gets GTK and Flatpak apps the Raye look
# (config/plasma-workspace/env/raye-apps-look.sh), the Flatpak overrides
# and the file dialog defaults.
#   python3 -m pytest -q -p no:cacheprovider tests/apps/

import configparser
import os
import pathlib
import pwd
import re
import shutil
import subprocess
import tempfile

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCRIPT = os.path.join(ROOT, "config", "plasma-workspace", "env", "raye-apps-look.sh")
FONTS = os.path.join(ROOT, "config", "fonts", "50-raye-fonts.conf")
OVERRIDES = os.path.join(ROOT, "config", "flatpak", "overrides")


def login(home, fonts=FONTS, as_user=None, check=True, script=None,
          shell_args=("-c", '. "$1"; env')):
    """Source the script the way the session does; return what it printed.

    as_user is a pwd entry: the script then runs as that user, which is
    the only way a mode-0500 folder means anything to a root test run."""
    env = {"HOME": str(home), "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
           "RAYE_FONTS_CONF": str(fonts)}
    extra = {}
    if as_user is not None:
        extra = dict(user=as_user.pw_uid, group=as_user.pw_gid, extra_groups=[])
    out = subprocess.run(["sh", *shell_args, "sh", script or SCRIPT], env=env,
                         capture_output=True, text=True, check=check, **extra)
    return out


def readable_copy(path, where):
    """🚨 A package build unpacks the source under /root, which is 0700:
    an unprivileged child cannot even walk into it, and the test would
    fail on the folder instead of on the thing it is about -- it did,
    in the resolute chroot. The script and the fonts file are copied
    somewhere world-traversable and read from there."""
    out = pathlib.Path(where) / os.path.basename(path)
    shutil.copyfile(path, out)
    os.chmod(out, 0o644)
    return str(out)


def unprivileged():
    """An account that mode bits actually apply to, or None.

    🚨 root ignores the write bit, so as root the read-only-home check
    below could only skip -- and EVERY machine that runs these tests is
    root: the package build chroot and the VM that drives the lab. The
    check had therefore never run anywhere, inside a green suite.
    """
    if os.geteuid() != 0:
        return None                     # the mode bits already bite
    try:
        return pwd.getpwnam("nobody")
    except KeyError:
        return None


def settings(path):
    cp = configparser.ConfigParser(interpolation=None)
    cp.read(path)
    return cp


def test_first_login_makes_both_gtk_folders(tmp_path):
    login(tmp_path)
    for v in ("3.0", "4.0"):
        ini = tmp_path / ".config" / ("gtk-" + v) / "settings.ini"
        assert settings(ini)["Settings"]["gtk-theme-name"] == "Breeze"


def test_names_the_theme_in_a_file_kded_already_wrote(tmp_path):
    d = tmp_path / ".config" / "gtk-3.0"
    d.mkdir(parents=True)
    (d / "settings.ini").write_text("[Settings]\ngtk-font-name=Inter,  10\n")
    login(tmp_path)
    cp = settings(d / "settings.ini")
    assert cp["Settings"]["gtk-theme-name"] == "Breeze"
    assert cp["Settings"]["gtk-font-name"] == "Inter,  10"


def test_a_file_without_its_group_gets_one(tmp_path):
    d = tmp_path / ".config" / "gtk-4.0"
    d.mkdir(parents=True)
    (d / "settings.ini").write_text("gtk-font-name=Inter 10\n")
    login(tmp_path)
    assert settings(d / "settings.ini")["Settings"]["gtk-theme-name"] == "Breeze"


def test_a_theme_the_user_picked_stays(tmp_path):
    d = tmp_path / ".config" / "gtk-3.0"
    d.mkdir(parents=True)
    (d / "settings.ini").write_text("[Settings]\ngtk-theme-name=Adwaita\n")
    login(tmp_path)
    text = (d / "settings.ini").read_text()
    assert text == "[Settings]\ngtk-theme-name=Adwaita\n"


def test_second_login_changes_nothing(tmp_path):
    login(tmp_path)
    before = {p: p.read_bytes() for p in tmp_path.rglob("*") if p.is_file()}
    login(tmp_path)
    after = {p: p.read_bytes() for p in tmp_path.rglob("*") if p.is_file()}
    assert before == after


def test_prints_and_exports_nothing(tmp_path):
    out = login(tmp_path)
    assert out.stderr == ""
    names = {line.split("=", 1)[0] for line in out.stdout.splitlines() if "=" in line}
    assert not {"cfg", "dir", "ini", "v", "fonts", "mine"} & names
    # the function is gone once sourced
    check = subprocess.run(["sh", "-c", '. "$1"; command -v raye_apps_look', "sh", SCRIPT],
                           env={"HOME": str(tmp_path), "PATH": "/usr/bin:/bin"},
                           capture_output=True, text=True)
    assert check.stdout == ""


def test_font_rules_reach_flatpak_apps(tmp_path):
    login(tmp_path)
    mine = tmp_path / ".config" / "fontconfig" / "conf.d" / "50-raye-fonts.conf"
    assert mine.read_bytes() == open(FONTS, "rb").read()


def test_font_rules_follow_an_update(tmp_path):
    login(tmp_path)
    newer = tmp_path / "new.conf"
    newer.write_text(open(FONTS).read().replace("Cascadia Mono", "Inter"))
    login(tmp_path, fonts=newer)
    mine = tmp_path / ".config" / "fontconfig" / "conf.d" / "50-raye-fonts.conf"
    assert mine.read_text() == newer.read_text()


def test_no_font_rules_no_copy(tmp_path):
    login(tmp_path, fonts=tmp_path / "missing.conf")
    assert not (tmp_path / ".config" / "fontconfig").exists()


def test_a_read_only_home_does_not_stop_the_session():
    # not tmp_path: pytest keeps it under /tmp/pytest-of-root with 0700
    # parents, which an unprivileged child cannot even walk into -- the
    # test would fail on the folder instead of on the thing it is about.
    user = unprivileged()
    home = pathlib.Path(tempfile.mkdtemp())
    try:
        cfg = home / ".config"
        cfg.mkdir()
        if user is not None:
            os.chmod(home, 0o755)
            for p in (home, cfg):
                os.chown(p, user.pw_uid, user.pw_gid)
        os.chmod(cfg, 0o500)
        if user is None and os.access(cfg, os.W_OK):
            pytest.skip("this account ignores the write bit and there is no"
                        " 'nobody' to borrow, so a read-only folder cannot be"
                        " made read-only here")
        # NOT "stderr is empty": the script ends with
        # `raye_apps_look >/dev/null 2>&1`, so stderr is empty whatever
        # happens and that assertion could never fail. It was the one this
        # test used to make, and the "running as root" skip meant nobody
        # ever saw that it proved nothing.
        #
        # The property that matters is the one in the name. This file is
        # SOURCED by the session, so its status is the sourcing shell's
        # status, and a shell with "set -e" stops there: every env script
        # after this one would never run because a folder in someone's home
        # was read-only. Measured before the fix: rc=1 and no marker.
        reachable = pathlib.Path(tempfile.mkdtemp())
        os.chmod(reachable, 0o755)
        try:
            out = login(home, as_user=user, check=False,
                        script=readable_copy(SCRIPT, reachable),
                        fonts=readable_copy(FONTS, reachable),
                        shell_args=("-e", "-c",
                                    '. "$1"; echo SESSION-CONTINUES'))
        finally:
            shutil.rmtree(reachable, ignore_errors=True)
        assert out.stdout.splitlines()[-1:] == ["SESSION-CONTINUES"], (
            "sourcing the script under `sh -e` with a read-only ~/.config"
            " stopped the shell: the session would lose every env script"
            " after this one")
    finally:
        os.chmod(home, 0o700)
        shutil.rmtree(home, ignore_errors=True)


def flatpak_options():
    with open(OVERRIDES, encoding="utf-8") as f:
        return [l.strip() for l in f if l.strip() and not l.startswith("#")]


def test_overrides_pass_the_stores_filter():
    # the same checks as raye-store.postinst: only --filesystem= and
    # --env=, and only plain characters
    for option in flatpak_options():
        assert option.startswith(("--filesystem=", "--env="))
        assert re.fullmatch(r"[A-Za-z0-9_./:=+~-]+", option), option


def test_overrides_are_read_only():
    for option in flatpak_options():
        if option.startswith("--filesystem="):
            assert option.endswith(":ro"), option


def test_overrides_cover_gtk_and_fonts():
    assert set(flatpak_options()) >= {
        "--filesystem=xdg-config/gtk-3.0:ro",
        "--filesystem=xdg-config/gtk-4.0:ro",
        "--filesystem=xdg-config/fontconfig:ro",
    }


def test_file_dialog_places_are_small():
    cp = configparser.ConfigParser(interpolation=None, strict=False)
    cp.optionxform = str
    cp.read(os.path.join(ROOT, "config", "kdeglobals"))
    g = cp["KFileDialog Settings"]
    assert g["Places Icons Auto-resize"] == "false"
    assert g["Places Icons Static Size"] == "16"


def test_everything_here_is_installed():
    with open(os.path.join(ROOT, "debian", "raye-theme.install"), encoding="utf-8") as f:
        text = f.read()
    for path in ("config/plasma-workspace/env/raye-apps-look.sh",
                 "config/flatpak/overrides",
                 "colors/editor/RayeNight.theme",
                 "colors/editor/RayeDay.theme"):
        assert re.search(r"^%s " % re.escape(path), text, re.M), path
