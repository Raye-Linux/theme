# Tests of the desktop and taskbar menu: the menu model, then the plugin on
# real Plasma containments. French runs as its own test (LANGUAGE=fr).

add_executable(tst_desktopmenu tst_desktopmenu.cpp ../src/menumodel.cpp)
target_include_directories(tst_desktopmenu PRIVATE ../src)
target_link_libraries(tst_desktopmenu PRIVATE Qt6::Test Qt6::Widgets Plasma::Plasma
    KF6::ConfigCore KF6::ConfigGui KF6::CoreAddons KF6::I18n)
target_compile_definitions(tst_desktopmenu PRIVATE
    TRANSLATION_DOMAIN="raye_shell_desktopmenu"
    RAYE_DESKTOPMENU_SRC="${CMAKE_CURRENT_SOURCE_DIR}/../src"
    RAYE_DESKTOPMENU_PLUGIN="$<TARGET_FILE:org.raye.desktopmenu>"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}")
add_dependencies(tst_desktopmenu org.raye.desktopmenu)

# French comes from the po file, compiled here
set(_raye_desktopmenu_mo "${CMAKE_CURRENT_BINARY_DIR}/locale/fr/LC_MESSAGES/raye_shell_desktopmenu.mo")
find_program(RAYE_MSGFMT msgfmt REQUIRED)
add_custom_command(OUTPUT ${_raye_desktopmenu_mo}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/locale/fr/LC_MESSAGES"
    COMMAND ${RAYE_MSGFMT} -o ${_raye_desktopmenu_mo} "${RAYE_SHELL_PO_DIR}/fr/raye_shell_desktopmenu.po"
    DEPENDS "${RAYE_SHELL_PO_DIR}/fr/raye_shell_desktopmenu.po")
add_custom_target(tst_desktopmenu_mo DEPENDS ${_raye_desktopmenu_mo})
add_dependencies(tst_desktopmenu tst_desktopmenu_mo)

set(_raye_desktopmenu_env
    "QT_QPA_PLATFORM=offscreen"
    "HOME=${CMAKE_CURRENT_BINARY_DIR}/home"
    "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config"
    "XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.local/share"
    "XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")
add_test(NAME desktopmenu COMMAND tst_desktopmenu)
set_tests_properties(desktopmenu PROPERTIES TIMEOUT 120 ENVIRONMENT "${_raye_desktopmenu_env}")
add_test(NAME desktopmenu-fr COMMAND tst_desktopmenu)
set_tests_properties(desktopmenu-fr PROPERTIES TIMEOUT 60 ENVIRONMENT
    "${_raye_desktopmenu_env};LANGUAGE=fr;LANG=fr_FR.UTF-8;LC_ALL=fr_FR.UTF-8;XDG_DATA_DIRS=${CMAKE_CURRENT_BINARY_DIR}:/usr/local/share:/usr/share")


# not a test: shows the menus, to look at under Xvfb
add_executable(desktopmenu_preview preview.cpp)
target_link_libraries(desktopmenu_preview PRIVATE Qt6::Widgets Plasma::Plasma KF6::CoreAddons)
target_compile_definitions(desktopmenu_preview PRIVATE RAYE_DESKTOPMENU_PLUGIN="$<TARGET_FILE:org.raye.desktopmenu>")
add_dependencies(desktopmenu_preview org.raye.desktopmenu)
