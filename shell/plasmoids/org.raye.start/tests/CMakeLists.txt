# Tests of the Start applet: the decisions (startlogic.js), the row of
# taskbar buttons (StartRow.qml) and the popup frame (StartFrame.qml), with
# the Raye tokens and the org.raye.shell module of this build. harness.qml
# is not a test: it shows the row in every search style and the popup.

add_executable(tst_start tst_start.cpp)
target_link_libraries(tst_start PRIVATE Qt6::Test Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(tst_start PRIVATE
    RAYE_START_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    RAYE_START_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}"
    RAYE_SHELL_QML_DIR="${CMAKE_BINARY_DIR}/qml"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_dependencies(tst_start rayeshellplugin)
add_test(NAME start-applet COMMAND tst_start)
set_tests_properties(start-applet PROPERTIES TIMEOUT 120 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")

add_executable(start_harness harness.cpp)
target_link_libraries(start_harness PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(start_harness PRIVATE RAYE_START_TESTS="${CMAKE_CURRENT_SOURCE_DIR}")
