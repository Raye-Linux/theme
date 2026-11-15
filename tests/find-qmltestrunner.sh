# Sourced, not run. Sets QMLTESTRUNNER, or exits 77 (ctest's skip code,
# which tools/raye-tests honours as a named skip with the last line as the
# reason).
#
# Two traps, and PATH walks into both.
#
# 1. Debian puts the binary in /usr/lib/qt6/bin, which is NOT on PATH,
#    while Ubuntu -- the base Raye is built on -- has something called
#    qmltestrunner on PATH. A script that only asks `command -v` says
#    "not installed" on Debian about a binary that is installed. That is
#    how 340 QML tests sat unrun.
#
# 2. And the thing on Ubuntu's PATH is a **symlink to qtchooser**,
#    which defaults to Qt 5 and fails outright:
#      $ /usr/bin/qmltestrunner -help
#      qmltestrunner: could not exec '/usr/lib/qt5/bin/qmltestrunner'
#      exit 1
#    Measured 2026-11-06 in the resolute chroot the desktop is built in.
#    So trusting PATH does not merely find the wrong Qt: on the very
#    platform Raye ships on it finds a binary that cannot run at all,
#    and every suite would have failed with "could not exec" instead of
#    running or skipping.
#
# Hence: the Qt 6 directories FIRST, PATH last, and every candidate is
# PROVED to work before it is accepted -- `-help` exits 0 on the real
# qmltestrunner (both distributions, measured) and 1 on the qtchooser
# wrapper. Existing and being executable is not the same as working.
#
# Two overrides, both for a caller that already knows better than a search:
#   QMLTESTRUNNER      an executable to use (CMake's find_program)
#   RAYE_QT_BIN_DIRS   the directories to look in, space separated
# The second replaces the DIRECTORY list only: `qmake6` and PATH stay as
# last resorts, so pinning it cannot quietly disable them. It is also what
# makes the "found nothing" path testable -- point it at an empty
# directory, with nothing on PATH, and the skip must happen. A guard that
# cannot be made to fire has never been shown to work.
RAYE_QT_BIN_DIRS_DEFAULT="/usr/lib/qt6/bin /usr/lib/x86_64-linux-gnu/qt6/bin
/usr/lib/aarch64-linux-gnu/qt6/bin /usr/lib64/qt6/bin /usr/local/lib/qt6/bin"

raye_qmltestrunner_works() {
    [ -x "$1" ] || return 1
    QT_QPA_PLATFORM=offscreen "$1" -help >/dev/null 2>&1
}

raye_find_qmltestrunner() {
    if [ -n "${QMLTESTRUNNER:-}" ]; then
        raye_qmltestrunner_works "$QMLTESTRUNNER" && return 0
        QMLTESTRUNNER=
    fi
    for _d in ${RAYE_QT_BIN_DIRS-$RAYE_QT_BIN_DIRS_DEFAULT}; do
        if raye_qmltestrunner_works "$_d/qmltestrunner"; then
            QMLTESTRUNNER=$_d/qmltestrunner; return 0
        fi
    done
    # Ask Qt itself rather than guess a sixth directory.
    if command -v qmake6 >/dev/null 2>&1; then
        for _q in QT_INSTALL_LIBEXECS QT_INSTALL_BINS; do
            _d=$(qmake6 -query "$_q" 2>/dev/null) || _d=
            if [ -n "$_d" ] && raye_qmltestrunner_works "$_d/qmltestrunner"; then
                QMLTESTRUNNER=$_d/qmltestrunner; return 0
            fi
        done
    fi
    # PATH last, and only if it really runs: see trap 2 above.
    _d=$(command -v qmltestrunner 2>/dev/null) || _d=
    if [ -n "$_d" ] && raye_qmltestrunner_works "$_d"; then
        QMLTESTRUNNER=$_d; return 0
    fi
    echo "no working qmltestrunner (looked in the Qt 6 bin directories, at"
    echo "what qmake6 reports, and on PATH; a candidate must answer -help)"
    exit 77
}
