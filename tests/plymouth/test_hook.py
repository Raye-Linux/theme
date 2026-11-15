"""The Raye boot screen: the initramfs hook, the script template and its
words. The pictures and the script are drawn for real by run.sh (needs
plymouthd and X); these tests need only sh and awk."""

import os
import re
import stat
import subprocess
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[2]
THEME = REPO / "plymouth" / "raye"
HOOK = REPO / "initramfs" / "raye-plymouth"
THEME_PATH = "usr/share/plymouth/themes/raye"


def strings(lang: str) -> dict[str, str]:
    out = {}
    for line in (THEME / f"strings-{lang}").read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        out[key] = value
    return out


def template_keys() -> set[str]:
    return set(re.findall(r"@([a-z0-9_]+)@", (THEME / "raye.script.in").read_text()))


@pytest.fixture
def system(tmp_path: Path) -> Path:
    """A fake installed system: the theme where the package puts it,
    stock plymouth defaults, Inter, the keyboard list."""
    root = tmp_path / "root"
    (root / "usr/share/plymouth/themes").mkdir(parents=True)
    (root / THEME_PATH).symlink_to(THEME)
    (root / "usr/share/plymouth/plymouthd.defaults").write_text(
        "[Daemon]\nShowDelay=0\nDeviceTimeout=8\nTheme=spinner\n")
    inter = root / "usr/share/fonts/opentype/inter"
    inter.mkdir(parents=True)
    for f in ("Inter-Regular.otf", "Inter-SemiBold.otf", "Inter-Bold.otf"):
        (inter / f).write_text(f)
    xkb = root / "usr/share/X11/xkb/rules"
    xkb.mkdir(parents=True)
    (xkb / "evdev.lst").write_text(
        "! model\n  pc105           Generic 105-key PC\n\n"
        "! layout\n  us              English (US)\n  ca              French (Canada)\n"
        "  fr              French\n\n"
        "! variant\n  fr-legacy       ca: French (Canada, legacy)\n"
        "  oss             fr: French (alt.)\n")
    (root / "etc/default").mkdir(parents=True)
    return root


@pytest.fixture
def fakebin(tmp_path: Path) -> Path:
    """Stand-ins for the tools the hook calls; each logs its arguments."""
    b = tmp_path / "bin"
    b.mkdir()
    log = tmp_path / "calls"

    def tool(name: str, body: str) -> None:
        p = b / name
        p.write_text(f'#!/bin/sh\necho "{name} $*" >> "{log}"\n{body}\n')
        p.chmod(0o755)

    tool("fc-cache", "exit 0")
    tool("blkid", 'case "$2" in 1111*) echo /dev/fake1 ;; 2222*) echo /dev/fake2 ;; esac')
    tool("clevis", '[ "$4" = /dev/fake1 ] && echo "1: tpm2 \'{\\"pcr_ids\\":\\"7\\"}\'"; exit 0')
    tool("gettext", '[ "$LANGUAGE" = fr ] && [ "$4" = "French (Canada)" ] '
         '&& { echo "Français (Canada)"; exit 0; }; echo "$4"')
    return b


def run_hook(root: Path, destdir: Path, fakebin: Path, *args: str) -> subprocess.CompletedProcess:
    env = {
        "PATH": f"{fakebin}:/usr/bin:/bin",
        "DESTDIR": str(destdir),
        "RAYE_PLYMOUTH_ROOT": str(root),
    }
    return subprocess.run(["sh", str(HOOK), *args], env=env, capture_output=True,
                          text=True, check=False)


def initramfs(tmp_path: Path, theme: bool = True) -> Path:
    """DESTDIR as the plymouth hook leaves it: the default theme copied."""
    dest = tmp_path / "initramfs"
    d = dest / THEME_PATH
    if theme:
        d.mkdir(parents=True)
        for f in THEME.iterdir():
            (d / f.name).write_bytes(f.read_bytes())
    else:
        (dest / "usr/share/plymouth/themes/spinner").mkdir(parents=True)
    return dest


# ---- words ----

def test_both_languages_have_the_same_keys():
    assert strings("en").keys() == strings("fr").keys()


def test_every_template_key_has_words():
    keys = strings("en").keys()
    for key in template_keys():
        if key == "keyboard_line":
            assert "keyboard" in keys
        elif key.endswith(("_a", "_b")) and key not in keys:
            base = key[:-2]
            assert base in keys, key
            assert re.search(r"\{[a-z]+\}", strings("en")[base]), base
        else:
            assert key in keys, key


def test_french_spacing_uses_no_break_spaces():
    fr = strings("fr")
    for value in fr.values():
        assert not re.search(r" [?:!;%]", value), value
    assert " %" in fr["updates"]


@pytest.mark.parametrize("lang", ["en", "fr"])
def test_words_name_no_other_system(lang):
    words = " ".join(strings(lang).values()).lower()
    for word in ("ubuntu", "debian", "linux", "windows", "luks", "cryptsetup",
                 "plymouth", "tpm", "grub", "fsck", "initramfs"):
        assert word not in words, word


def test_template_names_no_other_system():
    text = (THEME / "raye.script.in").read_text().lower()
    for word in ("ubuntu", "debian"):
        assert word not in text


# ---- render ----

@pytest.mark.parametrize("lang", ["en", "fr"])
def test_render_fills_every_key(system, fakebin, tmp_path, lang):
    r = run_hook(system, tmp_path, fakebin, "render", lang, "French (Canada)")
    assert r.returncode == 0, r.stderr
    assert not re.search(r"@[a-z0-9_]+@", r.stdout)
    assert f'T_PASSWORD = "{strings(lang)["password_title"]}";' in r.stdout
    kb = strings(lang)["keyboard"].replace("{layout}", "French (Canada)")
    assert f'T_KEYBOARD = "{kb}";' in r.stdout


def test_render_splits_texts_around_numbers(system, fakebin, tmp_path):
    r = run_hook(system, tmp_path, fakebin, "render", "en", "")
    assert 'T_UPDATES_A = "Working on updates  ";' in r.stdout
    assert 'T_UPDATES_B = "%";' in r.stdout
    assert 'T_FSCK_A = "Scanning and repairing drive (";' in r.stdout
    assert 'T_FSCK_B = ")";' in r.stdout
    # no keyboard known: no keyboard line
    assert 'T_KEYBOARD = "";' in r.stdout


def test_render_escapes_what_would_end_the_string(system, fakebin, tmp_path):
    r = run_hook(system, tmp_path, fakebin, "render", "en", 'A "q" \\ b\x07c')
    assert r.returncode == 0
    assert 'T_KEYBOARD = "Keyboard: A \\"q\\" \\\\ bc";' in r.stdout


def test_render_unknown_language_falls_back_to_english(system, fakebin, tmp_path):
    r = run_hook(system, tmp_path, fakebin, "render", "../../etc/passwd", "")
    assert r.returncode == 0
    assert '"Enter the password for this disk"' in r.stdout


def test_render_fails_on_a_key_without_words(system, fakebin, tmp_path):
    fake = tmp_path / "theme"
    fake.mkdir()
    for f in ("strings-en", "strings-fr"):
        (fake / f).write_bytes((THEME / f).read_bytes())
    (fake / "raye.script.in").write_text('A = "@nothing_here@";\n')
    (system / THEME_PATH).unlink()
    (system / THEME_PATH).symlink_to(fake)
    r = run_hook(system, tmp_path, fakebin, "render", "en", "")
    assert r.returncode != 0
    assert "nothing_here" in r.stderr


def test_shipped_script_is_the_english_render(system, fakebin, tmp_path):
    """raye.script is the fallback on the installed system: it must be
    the template rendered in English, never edited by hand."""
    r = run_hook(system, tmp_path, fakebin, "render", "en", "")
    assert r.returncode == 0
    assert (THEME / "raye.script").read_text(encoding="utf-8") == r.stdout, \
        "run: initramfs/raye-plymouth render en > plymouth/raye/raye.script"


# ---- the hook in mkinitramfs ----

def test_prereqs():
    r = subprocess.run(["sh", str(HOOK), "prereqs"], capture_output=True, text=True, check=True)
    assert r.stdout.strip() == "plymouth"


def test_other_default_theme_is_left_alone(system, fakebin, tmp_path):
    dest = initramfs(tmp_path, theme=False)
    r = run_hook(system, dest, fakebin)
    assert r.returncode == 0
    assert not (dest / THEME_PATH).exists()
    assert not (dest / "scripts").exists()
    assert not (dest / "usr/share/fonts").exists()


def configure(system: Path, locale: str, layout: str = "ca", variant: str = "") -> None:
    (system / "etc/default/locale").write_text(f'LANG="{locale}"\n')
    (system / "etc/default/keyboard").write_text(
        f'XKBMODEL="pc105"\nXKBLAYOUT="{layout}"\nXKBVARIANT="{variant}"\n')


def test_french_system(system, fakebin, tmp_path):
    configure(system, "fr_CA.UTF-8")
    dest = initramfs(tmp_path)
    r = run_hook(system, dest, fakebin)
    assert r.returncode == 0, r.stderr
    script = (dest / THEME_PATH / "raye.script").read_text(encoding="utf-8")
    assert '"Entrez le mot de passe de ce disque"' in script
    assert '"Clavier : Français (Canada)"' in script
    # the gettext call got the system locale and the language
    calls = (tmp_path / "calls").read_text()
    assert "gettext -d xkeyboard-config -- French (Canada)" in calls
    # the template and its words stay out of the initramfs
    assert not (dest / THEME_PATH / "raye.script.in").exists()
    assert not list((dest / THEME_PATH).glob("strings-*"))


def test_english_system_with_variant(system, fakebin, tmp_path):
    configure(system, "en_US.UTF-8", "ca,us", "fr-legacy,")
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    script = (dest / THEME_PATH / "raye.script").read_text(encoding="utf-8")
    assert '"Keyboard: French (Canada, legacy)"' in script
    assert "gettext" not in (tmp_path / "calls").read_text()


@pytest.mark.parametrize("layout", ["fr;reboot", "$(id)", "ca us", "../x"])
def test_odd_layout_values_draw_no_keyboard_line(system, fakebin, tmp_path, layout):
    configure(system, "en_US.UTF-8", layout)
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    assert 'T_KEYBOARD = "";' in (dest / THEME_PATH / "raye.script").read_text()


def test_unknown_layout_draws_no_keyboard_line(system, fakebin, tmp_path):
    configure(system, "en_US.UTF-8", "zz")
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    assert 'T_KEYBOARD = "";' in (dest / THEME_PATH / "raye.script").read_text()


def test_no_locale_means_english(system, fakebin, tmp_path):
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    assert '"Enter the password for this disk"' in (dest / THEME_PATH / "raye.script").read_text()


def test_inter_and_grey_text(system, fakebin, tmp_path):
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    fonts = sorted(p.name for p in (dest / "usr/share/fonts/opentype/inter").iterdir())
    assert fonts == ["Inter-Regular.otf", "Inter-SemiBold.otf"]
    conf = (dest / "etc/fonts/conf.d/10-raye-plymouth.conf").read_text()
    assert "<const>none</const>" in conf
    assert f"fc-cache -s -y {dest}" in (tmp_path / "calls").read_text()


def test_tpm_from_crypttab_option(system, fakebin, tmp_path):
    (system / "etc/crypttab").write_text("root UUID=3333 none luks,tpm2-device=auto\n")
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    assert (dest / THEME_PATH / "tpm.png").exists()


def test_tpm_from_clevis(system, fakebin, tmp_path):
    (system / "etc/crypttab").write_text(
        "# root UUID=1111 none luks\nhome UUID=2222 none luks\nroot UUID=1111 none luks\n")
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    assert (dest / THEME_PATH / "tpm.png").exists()


def test_no_tpm(system, fakebin, tmp_path):
    (system / "etc/crypttab").write_text("home UUID=2222 none luks\n")
    dest = initramfs(tmp_path)
    (dest / THEME_PATH / "tpm.png").write_text("stale")
    assert run_hook(system, dest, fakebin).returncode == 0
    assert not (dest / THEME_PATH / "tpm.png").exists()


def test_failed_render_keeps_the_copied_script(system, fakebin, tmp_path):
    dest = initramfs(tmp_path)
    (dest / THEME_PATH / "raye.script").write_text("kept")
    fake = tmp_path / "theme"
    fake.mkdir()
    for f in THEME.iterdir():
        (fake / f.name).write_bytes(f.read_bytes())
    (fake / "raye.script.in").write_text('A = "@nothing_here@";\n')
    (system / THEME_PATH).unlink()
    (system / THEME_PATH).symlink_to(fake)
    assert run_hook(system, dest, fakebin).returncode == 0
    assert (dest / THEME_PATH / "raye.script").read_text() == "kept"
    assert not (dest / THEME_PATH / "raye.script.new").exists()


# ---- the copy to /run for the shutdown screen ----

def test_run_copy_is_prepared(system, fakebin, tmp_path):
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    d = dest / THEME_PATH
    run_theme = (d / "run.plymouth").read_text()
    assert "ScriptFile=/run/plymouth/themes/raye/raye.script" in run_theme
    assert "ImageDir=/run/plymouth/themes/raye" in run_theme
    assert "/usr/share" not in run_theme
    defaults = (d / "run.defaults").read_text()
    assert defaults.count("Theme=") == 1 and "Theme=raye" in defaults
    assert "DeviceTimeout=8" in defaults
    script = dest / "scripts/init-bottom/raye-plymouth"
    assert script.stat().st_mode & stat.S_IXUSR
    r = subprocess.run(["sh", str(script), "prereqs"], capture_output=True, text=True, check=True)
    assert r.stdout.strip() == ""


def test_boot_script_copies_the_theme_to_run(system, fakebin, tmp_path):
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    fake_run = tmp_path / "run"
    (fake_run / "plymouth").mkdir(parents=True)
    body = (dest / "scripts/init-bottom/raye-plymouth").read_text()
    body = body.replace("/usr/share/plymouth/themes/raye", str(dest / THEME_PATH))
    body = body.replace("/run/", f"{fake_run}/")
    r = subprocess.run(["sh", "-c", body], capture_output=True, text=True, check=False)
    assert r.returncode == 0, r.stderr
    copy = fake_run / "plymouth/themes/raye"
    assert "ScriptFile=/run/plymouth/themes/raye/raye.script" in (copy / "raye.plymouth").read_text()
    assert (copy / "raye.script").exists() and (copy / "eye.png").exists()
    assert not (copy / "run.defaults").exists() and not (copy / "run.plymouth").exists()
    assert "Theme=raye" in (fake_run / "plymouth/plymouthd.defaults").read_text()


@pytest.mark.parametrize("tpm", [True, False])
def test_boot_script_tells_the_screen_the_disk_is_open(system, fakebin, tmp_path, tpm):
    if tpm:
        (system / "etc/crypttab").write_text("root UUID=3333 none luks,tpm2-device=auto\n")
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    log = tmp_path / "ply"
    b = tmp_path / "plybin"
    b.mkdir()
    (b / "plymouth").write_text(f'#!/bin/sh\necho "$*" >> "{log}"\n')
    (b / "plymouth").chmod(0o755)
    fake_run = tmp_path / "run"
    (fake_run / "plymouth").mkdir(parents=True)
    body = (dest / "scripts/init-bottom/raye-plymouth").read_text()
    body = body.replace("/usr/share/plymouth/themes/raye", str(dest / THEME_PATH))
    body = body.replace("/run/", f"{fake_run}/")
    r = subprocess.run(["sh", "-c", body], env={"PATH": f"{b}:/usr/bin:/bin"},
                       capture_output=True, text=True, check=False)
    assert r.returncode == 0, r.stderr
    assert "display-message --text=raye:disk-open" in log.read_text().splitlines()


def test_boot_script_without_a_boot_screen(system, fakebin, tmp_path):
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    b = tmp_path / "plybin"
    b.mkdir()
    (b / "plymouth").write_text("#!/bin/sh\nexit 1\n")
    (b / "plymouth").chmod(0o755)
    fake_run = tmp_path / "run"
    (fake_run / "plymouth").mkdir(parents=True)
    body = (dest / "scripts/init-bottom/raye-plymouth").read_text()
    body = body.replace("/usr/share/plymouth/themes/raye", str(dest / THEME_PATH))
    body = body.replace("/run/", f"{fake_run}/")
    r = subprocess.run(["sh", "-c", body], env={"PATH": f"{b}:/usr/bin:/bin"},
                       capture_output=True, text=True, check=False)
    assert r.returncode == 0, r.stderr
    assert (fake_run / "plymouth/themes/raye/raye.script").exists()


# ---- the script and its pictures ----

def test_script_hooks_every_screen():
    s = (THEME / "raye.script.in").read_text()
    for fn in ("SetRefreshFunction", "SetDisplayPasswordFunction",
               "SetDisplayQuestionFunction", "SetDisplayNormalFunction",
               "SetDisplayMessageFunction", "SetHideMessageFunction",
               "SetSystemUpdateFunction", "SetUpdateStatusFunction"):
        assert f"Plymouth.{fn}(" in s, fn
    for key in ("raye:remove-usb", "raye:fsck:", "raye:finishing-updates"):
        assert f'"{key}"' in s, key
    for mode in ("shutdown", "reboot", "updates", "system-upgrade", "firmware-upgrade"):
        assert f'"{mode}"' in s, mode


def test_messages_are_quiet_except_raye_keys():
    s = (THEME / "raye.script.in").read_text()
    body = s[s.index("fun message(str)"):s.index("fun hide_message")]
    # every branch that draws is behind a raye: key, and nothing else
    # gets through
    assert "} else {" not in body
    assert 0 < body.count("set_status(") <= body.count('"raye:')
    assert "global.showing_message = 1" in body


def script_function(name: str) -> str:
    s = re.sub(r"#.*", "", (THEME / "raye.script.in").read_text())
    m = re.search(r"\nfun " + name + r"\([^)]*\) \{\n(.*?)\n\}\n", s, re.S)
    assert m, name
    return m.group(1)


def test_finishing_updates_gives_way_to_the_percentage():
    """raye-update says "Finishing updates" while dpkg ends what a power cut
    left half done, then apt counts. The first number has to show: a
    message used to hold the line until the end, so the percentage never
    came."""
    msg = script_function("message")
    branch = msg[msg.index('"raye:finishing-updates"'):]
    branch = branch[:branch.index("} else")]
    assert "global.finishing = 1" in branch
    update = script_function("system_update")
    m = re.search(r"if \(global\.finishing\) \{(.*?)\}", update, re.S)
    assert m, "system_update does not drop the finishing message"
    assert "global.showing_message = 0" in m.group(1)
    assert "global.finishing = 0" in m.group(1)
    assert update.index("if (global.finishing)") < update.index("if (!global.showing_message)")
    # any other message, or going back to the mode, forgets it
    assert msg.count("global.finishing = 0") >= 3
    assert "global.finishing = 0" in script_function("hide_message")


def test_firmware_screen_shows_no_frozen_number():
    """fwupd gives no progress while it writes, often for a minute: a
    number there sat at 90 % the whole time. The words and the ring say
    it is working."""
    for lang in ("en", "fr"):
        t = strings(lang)
        assert "{" not in t["firmware"], lang
        assert t["firmware_note"] and t["firmware_note"] != t["updates_note"], lang
    status = script_function("mode_status")
    line = next(li for li in status.splitlines() if '"firmware-upgrade"' in li)
    assert "firmware_line()" in line and "T_FIRMWARE_NOTE" in line and "+ n" not in line


def test_firmware_steps_are_counted_when_there_are_several():
    msg = script_function("message")
    assert '"raye:firmware-step:"' in msg
    branch = msg[msg.index('"raye:firmware-step:"'):]
    branch = branch[:branch.index("} else")]
    assert "T_FIRMWARE_OF" in s_script() and "global.fw_step" in branch
    # the mode forgets the step, so a later set starts without one
    m = msg[msg.index('"raye:mode:"'):]
    assert 'global.fw_step = ""' in m[:m.index("} else")]
    assert strings("en")["firmware_of"] == "of"
    assert strings("fr")["firmware_of"] == "sur"


def s_script() -> str:
    return (THEME / "raye.script.in").read_text()


def test_update_words_show_the_number_in_both_languages():
    assert strings("en")["updates"] == "Working on updates  {n}%"
    assert strings("fr")["updates"] == "Installation des mises à jour  {n}\u00a0%"


def test_pictures():
    from struct import unpack

    def size(name: str) -> tuple[int, int]:
        data = (THEME / name).read_bytes()
        assert data[:8] == b"\x89PNG\r\n\x1a\n", name
        return unpack(">II", data[16:24])

    assert size("field.png") == (360, 40)
    assert not (THEME / "dot.png").exists()
    w, h = size("eye.png")
    assert w == h >= 512
    for name in ("glow.png", "bullet.png", "background.png", "tpm-marker.png"):
        size(name)
    # words are drawn with Inter now, never pictures
    assert not list(THEME.glob("prompt-*.png"))
    assert not list(THEME.glob("hint-*.png"))


def test_eye_width_matches_the_picture():
    pytest.importorskip("PIL")
    import importlib.util
    spec = importlib.util.spec_from_file_location("plymouth_art", REPO / "art" / "plymouth.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    s = (THEME / "raye.script.in").read_text()
    value = float(re.search(r"EYE_WIDTH = ([0-9.]+);", s).group(1))
    assert abs(value - mod.EYE_WIDTH) < 0.0005


def test_theme_file_points_at_the_installed_theme():
    t = (THEME / "raye.plymouth").read_text()
    assert "ModuleName=script" in t
    assert f"ScriptFile=/{THEME_PATH}/raye.script" in t
    assert f"ImageDir=/{THEME_PATH}" in t


def test_hook_is_executable():
    assert os.access(HOOK, os.X_OK)


def test_no_variable_takes_the_name_of_a_function():
    """Functions live in the same global namespace as variables: a flag
    named like a function reads as the function (always true)."""
    s = (THEME / "raye.script.in").read_text()
    funs = set(re.findall(r"^fun (\w+)\(", s, re.M))
    globals_ = set(re.findall(r"global\.(\w+)\s*(?:=[^=]|\.|\[)", s))
    top = set(re.findall(r"^(\w+)\s*(?:=[^=]|\.\w+\s*=|\[)", s, re.M))
    assert not funs & globals_
    assert not funs & top


def test_layout_reads_every_screen():
    """Without a screen number, Window.GetX()/GetY() give the offset of
    the smallest screen on the plymouth in the image (not 0): a laptop
    with a bigger monitor got the eye at the bottom right. The layout
    reads the screens one by one and centres on the canvas."""
    s = re.sub(r"#.*", "", (THEME / "raye.script.in").read_text())
    assert "Window.GetX()" not in s and "Window.GetY()" not in s
    for call in ("Window.GetX(i)", "Window.GetY(i)", "Window.GetWidth(i)", "Window.GetHeight(i)"):
        assert call in s, call
    # every position hangs off the middle of the canvas, sized for the
    # smallest screen
    assert "global.cx = (left + right) / 2;" in s
    assert "global.cy = (top + bottom) / 2;" in s
    assert "y0" not in s


def test_eye_at_rest_has_no_scanlines():
    """The still eye is whole: no slices, no lines trailing to its right."""
    Image = pytest.importorskip("PIL.Image")
    img = Image.open(THEME / "eye.png").convert("RGBA")
    w, h = img.size
    a = img.getchannel("A")
    half = 0.4291 * w / 2
    # right of the almond: nothing at all
    right = a.crop((round(w / 2 + half + 4), 0, w, h))
    assert right.getextrema()[1] == 0
    # the almond is symmetric: the slices shifted bands sideways
    mirrored = a.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    ImageChops = pytest.importorskip("PIL.ImageChops")
    assert ImageChops.difference(a, mirrored).getextrema()[1] < 40


def test_check_block_finds_a_block_off_the_middle(tmp_path):
    """check_block.py, which run.sh uses on the screenshots, passes a
    block in the middle of both screens and fails one hanging low."""
    Image = pytest.importorskip("PIL.Image")
    ImageDraw = pytest.importorskip("PIL.ImageDraw")
    import importlib.util
    spec = importlib.util.spec_from_file_location("check_block", Path(__file__).with_name("check_block.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)

    def shot(dy: int) -> str:
        img = Image.new("RGB", (1440, 600), (14, 10, 23))
        d = ImageDraw.Draw(img)
        for x0, w, h in ((0, 800, 600), (800, 640, 480)):
            cx, cy = x0 + w / 2, h / 2 + dy
            # an eye, a ring and a line of words, 160 px from top to bottom
            d.ellipse((cx - 40, cy - 80, cx + 40, cy - 50), outline=(140, 60, 255), width=4)
            d.ellipse((cx - 18, cy - 10, cx + 18, cy + 26), outline=(90, 40, 160), width=2)
            d.rectangle((cx - 90, cy + 66, cx + 90, cy + 79), fill=(243, 234, 255))
        p = tmp_path / f"shot{dy}.png"
        img.save(p)
        return str(p)

    areas = [(0, 0, 800, 600), (800, 0, 640, 480)]
    assert mod.check(shot(0), areas) == []
    assert len(mod.check(shot(60), areas)) == 2


def ring_constants() -> dict:
    s = (THEME / "raye.script.in").read_text()
    sizes = [int(v) for _, v in sorted(re.findall(r"RING_SIZES\[(\d)\] = (\d+);", s))]
    return {
        "sizes": sizes,
        "frames": int(re.search(r"RING_FRAMES = (\d+);", s).group(1)),
        "cols": int(re.search(r"RING_COLS = (\d+);", s).group(1)),
        "count": int(re.search(r"for \(i = 1; i < (\d+); i\+\+\)", s[s.index("fun pick_ring"):]).group(1)),
    }


def art_module():
    pytest.importorskip("PIL")
    import importlib.util
    spec = importlib.util.spec_from_file_location("plymouth_art", REPO / "art" / "plymouth.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def test_ring_sheets_match_the_script():
    """The script cuts the frames out of each sheet on a grid it does
    not read from the picture: sizes, frame count and columns must agree
    with art/plymouth.py and with the pictures."""
    from struct import unpack
    c = ring_constants()
    assert c["sizes"] == sorted(c["sizes"]) and c["count"] == len(c["sizes"])
    rows = -(-c["frames"] // c["cols"])
    for n in c["sizes"]:
        data = (THEME / f"ring-{n}.png").read_bytes()
        w, h = unpack(">II", data[16:24])
        side = w // c["cols"]
        assert w == side * c["cols"] and h == side * rows, n
        # the frame is the ring plus room for its light
        assert n < side <= 2 * n, n
        data = (THEME / f"ring-{n}-track.png").read_bytes()
        assert unpack(">II", data[16:24]) == (side, side), n
    assert sorted(p.name for p in THEME.glob("ring-*.png")) == sorted(
        [f"ring-{n}.png" for n in c["sizes"]] + [f"ring-{n}-track.png" for n in c["sizes"]])

    mod = art_module()
    assert list(mod.RING_SIZES) == c["sizes"]
    assert mod.RING_FRAMES == c["frames"] and mod.RING_COLS == c["cols"]


def test_ring_loop_closes_and_never_stops():
    mod = art_module()
    n = mod.RING_FRAMES
    tail0, head0 = mod.ring_arc(0)
    tail_n, head_n = mod.ring_arc(n)
    # one loop later: the same arc, whole turns further
    assert (tail_n - tail0) % 360 == pytest.approx(0, abs=1e-6)
    assert (head_n - head0) % 360 == pytest.approx(0, abs=1e-6)
    prev = None
    for i in range(n + 1):
        tail, head = mod.ring_arc(i)
        assert mod.RING_SHORT - 1e-6 <= head - tail < 360
        if prev:
            # both ends always go forward, the head never too fast to follow
            assert tail >= prev[0] and head > prev[1]
            assert head - prev[1] < 30
        prev = (tail, head)


def test_ring_picks_the_closest_size():
    """pick_ring() in the script, redone here: closest by ratio."""
    c = ring_constants()

    def pick(want: float) -> int:
        return min(c["sizes"], key=lambda n: abs(n / want - 1))

    # 4.6 % of the short side of common screens
    assert pick(768 * 0.046) == 36
    assert pick(1080 * 0.046) == 48
    assert pick(1440 * 0.046) == 64
    assert pick(2160 * 0.046) == 96


def test_times_follow_the_refresh_rate():
    """The script asks for 60 refreshes a second and falls back to 50:
    no time may be a bare frame count."""
    s = (THEME / "raye.script.in").read_text()
    assert "Plymouth.SetRefreshRate(60);" in s
    assert "WAIT = 20 * FPS;" in s and "global.t + 4 * FPS" in s
    code = "\n".join(line.split("#")[0] for line in s.splitlines())
    for bare in ("13", "50)", "200;", "TURN"):
        assert bare not in code, bare


# ---- the TPM watcher: tells the screen when the chip said no ----

def tpm_watcher(system: Path, fakebin: Path, tmp_path: Path) -> Path:
    (system / "etc/crypttab").write_text("root UUID=3333 none luks,tpm2-device=auto\n")
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    return dest / "scripts/init-premount/raye-plymouth-tpm"


def test_no_tpm_no_watcher(system, fakebin, tmp_path):
    (system / "etc/crypttab").write_text("home UUID=2222 none luks\n")
    dest = initramfs(tmp_path)
    assert run_hook(system, dest, fakebin).returncode == 0
    assert not (dest / "scripts/init-premount").exists()


def test_watcher_is_a_boot_script(system, fakebin, tmp_path):
    script = tpm_watcher(system, fakebin, tmp_path)
    assert script.stat().st_mode & stat.S_IXUSR
    r = subprocess.run(["sh", str(script), "prereqs"], capture_output=True, text=True, check=True)
    assert r.stdout.strip() == ""


def run_watcher(script: Path, tmp_path: Path, steps: list) -> str:
    """Plays a boot: steps are (seconds, pid, process name or None to end
    it) on a fake /proc. Returns what was sent to plymouth."""
    import threading
    import time

    proc = tmp_path / "proc"
    proc.mkdir()
    log = tmp_path / "plymouth-calls"
    b = tmp_path / "wbin"
    b.mkdir()
    (b / "plymouth").write_text(f'#!/bin/sh\necho "$*" >> "{log}"\n')
    (b / "plymouth").chmod(0o755)

    def play():
        start = time.monotonic()
        for at, pid, cmd in steps:
            time.sleep(max(0.0, at - (time.monotonic() - start)))
            d = proc / str(pid)
            if cmd is None:
                (d / "comm").unlink()
                d.rmdir()
            else:
                d.mkdir()
                (d / "comm").write_text(cmd + "\n")

    player = threading.Thread(target=play)
    player.start()
    env = {"PATH": f"{b}:/usr/bin:/bin", "RAYE_PROC": str(proc), "RAYE_TICK": "0.02",
           "RAYE_PATIENCE": "10", "RAYE_TICKS": "150"}
    r = subprocess.run(["sh", str(script)], env=env, capture_output=True, text=True,
                       check=False, timeout=10)
    assert r.returncode == 0, r.stderr
    player.join()
    # the watch runs in the background: give it its ticks
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline and not log.exists():
        time.sleep(0.1)
    return log.read_text() if log.exists() else ""


ASK = "plymouth"
# the kernel cuts names at 15 characters
UNSEAL = "clevis-decrypt-"


def test_watcher_says_when_the_tpm_said_no(system, fakebin, tmp_path):
    script = tpm_watcher(system, fakebin, tmp_path)
    # clevis unseals, fails, and the password is still asked for
    calls = run_watcher(script, tmp_path, [(0.0, 300, "plymouthd"), (0.0, 301, "clevis"),
                                           (0.0, 400, ASK), (0.3, 410, UNSEAL), (0.8, 410, None)])
    assert calls.strip() == "display-message --text=raye:tpm-failed"


def test_watcher_is_quiet_when_the_tpm_opens_the_disk(system, fakebin, tmp_path):
    script = tpm_watcher(system, fakebin, tmp_path)
    # the unseal ends and the question goes with it: the disk opens
    calls = run_watcher(script, tmp_path, [(0.0, 300, "plymouthd"), (0.0, 301, "clevis"),
                                           (0.0, 400, ASK), (0.3, 410, UNSEAL), (1.5, 410, None),
                                           (1.55, 400, None)])
    assert calls == ""


def test_watcher_waits_for_a_slow_chip(system, fakebin, tmp_path):
    script = tpm_watcher(system, fakebin, tmp_path)
    # no unseal has ended yet: however long the question waits, say nothing
    calls = run_watcher(script, tmp_path, [(0.0, 300, "plymouthd"), (0.0, 301, "clevis"),
                                           (0.0, 400, ASK), (1.0, 410, UNSEAL)])
    assert calls == ""
