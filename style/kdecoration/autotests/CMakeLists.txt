# SPDX-FileCopyrightText: 2026 techoraye
# SPDX-License-Identifier: GPL-2.0-or-later

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)

add_executable(tst_rayedecorationmetrics tst_rayedecorationmetrics.cpp ../rayedecorationmetrics.cpp)
target_include_directories(tst_rayedecorationmetrics PRIVATE ..)
target_link_libraries(tst_rayedecorationmetrics Qt6::Test Qt6::Gui)
add_test(NAME rayedecorationmetrics COMMAND tst_rayedecorationmetrics)

# the built plugin, painted through a stand-in for KWin
add_executable(tst_rayedecorationrender tst_rayedecorationrender.cpp)
target_link_libraries(tst_rayedecorationrender Qt6::Test Qt6::Widgets KF6::CoreAddons KDecoration3::KDecoration KDecoration3::KDecoration3Private)
add_dependencies(tst_rayedecorationrender rayedecoration)
target_compile_definitions(tst_rayedecorationrender PRIVATE DECORATION_PLUGIN="$<TARGET_FILE:rayedecoration>")
add_test(NAME rayedecorationrender COMMAND tst_rayedecorationrender)
set_tests_properties(rayedecorationrender PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config;XDG_CONFIG_DIRS=${CMAKE_CURRENT_BINARY_DIR}/config")
