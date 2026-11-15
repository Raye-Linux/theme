# Tests of the on-screen display: its files against the installed Plasma
# (qmldir, properties, what plasmashell sets), then Osd.qml run the way
# plasmashell drives it, with a stand-in for the player list (mock/).
# harness.qml is not a test: it shows the display over the wallpaper, to
# look at under Xvfb.

add_executable(tst_osd tst_osd.cpp)
target_link_libraries(tst_osd PRIVATE Qt6::Test Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(tst_osd PRIVATE
    RAYE_OSD_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    RAYE_OSD_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_QML_DIR="${CMAKE_BINARY_DIR}/qml"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
# the display uses org.raye.shell
add_dependencies(tst_osd rayeshellplugin)
add_test(NAME osd COMMAND tst_osd)
set_tests_properties(osd PROPERTIES TIMEOUT 120 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")
add_test(NAME osd-fr COMMAND tst_osd french)
set_tests_properties(osd-fr PROPERTIES TIMEOUT 60 ENVIRONMENT
    "LANGUAGE=fr;LANG=fr_FR.UTF-8;LC_ALL=fr_FR.UTF-8;QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")
