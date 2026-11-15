# Tests of the Raye sound theme: the sounds art/sounds.py composes, and
# the files committed in sounds/raye.
#
#   python3 -m pytest art/test_sounds.py
#
# The composition tests need numpy; the decoding tests also need oggdec
# (vorbis-tools). Each part is skipped when its tool is missing.

import configparser
import os
import shutil
import subprocess
import sys
import wave

import pytest

np = pytest.importorskip("numpy")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sounds  # noqa: E402

STEREO = os.path.join(sounds.OUT, "stereo")

# every name the spec (theming 3.9) and Plasma's notifications ask for
WANTED = {
    "desktop-login", "desktop-logout", "desktop-screen-unlock",
    "service-login", "message-new-instant", "message", "notification",
    "dialog-information", "dialog-warning", "dialog-error", "bell",
    "dialog-question", "device-added", "device-removed", "battery-low",
    "battery-caution", "power-plug", "power-unplug",
    "audio-volume-change", "screen-capture", "camera-shutter",
    "trash-empty", "complete", "outcome-success", "outcome-failure",
    "alarm-clock-elapsed", "reminder",
    "bell-window-system", "dialog-error-serious", "dialog-error-critical",
    "dialog-warning-auth", "completion-success", "completion-fail",
    "battery-full", "theme-demo",
}

LENGTHS = {
    "desktop-login": 1.4, "desktop-logout": 0.9,
    "desktop-screen-unlock": 0.4, "message-new-instant": 0.35,
    "dialog-information": 0.3, "dialog-warning": 0.4,
    "dialog-error": 0.45, "bell": 0.12, "device-added": 0.3,
    "device-removed": 0.3, "battery-low": 0.7, "battery-caution": 0.8,
    "power-plug": 0.25, "power-unplug": 0.25,
    "audio-volume-change": 0.06, "screen-capture": 0.25,
    "trash-empty": 0.45, "complete": 0.4, "outcome-failure": 0.4,
    "alarm-clock-elapsed": 1.6,
}

NOTIFICATION_LUFS = -23.0


@pytest.fixture(scope="module")
def rendered():
    return {name: sounds.render(name) for name in sounds.SOUNDS}


def test_every_name_has_a_sound():
    names = set(sounds.SOUNDS) | set(sounds.ALIASES)
    assert WANTED <= names
    assert not set(sounds.SOUNDS) & set(sounds.ALIASES)
    for alias, target in sounds.ALIASES.items():
        assert target in sounds.SOUNDS, alias


def test_lengths_are_the_spec_ones(rendered):
    assert set(LENGTHS) == set(sounds.SOUNDS)
    for name, x in rendered.items():
        assert len(x) == round(LENGTHS[name] * sounds.RATE), name


def test_peaks_stay_under_minus_six(rendered):
    for name, x in rendered.items():
        assert sounds.peak_dbfs(x) <= sounds.PEAK_DBFS + 0.01, name
        assert sounds.PEAK_DBFS <= -6.0


def test_no_clipping_and_no_dc(rendered):
    for name, x in rendered.items():
        assert np.all(np.isfinite(x)), name
        assert np.max(np.abs(x)) < 1.0, name
        assert abs(np.mean(x)) < 0.01, name


def test_loudness_band(rendered):
    for name, x in rendered.items():
        target = sounds.SOUNDS[name][1]
        lufs = sounds.loudness(x)
        limited = sounds.peak_dbfs(x) >= sounds.PEAK_DBFS - 0.05
        assert lufs <= target + 0.5, name
        # a sound held back by the peak limit is quieter than its target,
        # but never so quiet that nobody hears it
        assert lufs >= target - 1.0 or (limited and lufs >= -36), (name, lufs)


def test_nothing_louder_than_the_notification(rendered):
    for name, x in rendered.items():
        assert sounds.loudness(x) <= NOTIFICATION_LUFS + 0.5, name
    tick = sounds.loudness(rendered["audio-volume-change"])
    assert tick <= -29.5


def test_sounds_start_and_end_quietly(rendered):
    for name, x in rendered.items():
        fade = int(0.005 * sounds.RATE)
        assert np.max(np.abs(x[-fade:])) < 0.01, name
        assert abs(x[0]) < 0.01, name


def test_composition_is_deterministic():
    a = sounds.render("trash-empty")
    b = sounds.render("trash-empty")
    assert np.array_equal(a, b)


def test_loudness_meter_on_a_reference_tone():
    # a 997 Hz sine at -20 dBFS peak reads -23 LUFS within a tenth
    t = np.arange(sounds.RATE * 2) / sounds.RATE
    x = 10 ** (-20 / 20) * np.sin(2 * np.pi * 997 * t)
    assert abs(sounds.loudness(x) - (-23.0)) < 0.15


# ---------------------------------------------------------- committed files

def test_index_theme():
    cp = configparser.ConfigParser(interpolation=None)
    cp.optionxform = str
    cp.read(os.path.join(sounds.OUT, "index.theme"))
    st = cp["Sound Theme"]
    assert st["Name"] == "Raye"
    assert st["Inherits"] == "freedesktop"
    assert st["Directories"] == "stereo"
    assert cp["stereo"]["OutputProfile"] == "stereo"
    assert os.path.exists(os.path.join(STEREO, st["Example"] + ".oga"))


def test_every_name_is_a_committed_file():
    files = {f[:-4] for f in os.listdir(STEREO) if f.endswith(".oga")}
    assert files == set(sounds.SOUNDS) | set(sounds.ALIASES)
    for name in sounds.SOUNDS:
        p = os.path.join(STEREO, name + ".oga")
        assert os.path.isfile(p) and not os.path.islink(p), name
    for alias, target in sounds.ALIASES.items():
        p = os.path.join(STEREO, alias + ".oga")
        assert os.path.islink(p), alias
        # relative, inside the folder, so it works wherever it is installed
        assert os.readlink(p) == target + ".oga", alias
    assert not [f for f in os.listdir(STEREO) if not f.endswith(".oga")]


def _decode(path: str, tmp_path) -> tuple[int, int, "np.ndarray"]:
    wav = str(tmp_path / "out.wav")
    subprocess.run(["oggdec", "-Q", "-o", wav, path], check=True)
    with wave.open(wav) as w:
        rate, ch = w.getframerate(), w.getnchannels()
        pcm = np.frombuffer(w.readframes(w.getnframes()), "<i2")
    return rate, ch, pcm / 32768.0


@pytest.mark.skipif(not shutil.which("oggdec"), reason="needs oggdec")
def test_committed_files_match_the_code(rendered, tmp_path):
    for name, x in rendered.items():
        rate, ch, y = _decode(os.path.join(STEREO, name + ".oga"), tmp_path)
        assert (rate, ch) == (sounds.RATE, 1), name
        assert abs(len(y) - len(x)) <= 1, name
        assert sounds.peak_dbfs(y) <= -6.0, name
        # compare loudness envelopes in 10 ms steps: a lossy encoder keeps
        # the shape of a sound but not every sample of its noise
        n = min(len(x), len(y)) // 480 * 480
        ex = np.sqrt(np.mean(x[:n].reshape(-1, 480) ** 2, axis=1))
        ey = np.sqrt(np.mean(y[:n].reshape(-1, 480) ** 2, axis=1))
        corr = np.corrcoef(ex, ey)[0, 1]
        assert corr > 0.99, (name, "run art/sounds.py again", corr)
        assert abs(sounds.loudness(y) - sounds.loudness(x)) < 1.0, name
