#!/bin/sh
# The animated wallpaper with Plasma's modules replaced by the stand-ins
# of tests/fakes. Exits 77 (skip) when qmltestrunner is absent.
set -eu
top=$(cd "$(dirname "$0")/../.." && pwd)
cd "$top"
. "$top/tests/find-qmltestrunner.sh"
raye_find_qmltestrunner
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    exec "$QMLTESTRUNNER" -import wallpaper-animated/tests/fakes \
    -input wallpaper-animated/tests
