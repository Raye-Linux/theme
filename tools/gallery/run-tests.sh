#!/bin/sh
# Run the gallery's own ctest tests against a build tree you already made.
#
#   tools/gallery/run-tests.sh            run them
#   tools/gallery/run-tests.sh --have     exit 0 when a usable tree exists
#
# Build the tree first (the gallery is a developer tool and is in no
# package, so nothing builds it for you):
#
#   cmake -S tools/gallery -B ~/.cache/raye-gallery-build
#   cmake --build ~/.cache/raye-gallery-build -j
#
# Why this exists. tools/gallery registers FOUR ctest tests -- options,
# scheme, pages (23 QtTest cases between them) and shoot, which captures
# every page under Xvfb -- and until 2026-11-06 nothing ran any of them.
# Not iso/tools/raye-tests, which never builds; and not the package build,
# because debian/rules builds only style, qml, shell and session/polkit:
# the gallery is not in RAYE_TREES and is part of no package. Twenty-four
# tests with nowhere to run. Measured the day this was written: all four
# pass, shoot included.
#
# The build tree is OUTSIDE the repository, and that is deliberate: a
# file written into /srv from the build VM is born root:root while
# the services that read /srv run as uid 1000.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
cache=${XDG_CACHE_HOME:-${HOME:+$HOME/.cache}}
BUILD=${RAYE_GALLERY_BUILD:-${cache:-/var/tmp/raye-src}/raye-gallery-build}

usable() { [ -f "$BUILD/CTestTestfile.cmake" ]; }

if [ "${1:-}" = "--have" ]; then
    usable && exit 0
    echo "the gallery's four ctest tests (23 QtTest cases and the Xvfb" \
         "screenshot) are built by nothing: cmake -S tools/gallery -B" \
         "$BUILD && cmake --build $BUILD -j" >&2
    exit 1
fi

usable || { echo "no build tree at $BUILD; see $0 --help or --have" >&2; exit 1; }
# --no-tests=error: ctest exits 0 when it finds nothing, so a tree that
# configured without BUILD_TESTING would otherwise pass in silence.
exec ctest --test-dir "$BUILD" --output-on-failure --no-tests=error
