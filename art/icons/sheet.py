#!/usr/bin/env python3
#
# Contact sheet of the Raye icon themes, drawn by Qt's own SVG renderer
# (the one Plasma and Files use), at 16, 24, 32 and 48 px, on Raye Night
# and on Raye Day:
#
#   python3 art/icons/sheet.py out.png [--display :N] [--theme Raye-Dark]
#                              [--context places,devices]
#
# Needs Xvfb, qml6 and ImageMagick's import. Starts its own Xvfb on the
# given display (default :99) and stops it afterwards.

import argparse
import os
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SIZES = (16, 24, 32, 48)
NIGHT, DAY = "#141318", "#F5F4F7"
ROW = 60
LABEL = 230


def names(theme_dir: str, contexts: list[str]) -> list[tuple[str, str]]:
    """(context, name) of every drawn icon, links left out."""
    out = []
    for ctx in contexts:
        d = os.path.join(theme_dir, ctx, "48")
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if fn.endswith(".svg") and not os.path.islink(os.path.join(d, fn)):
                out.append((ctx, fn[:-4]))
    return out


def qml(theme_dir: str, icons: list[tuple[str, str]]) -> tuple[str, int, int]:
    rows = []
    for ctx, name in icons:
        cells = []
        for bg in (NIGHT, DAY):
            for n in SIZES:
                src = "file://" + os.path.join(theme_dir, ctx, str(n), name + ".svg")
                cells.append(
                    f'Rectangle {{ width: 64; height: {ROW}; color: "{bg}"; '
                    f'Image {{ x: {32 - n // 2}; y: {ROW // 2 - n // 2}; width: {n}; height: {n}; '
                    f'sourceSize.width: {n}; sourceSize.height: {n}; smooth: false; '
                    f'source: "{src}" }} }}')
        rows.append(
            f'Row {{ Rectangle {{ width: {LABEL}; height: {ROW}; color: "#FFFFFF"; '
            f'Text {{ x: 8; anchors.verticalCenter: parent.verticalCenter; '
            f'font.family: "Inter"; font.pixelSize: 13; text: "{ctx}/{name}" }} }} '
            + " ".join(cells) + " }")
    w = LABEL + 64 * 2 * len(SIZES)
    h = ROW * len(icons)
    return ("import QtQuick\n"
            f"Rectangle {{ width: {w}; height: {h}; color: \"#FFFFFF\"\n"
            "Column {\n" + "\n".join(rows) + "\n} }\n"), w, h


def main() -> int:
    ap = argparse.ArgumentParser(description="Contact sheet of the Raye icons.")
    ap.add_argument("out")
    ap.add_argument("--display", default=":99")
    ap.add_argument("--theme", default="Raye")
    ap.add_argument("--context", default="places,devices,mimetypes")
    args = ap.parse_args()
    theme_dir = os.path.join(ROOT, "icons", args.theme)
    icons = names(theme_dir, args.context.split(","))
    text, w, h = qml(theme_dir, icons)
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "sheet.qml")
        with open(path, "w") as fh:
            fh.write(text)
        xvfb = subprocess.Popen(["Xvfb", args.display, "-screen", "0", f"{w}x{h}x24"],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        env = dict(os.environ, DISPLAY=args.display, QT_QUICK_BACKEND="software")
        app = None
        try:
            time.sleep(1)
            app = subprocess.Popen(["qml6", path], env=env, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL)
            time.sleep(4)
            subprocess.run(["import", "-window", "root", args.out], env=env, check=True)
        finally:
            if app:
                app.terminate()
                app.wait(5)
            xvfb.terminate()
            xvfb.wait(5)
    print(args.out, len(icons), "icons")
    return 0


if __name__ == "__main__":
    sys.exit(main())
