# Tests of the taskbar apps: the decisions (taskbarlogic.js) and the row of
# buttons (TaskBar.qml) on a stand-in for Plasma's tasks model, with the
# Raye tokens and the org.raye.shell module of this build. harness.qml is
# not a test: it shows the taskbar's states, to look at under Xvfb.

add_executable(tst_taskbar tst_taskbar.cpp)
target_link_libraries(tst_taskbar PRIVATE Qt6::Test Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(tst_taskbar PRIVATE
    RAYE_TASKBAR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    RAYE_TASKBAR_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}"
    RAYE_SHELL_QML_DIR="${CMAKE_BINARY_DIR}/qml"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_dependencies(tst_taskbar rayeshellplugin)
add_test(NAME taskbar COMMAND tst_taskbar)
set_tests_properties(taskbar PROPERTIES TIMEOUT 120 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")

add_executable(taskbar_harness harness.cpp)
target_link_libraries(taskbar_harness PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(taskbar_harness PRIVATE RAYE_TASKBAR_TESTS="${CMAKE_CURRENT_SOURCE_DIR}")
