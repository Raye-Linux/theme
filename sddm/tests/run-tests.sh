#!/bin/sh
# Runs the sign-in screen tests without a display:
#   sddm/tests/run-tests.sh
# org.raye.signin is taken from the repo (signin/), not from the system.
# Exits 77 (skip) when qmltestrunner is absent.
set -eu
top=$(cd "$(dirname "$0")/../.." && pwd)
cd "$top"
. "$top/tests/find-qmltestrunner.sh"
raye_find_qmltestrunner
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    exec "$QMLTESTRUNNER" -import signin -input sddm/tests
