add_executable(tst_displayplan tst_displayplan.cpp)
target_link_libraries(tst_displayplan raye-project-core Qt6::Test)
add_test(NAME project-displayplan COMMAND tst_displayplan)

# a change the screens refuse leaves the panel knowing them as they are
add_executable(tst_displaycontroller tst_displaycontroller.cpp ../src/displaycontroller.cpp)
target_link_libraries(tst_displaycontroller raye-project-core Qt6::Test KF6::Screen)
add_test(NAME project-displaycontroller COMMAND tst_displaycontroller)

# Win+P is asked for by the panel's .desktop
add_test(NAME project-desktop-shortcut COMMAND grep -qx "X-KDE-Shortcuts=Display,Meta+P"
         ${CMAKE_CURRENT_BINARY_DIR}/../org.raye.project.desktop)

# plugging a screen in opens the panel too: the switcher's service runs it
add_test(NAME project-switcher-unit COMMAND grep -qx "ExecStart=/usr/bin/raye-project --display-switcher"
         ${CMAKE_CURRENT_SOURCE_DIR}/../data/raye-project-osd.conf)
add_test(NAME project-switcher-unit-resets COMMAND grep -qx "ExecStart="
         ${CMAKE_CURRENT_SOURCE_DIR}/../data/raye-project-osd.conf)

# the panel itself, on a stand-in for the screens
add_executable(tst_projectpanel tst_projectpanel.cpp)
target_link_libraries(tst_projectpanel PRIVATE Qt6::Test Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(tst_projectpanel PRIVATE
    RAYE_PROJECT_QML="${CMAKE_CURRENT_SOURCE_DIR}/../qml"
    RAYE_PROJECT_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}"
    RAYE_SHELL_QML_DIR="${CMAKE_BINARY_DIR}/qml"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_dependencies(tst_projectpanel rayeshellplugin)
add_test(NAME project-panel COMMAND tst_projectpanel)
set_tests_properties(project-panel PROPERTIES TIMEOUT 60 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")

add_executable(project_harness harness.cpp)
target_link_libraries(project_harness PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(project_harness PRIVATE RAYE_PROJECT_TESTS="${CMAKE_CURRENT_SOURCE_DIR}")
