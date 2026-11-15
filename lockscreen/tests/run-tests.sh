#!/bin/sh
# Runs the lock screen tests without a display:
#   lockscreen/tests/run-tests.sh
# org.raye.signin is taken from the repo (signin/), not from the system,
# and Plasma's session module from the stand-in in tests/fakes/.
# Exits 77 (skip) when qmltestrunner is absent.
set -eu
top=$(cd "$(dirname "$0")/../.." && pwd)
cd "$top"
. "$top/tests/find-qmltestrunner.sh"
raye_find_qmltestrunner
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    exec "$QMLTESTRUNNER" -import signin -import lockscreen/tests/fakes \
    -input lockscreen/tests
