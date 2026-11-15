# Tests of org.raye.shell, run by ctest (and so by the package build).
#
#   shellsettings   rayeshellrc: defaults, checks, writes, live changes
#   updatestate     Raye Update's state file and "Update and restart"
#   recentlyadded   which apps are new, and for how long
#   components      the QML pieces load cleanly; tokens, curves, helpers
#   appjumplist     a taskbar app's tasks and recent files
#   volumechime     when the volume sound plays: once, never a burst
#   launcherentry   the counts applications publish about themselves,
#                   and the badge a gone application must not leave behind
#
# gallery.qml is not a test: it shows every piece, to look at under Xvfb.

set(_raye_lib_test_env
    "QT_QPA_PLATFORM=offscreen"
    "QT_QUICK_BACKEND=software"
    "HOME=${CMAKE_CURRENT_BINARY_DIR}/home"
    "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config"
    "XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.local/share"
    "XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")

find_program(RAYE_DBUS_RUN_SESSION dbus-run-session)

foreach(_test shellsettings updatestate recentlyadded components appjumplist launcherentry volumechime)
    add_executable(tst_lib_${_test} tst_${_test}.cpp)
    target_link_libraries(tst_lib_${_test} PRIVATE rayeshell_logic Qt6::Test)
    target_compile_definitions(tst_lib_${_test} PRIVATE
        RAYE_SHELLRC="${CMAKE_CURRENT_SOURCE_DIR}/../rayeshellrc"
        RAYE_STATE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/state"
        RAYE_SHELL_QML_DIR="${CMAKE_BINARY_DIR}/qml"
        RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
    # the live change of rayeshellrc travels on D-Bus: give it a private bus
    if(RAYE_DBUS_RUN_SESSION)
        add_test(NAME lib-${_test} COMMAND ${RAYE_DBUS_RUN_SESSION} -- $<TARGET_FILE:tst_lib_${_test}>)
    else()
        add_test(NAME lib-${_test} COMMAND tst_lib_${_test})
    endif()
    set_tests_properties(lib-${_test} PROPERTIES ENVIRONMENT "${_raye_lib_test_env}" TIMEOUT 120)
endforeach()
# the QML tests need the module built first
add_dependencies(tst_lib_components rayeshellplugin)
