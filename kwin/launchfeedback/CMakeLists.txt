# The pointer feedback while an app starts: the busy ring of the Raye
# progress pointer, turning beside the pointer, in place of the app icon
# that KWin's own startup feedback draws there.
cmake_minimum_required(VERSION 3.22)
project(raye-launchfeedback LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

# KWin's headers are C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Quick Svg Test Widgets)
find_package(KF6 REQUIRED COMPONENTS Config)
find_package(KWin REQUIRED)

add_library(ringframes STATIC ringframes.cpp)
target_link_libraries(ringframes PUBLIC Qt6::Gui Qt6::Svg)
set_target_properties(ringframes PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(raye_launchfeedback MODULE main.cpp launchfeedback.cpp)
target_link_libraries(raye_launchfeedback PRIVATE ringframes KWin::kwin KF6::ConfigCore Qt6::DBus)
install(TARGETS raye_launchfeedback DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_ringframes tests/test_ringframes.cpp)
    target_link_libraries(test_ringframes ringframes Qt6::Test)
    add_test(NAME raye_launchfeedback_ringframes COMMAND test_ringframes)
    set_tests_properties(raye_launchfeedback_ringframes PROPERTIES
        ENVIRONMENT "RAYE_CURSORS=${CMAKE_CURRENT_SOURCE_DIR}/../../cursors;QT_QPA_PLATFORM=offscreen")
endif()
