# Tests of raye-hyprland: the decisions (hyprlogic.js), the link to
# Hyprland against a stand-in for its sockets, and the session (its
# settings parsed by the Hyprland of the build, every Windows key bound).

find_program(QMLTESTRUNNER NAMES qmltestrunner
    HINTS ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS} /usr/lib/qt6/bin REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME hyprlogic COMMAND ${QMLTESTRUNNER} -input ${CMAKE_CURRENT_SOURCE_DIR}/tst_hyprlogic.qml)
set_tests_properties(hyprlogic PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_hyprland tst_hyprland.cpp ../shell/src/hyprland.cpp)
set_target_properties(tst_hyprland PROPERTIES AUTOMOC ON)
target_include_directories(tst_hyprland PRIVATE ../shell/src)
target_link_libraries(tst_hyprland PRIVATE Qt6::Test Qt6::Network Qt6::Qml)
add_test(NAME hyprland-ipc COMMAND tst_hyprland)
set_tests_properties(hyprland-ipc PROPERTIES TIMEOUT 60)

add_test(NAME hyprland-session
    COMMAND ${Python3_EXECUTABLE} -m pytest -q -p no:cacheprovider ${CMAKE_CURRENT_SOURCE_DIR}/test_session.py)
set_tests_properties(hyprland-session PROPERTIES TIMEOUT 300 ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
