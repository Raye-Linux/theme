# SPDX-FileCopyrightText: 2026 techoraye
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)

add_executable(tst_rayeplugins tst_rayeplugins.cpp)
target_link_libraries(tst_rayeplugins Qt6::Test Qt6::Widgets KF6::CoreAddons)
# the test loads the plugins straight from the build tree
add_dependencies(tst_rayeplugins raye6 rayedecoration kcm_rayedecoration rayestyleconfig)
target_compile_definitions(tst_rayeplugins PRIVATE
    STYLE_PLUGIN="$<TARGET_FILE:raye6>"
    DECORATION_PLUGIN="$<TARGET_FILE:rayedecoration>"
    DECORATION_KCM="$<TARGET_FILE:kcm_rayedecoration>"
    STYLE_KCM="$<TARGET_FILE:rayestyleconfig>")
# Qt looks for widget styles in <plugin path>/styles/, so the test gets a
# plugin path of its own holding the style that was just built.
set(RAYE_TEST_PLUGIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/plugins)
add_custom_command(TARGET tst_rayeplugins POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RAYE_TEST_PLUGIN_PATH}/styles
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:raye6> ${RAYE_TEST_PLUGIN_PATH}/styles/)

add_test(NAME rayeplugins COMMAND tst_rayeplugins)
set_tests_properties(rayeplugins PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${RAYE_TEST_PLUGIN_PATH}")
