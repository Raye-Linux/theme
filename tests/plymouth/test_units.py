# SPDX-License-Identifier: GPL-3.0-or-later
"""The systemd pieces around the boot screen: no ring frozen mid-turn when
the sign-in screen takes over, and no TPM watch left behind."""
import configparser
import os
import re
import shlex
import subprocess
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[2]
UNITS = REPO / "plymouth" / "systemd"
QUIT = UNITS / "plymouth-quit.service.d" / "raye-retain-splash.conf"
INSTALL = REPO / "debian" / "plymouth-theme-raye.install"
SCRIPT = REPO / "plymouth" / "raye" / "raye.script.in"
HOOK = REPO / "initramfs" / "raye-plymouth"


def service(path, key):
    """The last value of key in [Service], the way systemd reads a drop-in."""
    values = []
    for line in path.read_text().splitlines():
        if line.startswith(key + "="):
            values.append(line[len(key) + 1:])
    assert values, f"{key} missing in {path.name}"
    return values[-1]


def fake_tools(tmp_path, active=False, job=""):
    """systemctl that answers like systemd 257 while plymouth-quit runs:
    sddm is ordered after it, so at most it has a start job waiting.
    That job is listed under sddm.service, never under the alias, while
    show follows the alias. plymouth logs what it is asked."""
    b = tmp_path / "bin"
    b.mkdir()
    log = tmp_path / "calls"
    jobline = f"{job} sddm.service start waiting" if job else ""
    (b / "systemctl").write_text(
        "#!/bin/sh\n"
        f'echo "systemctl $*" >> "{log}"\n'
        'case "$*" in\n'
        f'  *"is-active display-manager.service"*) exit {0 if active else 3} ;;\n'
        "  *is-active*) exit 3 ;;\n"
        f'  *"list-jobs"*"sddm.service"*) [ -n "{jobline}" ] && echo "{jobline}"; exit 0 ;;\n'
        "  *list-jobs*) exit 0 ;;\n"
        f'  *"-P Job display-manager.service"*) echo "{job}"; exit 0 ;;\n'
        '  *"-P Id display-manager.service"*) echo sddm.service; exit 0 ;;\n'
        "  *) exit 0 ;;\nesac\n")
    (b / "plymouth").write_text(f'#!/bin/sh\necho "plymouth $*" >> "{log}"\n')
    for f in b.iterdir():
        f.chmod(0o755)
    return b, log


def run_line(value, bindir):
    """Run an Exec line from a unit with the tools of bindir in place of
    the absolute ones."""
    value = value.lstrip("-").replace("$$", "$")
    argv = shlex.split(value)
    argv = [re.sub(r"^/usr/bin/plymouth$", str(bindir / "plymouth"), a) for a in argv]
    argv = [a.replace("/usr/bin/plymouth", str(bindir / "plymouth")) for a in argv]
    env = dict(os.environ, PATH=f"{bindir}:/usr/bin:/bin")
    return subprocess.run(argv, env=env, capture_output=True, text=True)


def calls(log):
    return [l for l in log.read_text().splitlines() if l.startswith("plymouth")] if log.exists() else []


def test_drop_in_ships():
    lines = [l.split() for l in INSTALL.read_text().splitlines() if l.strip()]
    assert ["plymouth/systemd/plymouth-quit.service.d", "usr/lib/systemd/system"] in lines


RETAIN = ["plymouth display-message --text=raye:handoff", "plymouth quit --retain-splash"]


def test_sign_in_screen_waiting_keeps_the_splash(tmp_path):
    bindir, log = fake_tools(tmp_path, job="7")
    r = run_line(service(QUIT, "ExecStart"), bindir)
    assert r.returncode == 0, r.stderr
    assert calls(log) == RETAIN


def test_sign_in_screen_running_keeps_the_splash(tmp_path):
    bindir, log = fake_tools(tmp_path, active=True)
    r = run_line(service(QUIT, "ExecStart"), bindir)
    assert r.returncode == 0, r.stderr
    assert calls(log) == RETAIN


def test_text_boot_gets_the_console_back(tmp_path):
    bindir, log = fake_tools(tmp_path)
    r = run_line(service(QUIT, "ExecStart"), bindir)
    assert r.returncode == 0, r.stderr
    assert calls(log) == ["plymouth quit"]


def test_quit_drop_in_clears_stock_command():
    lines = [l for l in QUIT.read_text().splitlines() if l.startswith("ExecStart=")]
    assert lines[0] == "ExecStart="


def test_drop_in_parses():
    cp = configparser.ConfigParser(interpolation=None, strict=False)
    cp.read_string(QUIT.read_text())
    assert cp.sections() == ["Service"]


def test_script_hides_ring_on_handoff_and_quit():
    text = SCRIPT.read_text()
    assert 'str == "raye:handoff"' in text
    assert "Plymouth.SetQuitFunction(quit);" in text
    assert re.search(r"fun quit\(\) \{\s*hand_off\(\);", text)
    # the ring is drawn hidden once handed off, whatever the password field did
    assert re.search(r"on = global\.ring_on && !global\.handoff;", text)


@pytest.fixture
def tpm_initramfs(tmp_path):
    root = tmp_path / "root"
    theme = root / "usr/share/plymouth/themes/raye"
    theme.parent.mkdir(parents=True)
    theme.symlink_to(REPO / "plymouth" / "raye")
    (root / "etc").mkdir()
    (root / "etc/crypttab").write_text("root UUID=1111 none luks,tpm2-device=auto\n")
    dest = tmp_path / "initramfs"
    d = dest / "usr/share/plymouth/themes/raye"
    d.mkdir(parents=True)
    for f in (REPO / "plymouth" / "raye").iterdir():
        (d / f.name).write_bytes(f.read_bytes())
    b = tmp_path / "hookbin"
    b.mkdir()
    (b / "fc-cache").write_text("#!/bin/sh\nexit 0\n")
    (b / "fc-cache").chmod(0o755)
    env = {"PATH": f"{b}:/usr/bin:/bin", "DESTDIR": str(dest), "RAYE_PLYMOUTH_ROOT": str(root)}
    r = subprocess.run(["sh", str(HOOK)], env=env, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return dest


def test_tpm_watch_stops_with_the_initramfs(tpm_initramfs, tmp_path):
    premount = (tpm_initramfs / "scripts/init-premount/raye-plymouth-tpm").read_text()
    bottom = (tpm_initramfs / "scripts/init-bottom/raye-plymouth").read_text()
    run = tmp_path / "run"
    run.mkdir()
    # the watch, started the way init-premount starts it, with a long life
    env = dict(os.environ, RAYE_PROC=str(tmp_path / "noproc"), RAYE_TICKS="100000",
               RAYE_TICK="0.05")
    subprocess.run(["sh", "-c", premount.replace("/run/", f"{run}/")], env=env, check=True,
                   timeout=10)
    pid = int((run / "raye-plymouth-tpm.pid").read_text())
    os.kill(pid, 0)
    subprocess.run(["sh", "-c", bottom.replace("/run/", f"{run}/")], check=True, timeout=10)
    assert not (run / "raye-plymouth-tpm.pid").exists()
    for _ in range(50):
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            break
        subprocess.run(["sleep", "0.05"])
    else:
        os.kill(pid, 9)
        pytest.fail("the TPM watch still runs after init-bottom")
