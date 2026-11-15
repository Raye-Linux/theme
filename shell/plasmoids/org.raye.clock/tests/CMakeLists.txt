# Tests of the clock: the decisions (clocklogic.js), the clock button
# (ClockButton.qml) and the notification centre (Centre.qml) on a stand-in
# for Plasma's notification model, with the Raye tokens and the
# org.raye.shell module of this build. harness.qml is not a test: it shows
# the clock and the centre, to look at under Xvfb.

add_executable(tst_clock tst_clock.cpp)
target_link_libraries(tst_clock PRIVATE Qt6::Test Qt6::Qml Qt6::Quick KF6::ConfigCore KF6::I18n KF6::I18nQml)
target_compile_definitions(tst_clock PRIVATE
    RAYE_CLOCK_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    RAYE_CLOCK_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}"
    RAYE_SHELL_QML_DIR="${CMAKE_BINARY_DIR}/qml"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_dependencies(tst_clock rayeshellplugin)
add_test(NAME clock-applet COMMAND tst_clock)
set_tests_properties(clock-applet PROPERTIES TIMEOUT 120 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")

# the centre's window opened from the clock of one screen, then another
add_executable(tst_centrewindow tst_centrewindow.cpp)
target_link_libraries(tst_centrewindow PRIVATE Qt6::Test Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(tst_centrewindow PRIVATE
    RAYE_CLOCK_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    RAYE_CLOCK_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_QML_DIR="${CMAKE_BINARY_DIR}/qml"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_dependencies(tst_centrewindow rayeshellplugin)
add_test(NAME clock-centre-window COMMAND tst_centrewindow)
set_tests_properties(clock-centre-window PROPERTIES TIMEOUT 120 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen:configfile=${CMAKE_CURRENT_SOURCE_DIR}/two-screens.json;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")

# the toast windows stacking above the taskbar as they come and go
add_executable(tst_toaststack tst_toaststack.cpp)
target_link_libraries(tst_toaststack PRIVATE Qt6::Test Qt6::Qml Qt6::Gui)
target_compile_definitions(tst_toaststack PRIVATE
    RAYE_CLOCK_TESTS="${CMAKE_CURRENT_SOURCE_DIR}"
    RAYE_SHELL_QML_DIR="${CMAKE_BINARY_DIR}/qml"
    RAYE_THEME_QML_DIR="${RAYE_THEME_QML_DIR}")
add_dependencies(tst_toaststack rayeshellplugin)
add_test(NAME clock-toast-stack COMMAND tst_toaststack)
set_tests_properties(clock-toast-stack PROPERTIES TIMEOUT 60 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.cache")

# notification sounds under Do not disturb, on a stand-in for the sound
# server's saved streams (mock/)
add_executable(tst_toastsounds tst_toastsounds.cpp)
target_link_libraries(tst_toastsounds PRIVATE Qt6::Test Qt6::Qml)
target_compile_definitions(tst_toastsounds PRIVATE
    RAYE_CLOCK_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    RAYE_CLOCK_TESTS="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME clock-toast-sounds COMMAND tst_toastsounds)

add_executable(clock_harness harness.cpp)
target_link_libraries(clock_harness PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick KF6::I18n KF6::I18nQml)
target_compile_definitions(clock_harness PRIVATE RAYE_CLOCK_TESTS="${CMAKE_CURRENT_SOURCE_DIR}")

# the agenda the applet gets back from Raye Calendar's tool, on a fake tool
add_executable(tst_agenda tst_agenda.cpp ../clockapplet.cpp)
target_include_directories(tst_agenda PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(tst_agenda PRIVATE Qt6::Test Qt6::Gui Qt6::Qml Plasma::Plasma KF6::ConfigCore KF6::ConfigGui
    KF6::CoreAddons PW::LibNotificationManager)
target_compile_definitions(tst_agenda PRIVATE
    RAYE_CALENDAR_TOOL="${CMAKE_CURRENT_SOURCE_DIR}/fake-calendar-tool")
add_test(NAME clock-agenda COMMAND tst_agenda)
set_tests_properties(clock-agenda PROPERTIES TIMEOUT 60 ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen:configfile=${CMAKE_CURRENT_SOURCE_DIR}/two-screens.json;HOME=${CMAKE_CURRENT_BINARY_DIR}/home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/home/.config")

# the stock notifications item the clock takes out of the trays
add_executable(tst_traybell tst_traybell.cpp)
target_link_libraries(tst_traybell PRIVATE Qt6::Test)
add_test(NAME clock-traybell COMMAND tst_traybell)
