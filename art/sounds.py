#!/usr/bin/env python3
#
# Composes the Raye sound theme from code and writes it to sounds/raye:
#
#   sounds/raye/index.theme
#   sounds/raye/stereo/<name>.oga      one file per sound
#   sounds/raye/stereo/<alias>.oga     links to the file a name shares
#
#   python3 art/sounds.py [--wav DIR]
#
# Needs numpy and oggenc (vorbis-tools). The output is committed, so the
# package build needs neither. Run it again only when a sound changes;
# art/test_sounds.py checks the committed files still match this code.
#
# Every sound is 48 kHz mono: a soft "glass" tone (sine with a short FM
# shimmer on the attack) over a small marimba body, with a little room
# around it. Raye's motif is the eye opening: E5, B5, G#5.

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import wave

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "sounds", "raye")
RATE = 48000

# the loudest sample of any sound, in dBFS; the spec says -6, the half
# decibel is room for what the Vorbis encoder adds to the peaks
PEAK_DBFS = -6.5
FADE_OUT = 0.03

E4, G4s, B4 = 329.63, 415.30, 493.88
C5s, E5, F5s, G5s, A5, B5 = 554.37, 659.26, 739.99, 830.61, 880.00, 987.77
C6s, E6 = 1108.73, 1318.51
MOTIF = (E5, B5, G5s)


# ---------------------------------------------------------------- voices

def _t(length: float) -> np.ndarray:
    return np.arange(int(round(length * RATE))) / RATE


def _attack(t: np.ndarray, attack: float) -> np.ndarray:
    a = np.clip(t / attack, 0.0, 1.0)
    return 0.5 - 0.5 * np.cos(np.pi * a)


def glass(freq: float, length: float, decay: float) -> np.ndarray:
    """Sine with a second and third partial, and an FM shimmer that dies
    within the first 60 ms: the "glass" part of the Raye timbre."""
    t = _t(length)
    index = 1.1 * np.exp(-t / 0.02)
    mod = index * np.sin(2 * np.pi * 3.5 * freq * t)
    tone = (np.sin(2 * np.pi * freq * t + mod)
            + 0.12 * np.sin(2 * np.pi * 2 * freq * t) * np.exp(-t / (decay * 0.6))
            + 0.04 * np.sin(2 * np.pi * 3 * freq * t) * np.exp(-t / (decay * 0.3)))
    return tone * _attack(t, 0.004) * np.exp(-t / decay)


def marimba(freq: float, length: float, decay: float) -> np.ndarray:
    """A bar's partials (1, 3.93, 9.2) with the high ones dying fast: the
    short wooden body under the glass."""
    t = _t(length)
    tone = (np.sin(2 * np.pi * freq * t) * np.exp(-t / decay)
            + 0.25 * np.sin(2 * np.pi * 3.93 * freq * t) * np.exp(-t / 0.04)
            + 0.08 * np.sin(2 * np.pi * 9.2 * freq * t) * np.exp(-t / 0.012))
    return tone * _attack(t, 0.002)


def note(freq: float, length: float, decay: float = 0.18,
         wood: float = 0.35) -> np.ndarray:
    return (1 - wood) * glass(freq, length, decay) + wood * marimba(freq, length, decay)


def pad(freqs: tuple[float, ...], length: float, attack: float,
        release: float) -> np.ndarray:
    """Slow warm chord: two slightly detuned sines per note."""
    t = _t(length)
    tone = np.zeros_like(t)
    for f in freqs:
        tone += np.sin(2 * np.pi * f * 0.998 * t) + np.sin(2 * np.pi * f * 1.002 * t)
    env = _attack(t, attack)
    tail = np.clip((length - t) / release, 0.0, 1.0)
    return tone / (2 * len(freqs)) * env * tail ** 2


def blip(f0: float, f1: float, length: float) -> np.ndarray:
    """A short sine that glides from f0 to f1."""
    t = _t(length)
    freq = f0 * (f1 / f0) ** (t / length)
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    return np.sin(phase) * _attack(t, 0.003) * np.exp(-t / (length * 0.45))


def _band_noise(length: float, low: float, high: float, seed: int) -> np.ndarray:
    n = int(round(length * RATE))
    rng = np.random.default_rng(seed)
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / RATE)
    shape = np.exp(-((np.log(np.maximum(f, 1.0) / np.sqrt(low * high))
                      / np.log(high / low)) ** 2) * 4)
    noise = np.fft.irfft(spec * shape, n)
    return noise / (np.max(np.abs(noise)) or 1.0)


def paper(length: float) -> np.ndarray:
    """Soft paper whoosh: band noise rising then falling, with a few
    crinkles on top."""
    t = _t(length)
    swell = np.sin(np.pi * np.clip(t / length, 0, 1)) ** 1.5
    whoosh = _band_noise(length, 900, 5000, seed=7) * swell
    rng = np.random.default_rng(11)
    grains = np.zeros_like(t)
    for start in sorted(rng.uniform(0.05, length * 0.8, 9)):
        g = _t(0.012)
        i = int(start * RATE)
        burst = rng.standard_normal(len(g)) * np.exp(-g / 0.003)
        end = min(len(grains), i + len(g))
        grains[i:end] += burst[:end - i] * 0.25
    crinkle = np.fft.irfft(np.fft.rfft(grains) * (np.fft.rfftfreq(len(t), 1 / RATE) > 2500),
                           len(t))
    return 0.8 * whoosh + crinkle


def shutter(length: float) -> np.ndarray:
    """Soft click, then a breath of air."""
    t = _t(length)
    click = (np.sin(2 * np.pi * 1900 * t) * np.exp(-t / 0.004)
             + 0.6 * np.sin(2 * np.pi * 1250 * t) * np.exp(-np.maximum(t - 0.07, 0) / 0.004)
             * (t >= 0.07))
    air = _band_noise(length, 2000, 9000, seed=3) * np.exp(-t / 0.06) * 0.35
    return click + air


# ---------------------------------------------------------------- mixing

def room(x: np.ndarray, wet: float = 0.14, tail: float = 0.22) -> np.ndarray:
    """A small soft room: convolution with a decaying, darkened noise."""
    if wet <= 0:
        return x
    n = int(tail * RATE)
    rng = np.random.default_rng(1)
    t = np.arange(n) / RATE
    ir = rng.standard_normal(n) * np.exp(-t / (tail / 5))
    ir = np.fft.irfft(np.fft.rfft(ir) * np.exp(-np.fft.rfftfreq(n, 1 / RATE) / 3500), n)
    ir /= np.sqrt(np.sum(ir ** 2))
    size = len(x) + n
    wet_sig = np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(ir, size), size)[:len(x)]
    return (1 - wet) * x + wet * wet_sig


def mix(length: float, parts: list[tuple[float, float, np.ndarray]],
        wet: float = 0.14) -> np.ndarray:
    """parts: (start in s, gain, signal). Cut to length, room, fade."""
    out = np.zeros(int(round(length * RATE)))
    for start, gain, sig in parts:
        i = int(round(start * RATE))
        end = min(len(out), i + len(sig))
        out[i:end] += gain * sig[:end - i]
    out = room(out, wet)
    fade = int(FADE_OUT * RATE)
    out[-fade:] *= np.linspace(1.0, 0.0, fade) ** 2
    # noise starts mid-wave: a 2 ms fade in keeps the first sample silent
    start = int(0.002 * RATE)
    out[:start] *= _attack(np.arange(start) / RATE, 0.002)
    return out


# ---------------------------------------------------------------- loudness

# ITU-R BS.1770 K-weighting at 48 kHz (pre-filter and RLB high-pass)
_K = (((1.53512485958697, -2.69169618940638, 1.19839281085285),
       (1.0, -1.69065929318241, 0.73248077421585)),
      ((1.0, -2.0, 1.0),
       (1.0, -1.99004745483398, 0.99007225036621)))


def _biquad(b: tuple, a: tuple, x: np.ndarray) -> np.ndarray:
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    b0, b1, b2 = b
    _, a1, a2 = a
    for i, v in enumerate(x.tolist()):
        out = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, v, y1, out
        y[i] = out
    return y


def k_weight(x: np.ndarray) -> np.ndarray:
    for b, a in _K:
        x = _biquad(b, a, x)
    return x


def loudness(x: np.ndarray) -> float:
    """Loudness in LUFS. BS.1770 gated integration for sounds of 400 ms
    and more; a short sound has no full block, so it is measured whole."""
    y = k_weight(np.asarray(x, dtype=float))
    block, hop = int(0.4 * RATE), int(0.1 * RATE)
    if len(y) < block:
        ms = np.mean(y ** 2)
        return -0.691 + 10 * np.log10(ms) if ms > 0 else -np.inf
    ms = np.array([np.mean(y[i:i + block] ** 2)
                   for i in range(0, len(y) - block + 1, hop)])
    lk = -0.691 + 10 * np.log10(np.maximum(ms, 1e-20))
    ms = ms[lk > -70]
    if not len(ms):
        return -np.inf
    rel = -0.691 + 10 * np.log10(np.mean(ms)) - 10
    ms = ms[-0.691 + 10 * np.log10(ms) > rel]
    return -0.691 + 10 * np.log10(np.mean(ms))


def peak_dbfs(x: np.ndarray) -> float:
    p = np.max(np.abs(x))
    return 20 * np.log10(p) if p > 0 else -np.inf


def level(x: np.ndarray, target_lufs: float) -> np.ndarray:
    """Scale to the target loudness, then down again if the peak would
    pass PEAK_DBFS: short sounds stay under the notification, never
    over it."""
    x = x * 10 ** ((target_lufs - loudness(x)) / 20)
    over = peak_dbfs(x) - PEAK_DBFS
    if over > 0:
        x = x * 10 ** (-over / 20)
    return x


# ---------------------------------------------------------------- sounds

# name: (length in s, target loudness in LUFS, builder). The notification
# is the reference at -23; nothing is louder, the volume tick is -30.
def _login():
    m = [(0.00, 1.0, note(E5, 1.0, 0.35)),
         (0.14, 0.9, note(B5, 1.0, 0.35)),
         (0.28, 0.9, note(G5s, 1.1, 0.45)),
         (0.25, 0.55, pad((E4, B4, G5s), 1.15, 0.25, 0.7))]
    return mix(1.4, m, wet=0.2)


def _logout():
    return mix(0.9, [(0.00, 1.0, note(B5, 0.6, 0.22)),
                     (0.13, 0.9, note(G5s, 0.6, 0.22)),
                     (0.26, 0.9, note(E5, 0.64, 0.28))], wet=0.18)


def _unlock():
    return mix(0.4, [(0.00, 0.9, note(E5, 0.4, 0.12)),
                     (0.09, 1.0, note(B5, 0.31, 0.12))])


def _message():
    return mix(0.35, [(0.00, 0.9, note(B5, 0.35, 0.09, wood=0.2)),
                      (0.08, 1.0, note(E6, 0.27, 0.1, wood=0.2))])


def _information():
    return mix(0.3, [(0.0, 1.0, note(G5s, 0.3, 0.11))])


def _warning():
    return mix(0.4, [(0.00, 1.0, note(B5, 0.4, 0.1, wood=0.45)),
                     (0.11, 1.0, note(G5s, 0.29, 0.12, wood=0.45))])


def _error():
    return mix(0.45, [(0.00, 1.0, note(B4, 0.45, 0.12, wood=0.55)),
                      (0.13, 1.0, note(E4, 0.32, 0.15, wood=0.55))])


def _bell():
    return mix(0.12, [(0.0, 1.0, note(E6, 0.12, 0.03, wood=0.6))], wet=0.08)


def _device_added():
    return mix(0.3, [(0.00, 0.9, note(E5, 0.3, 0.08)),
                     (0.08, 1.0, note(B5, 0.22, 0.09))])


def _device_removed():
    return mix(0.3, [(0.00, 1.0, note(B5, 0.3, 0.08)),
                     (0.08, 0.9, note(E5, 0.22, 0.09))])


def _battery(freq: float, gain: float, wood: float, gap: float, length: float):
    return mix(length, [(i * gap, gain, note(freq, gap + 0.15, 0.07, wood=wood))
                        for i in range(3)])


def _battery_low():
    return _battery(G5s, 0.8, 0.3, 0.2, 0.7)


def _battery_caution():
    return _battery(E5, 1.0, 0.55, 0.22, 0.8)


def _plug():
    return mix(0.25, [(0.0, 1.0, blip(E5, B5, 0.12)),
                      (0.06, 0.5, note(B5, 0.19, 0.06))], wet=0.1)


def _unplug():
    return mix(0.25, [(0.0, 1.0, blip(B5, E5, 0.12)),
                      (0.06, 0.5, note(E5, 0.19, 0.06))], wet=0.1)


def _volume():
    return mix(0.06, [(0.0, 1.0, note(E6, 0.06, 0.012, wood=0.3))], wet=0.0)


def _capture():
    return mix(0.25, [(0.0, 1.0, shutter(0.25))], wet=0.08)


def _trash():
    return mix(0.45, [(0.0, 1.0, paper(0.45))], wet=0.1)


def _success():
    return mix(0.4, [(0.00, 0.9, note(B5, 0.4, 0.1, wood=0.25)),
                     (0.10, 1.0, note(E6, 0.3, 0.12, wood=0.25))])


def _failure():
    return mix(0.4, [(0.00, 1.0, note(G4s, 0.4, 0.11, wood=0.5)),
                     (0.12, 1.0, note(E4, 0.28, 0.13, wood=0.5))])


def _reminder():
    parts = []
    for start in (0.0, 0.8):
        parts += [(start, 1.0, note(E5, 0.6, 0.18)),
                  (start + 0.13, 0.9, note(B5, 0.6, 0.18)),
                  (start + 0.26, 0.9, note(G5s, 0.54 if start else 0.6, 0.22))]
    return mix(1.6, parts, wet=0.18)


SOUNDS = {
    "desktop-login": (1.4, -23.0, _login),
    "desktop-logout": (0.9, -24.0, _logout),
    "desktop-screen-unlock": (0.4, -25.0, _unlock),
    "message-new-instant": (0.35, -23.0, _message),
    "dialog-information": (0.3, -24.0, _information),
    "dialog-warning": (0.4, -23.0, _warning),
    "dialog-error": (0.45, -23.0, _error),
    "bell": (0.12, -26.0, _bell),
    "device-added": (0.3, -24.0, _device_added),
    "device-removed": (0.3, -24.0, _device_removed),
    "battery-low": (0.7, -24.0, _battery_low),
    "battery-caution": (0.8, -23.0, _battery_caution),
    "power-plug": (0.25, -25.0, _plug),
    "power-unplug": (0.25, -25.0, _unplug),
    "audio-volume-change": (0.06, -30.0, _volume),
    "screen-capture": (0.25, -26.0, _capture),
    "trash-empty": (0.45, -25.0, _trash),
    "complete": (0.4, -24.0, _success),
    "outcome-failure": (0.4, -24.0, _failure),
    "alarm-clock-elapsed": (1.6, -23.0, _reminder),
}

# other names that play the same file; the second group are the names
# Plasma's own notifications ask for
ALIASES = {
    "message": "message-new-instant",
    "notification": "message-new-instant",
    "message-attention": "message-new-instant",
    "message-highlight": "message-new-instant",
    "service-login": "desktop-screen-unlock",
    "service-logout": "desktop-logout",
    "dialog-question": "bell",
    "camera-shutter": "screen-capture",
    "outcome-success": "complete",
    "reminder": "alarm-clock-elapsed",
    "theme-demo": "desktop-login",
    "bell-window-system": "bell",
    "dialog-error-serious": "dialog-error",
    "dialog-error-critical": "dialog-error",
    "dialog-warning-auth": "dialog-warning",
    "completion-success": "complete",
    "completion-fail": "outcome-failure",
    "battery-full": "complete",
}

INDEX = """\
[Sound Theme]
Name=Raye
Comment=The sounds of Raye Linux
Comment[fr]=Les sons de Raye Linux
Inherits=freedesktop
Directories=stereo
Example=theme-demo

[stereo]
OutputProfile=stereo
"""


def render(name: str) -> np.ndarray:
    _, target, build = SOUNDS[name]
    return level(build(), target)


def write_wav(path: str, x: np.ndarray) -> None:
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())


def encode(wav: str, oga: str) -> None:
    # a fixed stream serial keeps the output the same from run to run
    subprocess.run(["oggenc", "-Q", "-q", "5", "--serial", "1",
                    "-o", oga, wav], check=True)


def main() -> int:
    ap = argparse.ArgumentParser(description="Compose the Raye sound theme.")
    ap.add_argument("--wav", help="also keep the WAV files in this folder")
    args = ap.parse_args()
    if not shutil.which("oggenc"):
        print("oggenc not found (vorbis-tools)", file=sys.stderr)
        return 1
    stereo = os.path.join(OUT, "stereo")
    if os.path.isdir(stereo):
        shutil.rmtree(stereo)
    os.makedirs(stereo)
    with open(os.path.join(OUT, "index.theme"), "w") as f:
        f.write(INDEX)
    with tempfile.TemporaryDirectory() as tmp:
        for name in SOUNDS:
            wav = os.path.join(args.wav or tmp, name + ".wav")
            if args.wav:
                os.makedirs(args.wav, exist_ok=True)
            x = render(name)
            write_wav(wav, x)
            encode(wav, os.path.join(stereo, name + ".oga"))
            print(f"{name:24s} {len(x) / RATE:5.2f} s  {loudness(x):6.1f} LUFS  "
                  f"peak {peak_dbfs(x):5.1f} dBFS")
    for alias, target in sorted(ALIASES.items()):
        os.symlink(target + ".oga", os.path.join(stereo, alias + ".oga"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
