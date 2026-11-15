#!/bin/sh
# Runs the org.raye.signin component tests without a display:
#   signin/tests/run-tests.sh
# The module is taken from the repo (signin/), not from the system, so this
# needs no build.
# Exits 77 (skip) when qmltestrunner is absent.
set -eu
top=$(cd "$(dirname "$0")/../.." && pwd)
cd "$top"
. "$top/tests/find-qmltestrunner.sh"
raye_find_qmltestrunner
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    exec "$QMLTESTRUNNER" -import signin -import signin/tests/fakes \
    -input signin/tests
