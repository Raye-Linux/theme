# Tests of Raye Snap: snap.js, Layouts.qml, and main.qml run against
# stand-ins for KWin (mock/), with the real Plasma dialogs and the Raye
# tokens. harness.qml is not a test: it shows the layouts flyout and the
# snap bar over the wallpaper, to look at under Xvfb. run-in-kwin.sh runs
# the script in a nested KWin (by hand, not by ctest).

add_executable(tst_snap tst_snap.cpp)
target_link_libraries(tst_snap PRIVATE Qt6::Test Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(tst_snap PRIVATE
    RAYE_SNAP_PACKAGE="${CMAKE_CURRENT_SOURCE_DIR}/../package"
    RAYE_SNAP_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_test(NAME snap COMMAND tst_snap)
set_tests_properties(snap PROPERTIES TIMEOUT 120 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")
