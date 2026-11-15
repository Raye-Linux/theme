#!/bin/sh
# Runs the polkit prompt's QML tests without a display:
#   session/polkit/tests/run-tests.sh
# -import signin is not optional: tst_prompt.qml imports org.raye.signin,
# and without it the whole file fails to compile with "module not installed"
# -- which reads like a missing system package and is really a missing flag.
# Exits 77 (skip) when qmltestrunner is absent.
set -eu
top=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$top"
. "$top/tests/find-qmltestrunner.sh"
raye_find_qmltestrunner
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    exec "$QMLTESTRUNNER" -import signin -input session/polkit/tests
