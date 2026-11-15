# Tests of quick settings: its decisions (quicklogic.js) and the panel
# (QuickPanel.qml) on a stand-in for the system services, with the Raye
# tokens and the org.raye.shell module of this build, and Backend.qml itself
# on stand-ins for those services (mock/). harness.qml is not a test: it
# shows the panel's states, to look at under Xvfb.

add_executable(tst_quicksettings tst_quicksettings.cpp)
target_link_libraries(tst_quicksettings PRIVATE Qt6::Test Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(tst_quicksettings PRIVATE
    RAYE_QS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    RAYE_QS_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}"
    RAYE_SHELL_QML_DIR="${CMAKE_BINARY_DIR}/qml"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_dependencies(tst_quicksettings rayeshellplugin)
add_test(NAME quicksettings COMMAND tst_quicksettings)
set_tests_properties(quicksettings PROPERTIES TIMEOUT 120 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")

# Backend.qml itself, on stand-ins for the system services (mock/)
add_executable(tst_quicksettings_backend tst_backend.cpp)
target_link_libraries(tst_quicksettings_backend PRIVATE Qt6::Test Qt6::Qml Qt6::Quick)
target_compile_definitions(tst_quicksettings_backend PRIVATE
    RAYE_QS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    RAYE_QS_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_QML_DIR="${CMAKE_BINARY_DIR}/qml"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_dependencies(tst_quicksettings_backend rayeshellplugin)
add_test(NAME quicksettings-backend COMMAND tst_quicksettings_backend)
set_tests_properties(quicksettings-backend PROPERTIES TIMEOUT 60 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")

add_executable(quicksettings_harness harness.cpp)
target_link_libraries(quicksettings_harness PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(quicksettings_harness PRIVATE RAYE_QS_TESTS="${CMAKE_CURRENT_SOURCE_DIR}")
