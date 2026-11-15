# org.raye.ui: the Raye building blocks for QML (Tokens and the
# primitives), plus a small C++ plugin for what QML cannot do itself
# (blur behind a window). Installed with raye-style.
#
# The QML files are copied into build/qml/org/raye/ui and the plugin is
# built into build/qml/org/raye/ui/effects, so the tests (and anyone with
# QML_IMPORT_PATH=build/qml) use the module exactly as it is installed.
# The plugin is a module of its own (org.raye.ui.effects): the QML part of
# org.raye.ui also loads straight from the source tree, where no plugin
# is built (the shell's load tests do that); only MicaWindow needs it.

cmake_minimum_required(VERSION 3.22)
project(raye-ui LANGUAGES CXX)

set(QT_MIN_VERSION 6.6)
set(KF_MIN_VERSION 6.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config WindowSystem)

set(RAYE_UI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/org/raye/ui")
set(RAYE_UI_BUILD_DIR "${CMAKE_BINARY_DIR}/qml/org/raye/ui")
set(RAYE_UI_EFFECTS_BUILD_DIR "${RAYE_UI_BUILD_DIR}/effects")

# The module's files: qmldir and every QML file, nothing else (no C++
# sources, no tests). Sorted so the order never depends on the disk.
file(GLOB _raye_ui_qml CONFIGURE_DEPENDS LIST_DIRECTORIES false
    RELATIVE "${RAYE_UI_SOURCE_DIR}" "${RAYE_UI_SOURCE_DIR}/*.qml")
list(SORT _raye_ui_qml)
set(RAYE_UI_FILES qmldir ${_raye_ui_qml})
foreach(_f IN LISTS RAYE_UI_FILES)
    configure_file("${RAYE_UI_SOURCE_DIR}/${_f}" "${RAYE_UI_BUILD_DIR}/${_f}" COPYONLY)
endforeach()

add_library(rayeuiplugin MODULE
    org/raye/ui/plugin/rayeuiplugin.cpp
    org/raye/ui/plugin/windoweffects.cpp
    org/raye/ui/plugin/windoweffects.h)
set_target_properties(rayeuiplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${RAYE_UI_EFFECTS_BUILD_DIR}"
    PREFIX "lib")
file(WRITE "${RAYE_UI_EFFECTS_BUILD_DIR}/qmldir"
    "module org.raye.ui.effects\nplugin rayeuiplugin\n")
target_link_libraries(rayeuiplugin PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    KF6::ConfigCore KF6::WindowSystem)

set(RAYE_UI_INSTALL_DIR "${KDE_INSTALL_QMLDIR}/org/raye/ui")
install(TARGETS rayeuiplugin DESTINATION "${RAYE_UI_INSTALL_DIR}/effects")
install(FILES "${RAYE_UI_EFFECTS_BUILD_DIR}/qmldir" DESTINATION "${RAYE_UI_INSTALL_DIR}/effects")
foreach(_f IN LISTS RAYE_UI_FILES)
    install(FILES "${RAYE_UI_SOURCE_DIR}/${_f}" DESTINATION "${RAYE_UI_INSTALL_DIR}")
endforeach()

if(BUILD_TESTING)
    find_program(QMLTESTRUNNER qmltestrunner
        HINTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}" /usr/lib/qt6/bin REQUIRED)
    set(_raye_ui_test_env
        QT_QPA_PLATFORM=offscreen
        QT_QUICK_BACKEND=software
        # no Plasma colours: Tokens paints Raye Night, as on a login screen
        QT_QPA_PLATFORMTHEME=
        LANG=en_US.UTF-8
        XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-config)
    set(_raye_ui_tests "${CMAKE_CURRENT_SOURCE_DIR}/../tests/qml")

    # the module as installed, with its plugin
    add_test(NAME raye-ui
        COMMAND ${QMLTESTRUNNER} -input "${_raye_ui_tests}" -import "${CMAKE_BINARY_DIR}/qml")
    set_tests_properties(raye-ui PROPERTIES
        ENVIRONMENT "${_raye_ui_test_env}"
        # the plugin must load here: a skip for it is a failure
        FAIL_REGULAR_EXPRESSION "plugin not loaded")

    # straight from the source tree, without the plugin: what the shell's
    # load tests do. Everything but MicaWindow must still work.
    add_test(NAME raye-ui-source
        COMMAND ${QMLTESTRUNNER} -input "${_raye_ui_tests}" -import "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(raye-ui-source PROPERTIES ENVIRONMENT "${_raye_ui_test_env}")
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
