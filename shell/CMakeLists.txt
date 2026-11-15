# Raye shell: taskbar, Start, quick settings, clock and the rest of the
# desktop shell. This file finds every dependency once and picks up the
# parts by folder, so adding a part never means editing this file.
#
#   shell/<part>/CMakeLists.txt            built with add_subdirectory
#   shell/<group>/<part>/CMakeLists.txt    same, when <group> has no CMakeLists.txt
#   shell/plasmoids/<id>/                  installed as a Plasma applet package
#   shell/plasmoids/<id>/CMakeLists.txt    compiled applet, builds and installs itself
#   shell/plasmoids/<id>/tests/            built when testing, never installed
#   shell/po/<lang>/<domain>.po            compiled and installed to share/locale
#   shell/tests/                           load test of every applet (ctest)

cmake_minimum_required(VERSION 3.22)

project(raye-shell LANGUAGES CXX)

set(QT_MIN_VERSION "6.8.0")
set(KF6_MIN_VERSION "6.18.0")
set(PLASMA_MIN_VERSION "6.6.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQmlModule)
include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core Gui Qml Quick DBus Widgets Test)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    Config CoreAddons I18n GlobalAccel KIO Service WindowSystem Package)

find_package(Plasma ${PLASMA_MIN_VERSION} REQUIRED)
find_package(PlasmaQuick ${PLASMA_MIN_VERSION} REQUIRED)
find_package(PlasmaActivitiesStats ${PLASMA_MIN_VERSION} REQUIRED)
find_package(LibTaskManager ${PLASMA_MIN_VERSION} REQUIRED)
find_package(LibNotificationManager ${PLASMA_MIN_VERSION} REQUIRED)
find_package(LibKWorkspace ${PLASMA_MIN_VERSION} REQUIRED)

# Every QML module of the shell lands here in the build tree, so tests and
# applets find org.raye.shell before it is installed. Parts must not set
# their own OUTPUT_DIRECTORY.
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# The org.raye.ui primitives (theme/qml) when they are in the tree.
set(RAYE_THEME_QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../qml")

set(RAYE_SHELL_PLASMOIDS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/plasmoids")
set(RAYE_SHELL_PO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/po")

# Applet packages. Same destination and layout as plasma_install_package(),
# but a tests/ folder or a README next to the QML never ends up installed.
function(raye_shell_install_plasmoid dir id)
    install(DIRECTORY ${dir}/
        DESTINATION ${PLASMA_DATA_INSTALL_DIR}/plasmoids/${id}
        PATTERN *.qmlc EXCLUDE
        PATTERN CMakeLists.txt EXCLUDE
        PATTERN Messages.sh EXCLUDE
        PATTERN README.md EXCLUDE
        PATTERN dummydata EXCLUDE
        PATTERN tests EXCLUDE)
endfunction()

# Parts, by folder. Sorted so the build order never depends on the disk.
set(_raye_shell_skip plasmoids po tests)
file(GLOB _raye_shell_children LIST_DIRECTORIES true CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
list(SORT _raye_shell_children)
foreach(_child IN LISTS _raye_shell_children)
    if(NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${_child} OR _child IN_LIST _raye_shell_skip)
        continue()
    endif()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_child}/CMakeLists.txt)
        add_subdirectory(${_child})
        continue()
    endif()
    # A group folder (kwin/, tabbox/): each part inside builds itself.
    file(GLOB _raye_shell_parts LIST_DIRECTORIES true CONFIGURE_DEPENDS
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${_child}/*/CMakeLists.txt)
    list(SORT _raye_shell_parts)
    foreach(_part IN LISTS _raye_shell_parts)
        get_filename_component(_part_dir ${_part} DIRECTORY)
        add_subdirectory(${_part_dir})
    endforeach()
endforeach()

# Applets.
set(RAYE_SHELL_PLASMOID_IDS "")
file(GLOB _raye_shell_plasmoids LIST_DIRECTORIES true CONFIGURE_DEPENDS
    ${RAYE_SHELL_PLASMOIDS_DIR}/*)
list(SORT _raye_shell_plasmoids)
foreach(_dir IN LISTS _raye_shell_plasmoids)
    if(NOT IS_DIRECTORY ${_dir})
        continue()
    endif()
    get_filename_component(_id ${_dir} NAME)
    if(EXISTS ${_dir}/CMakeLists.txt)
        add_subdirectory(${_dir} plasmoids/${_id})
    elseif(EXISTS ${_dir}/metadata.json)
        raye_shell_install_plasmoid(${_dir} ${_id})
    else()
        message(STATUS "raye-shell: ${_id} has no metadata.json yet, not installed")
        continue()
    endif()
    list(APPEND RAYE_SHELL_PLASMOID_IDS ${_id})
    if(BUILD_TESTING AND EXISTS ${_dir}/tests/CMakeLists.txt)
        add_subdirectory(${_dir}/tests plasmoids/${_id}/tests)
    endif()
endforeach()

# Translations: shell/po/fr/raye_shell_<part>.po and so on.
if(EXISTS ${RAYE_SHELL_PO_DIR})
    ki18n_install(${RAYE_SHELL_PO_DIR})
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

message(STATUS "raye-shell applets: ${RAYE_SHELL_PLASMOID_IDS}")

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
