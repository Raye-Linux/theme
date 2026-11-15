# Tests of the window switcher: the sizes (layout.js), and main.qml run
# against stand-ins for KWin's switcher and thumbnails (mock/), with the
# real Plasma dialog and the Raye tokens. harness.qml is not a test: it
# shows the switcher over the wallpaper, to look at under Xvfb.

add_executable(tst_tabbox tst_tabbox.cpp)
target_link_libraries(tst_tabbox PRIVATE Qt6::Test Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(tst_tabbox PRIVATE
    RAYE_TABBOX_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../package"
    RAYE_TABBOX_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_test(NAME tabbox COMMAND tst_tabbox)
set_tests_properties(tabbox PROPERTIES TIMEOUT 120 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")
