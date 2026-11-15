#!/usr/bin/env python3
#
# The two animated Raye wallpapers, drawn frame by frame by wallpapers.py
# and encoded once into wallpaper-animated/motion/:
#
#   python3 art/wallpapers_motion.py [strata|paper]
#
# 1920x1080, 24 frames a second, a loop (10 s, 20 s for the slower paper)
# whose last frame leads back into the first, VP9 in webm. Small on purpose: decoding a wallpaper
# should cost next to nothing, on the graphics card when it can.
# Needs ffmpeg with libvpx-vp9, Pillow and numpy.

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wallpapers as wp  # noqa: E402

OUT = os.path.join(wp.ROOT, "wallpaper-animated", "motion")
W, H, FPS = 1920, 1080, 24

# name: (seconds, draw)
LOOPS = {
    "strata": (10, lambda t: wp.strata(W, H, t, grain=0)),
    "paper": (20, lambda t: wp.paper(W, H, t, grain=0, ss=1)),
}


def encode(name, seconds, draw):
    os.makedirs(OUT, exist_ok=True)
    frames = FPS * seconds
    dest = os.path.join(OUT, "raye-%s.webm" % name)
    ff = subprocess.Popen(
        ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24",
         "-s", "%dx%d" % (W, H), "-r", str(FPS), "-i", "-",
         "-c:v", "libvpx-vp9", "-pix_fmt", "yuv420p", "-b:v", "0", "-crf", "36",
         "-row-mt", "1", "-deadline", "good", "-cpu-used", "2", "-g", str(frames),
         "-an", dest], stdin=subprocess.PIPE)
    for i in range(frames):
        img = draw(i / frames)
        if i == 0:
            # the still shown while the video opens
            img.save(os.path.join(OUT, "raye-%s.jpg" % name), quality=88)
        ff.stdin.write(img.tobytes())
    ff.stdin.close()
    if ff.wait() != 0:
        raise SystemExit("ffmpeg failed on " + name)
    print("wrote", dest, os.path.getsize(dest) // 1024, "KB")


def main(argv):
    for name, (seconds, draw) in LOOPS.items():
        if argv and name not in argv:
            continue
        encode(name, seconds, draw)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
