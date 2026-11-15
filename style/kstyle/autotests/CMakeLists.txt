# SPDX-FileCopyrightText: 2026 techoraye
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)

# the colour roles are plain functions of a palette: built straight from
# the source, no plugin needed
add_executable(tst_rayetokens tst_rayetokens.cpp ../rayetokens.cpp)
target_include_directories(tst_rayetokens PRIVATE ..)
target_link_libraries(tst_rayetokens Qt6::Test Qt6::Gui)
add_test(NAME rayetokens COMMAND tst_rayetokens)

# sizes and pictures of the built style
add_executable(tst_rayestyle tst_rayestyle.cpp)
target_link_libraries(tst_rayestyle Qt6::Test Qt6::Widgets)
add_dependencies(tst_rayestyle raye6)
set(RAYE_STYLE_TEST_PLUGIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/plugins)
add_custom_command(TARGET tst_rayestyle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RAYE_STYLE_TEST_PLUGIN_PATH}/styles
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:raye6> ${RAYE_STYLE_TEST_PLUGIN_PATH}/styles/)
add_test(NAME rayestyle COMMAND tst_rayestyle)
# an empty configuration folder: the user's rayerc never changes the result
set_tests_properties(rayestyle PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${RAYE_STYLE_TEST_PLUGIN_PATH};XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config;XDG_CONFIG_DIRS=${CMAKE_CURRENT_BINARY_DIR}/config")
