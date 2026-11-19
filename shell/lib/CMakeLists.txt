# org.raye.shell: what every part of the shell builds on. The options
# (ShellSettings over rayeshellrc), the shell's tokens and easing curves,
# global shortcuts, starting apps and Settings pages, Raye Update's state,
# "Recently added", the unread counts applications publish about
# themselves (com.canonical.Unity.LauncherEntry, the taskbar badges), and
# the QML pieces drawn the Raye way (acrylic surfaces, panel buttons,
# menus, tooltips, sliders, toggle tiles).
# Colours and type come from org.raye.ui (theme/qml, in raye-style).

find_package(PlasmaActivities ${PLASMA_MIN_VERSION} REQUIRED)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS JobWidgets)

# The C++ part. The plugin and the tests each build it: Qt cannot make
# a shared plugin out of a static library, and the tests need the classes
# without loading a plugin.
set(_raye_shell_lib_sources
    src/shellsettings.cpp
    src/updatestate.cpp
    src/recentlyadded.cpp
    src/launcher.cpp
    src/globalshortcut.cpp
    src/motion.cpp
    src/blurbehind.cpp
    src/appjumplist.cpp
    src/activeoutput.cpp
    src/launcherentry.cpp
    src/volumechime.cpp)
set(_raye_shell_lib_libs
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus
    KF6::ConfigCore KF6::GlobalAccel KF6::KIOGui KF6::JobWidgets KF6::Service KF6::WindowSystem
    Plasma::Activities Plasma::ActivitiesStats)

# Every file sits at the root of the module.
set(_raye_shell_lib_qml
    ShellTokens.qml
    AcrylicSurface.qml
    PanelButton.qml
    EdgeReach.qml
    RayeMenu.qml
    RayeMenuItem.qml
    RayeMenuSeparator.qml
    RayeSlider.qml
    RayeToggleTile.qml
    RayeTooltip.qml
    TooltipWarmth.qml
    PanelMenu.qml)
set(_raye_shell_lib_qml_files "")
foreach(_file IN LISTS _raye_shell_lib_qml)
    set_source_files_properties(qml/${_file} PROPERTIES QT_RESOURCE_ALIAS ${_file})
    list(APPEND _raye_shell_lib_qml_files qml/${_file})
endforeach()
set_source_files_properties(qml/ShellTokens.qml qml/TooltipWarmth.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(rayeshellplugin MODULE)
# QML looks for lib<name>.so; KDE's settings drop the prefix of modules
set_target_properties(rayeshellplugin PROPERTIES PREFIX "lib")
target_include_directories(rayeshellplugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rayeshellplugin PRIVATE ${_raye_shell_lib_libs})

qt_add_qml_module(rayeshellplugin
    URI org.raye.shell
    VERSION 1.0
    PLUGIN_TARGET rayeshellplugin
    CLASS_NAME RayeShellPlugin
    DEPENDENCIES QtQuick
    OPTIONAL_IMPORTS org.raye.ui org.kde.kirigami
    SOURCES ${_raye_shell_lib_sources}
    QML_FILES ${_raye_shell_lib_qml_files})

install(TARGETS rayeshellplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/raye/shell)
install(FILES
    ${CMAKE_BINARY_DIR}/qml/org/raye/shell/qmldir
    ${CMAKE_BINARY_DIR}/qml/org/raye/shell/rayeshellplugin.qmltypes
    DESTINATION ${KDE_INSTALL_QMLDIR}/org/raye/shell)

install(FILES rayeshellrc DESTINATION ${KDE_INSTALL_CONFDIR})

if(BUILD_TESTING)
    add_library(rayeshell_logic STATIC ${_raye_shell_lib_sources})
    set_target_properties(rayeshell_logic PROPERTIES AUTOMOC ON)
    target_include_directories(rayeshell_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(rayeshell_logic PUBLIC ${_raye_shell_lib_libs})
    add_subdirectory(tests)
endif()
