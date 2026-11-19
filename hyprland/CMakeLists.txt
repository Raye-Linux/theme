# Raye on Hyprland (raye-hyprland): the session, Raye's Hyprland settings
# and raye-hypr-shell, the host that puts the Raye taskbar, Start, quick
# settings and notifications on Hyprland. The views themselves come from
# raye-shell's applets at run time (see shell/src/views.h).

cmake_minimum_required(VERSION 3.22)

project(raye-hyprland LANGUAGES CXX)

set(QT_MIN_VERSION "6.8.0")
set(KF6_MIN_VERSION "6.18.0")
set(PLASMA_MIN_VERSION "6.6.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick DBus Network Test)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n KIO Service)
find_package(LibNotificationManager ${PLASMA_MIN_VERSION} REQUIRED)

set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/shell/src)

qt_add_executable(raye-hypr-shell shell/src/main.cpp)

# every QML file at the root of the module
set(_hypr_qml hyprlogic.js Shell.qml Panel.qml Wallpaper.qml Flyout.qml TaskController.qml
    StartPopup.qml QuickPopup.qml Toasts.qml)
set(_hypr_qml_files "")
foreach(_file IN LISTS _hypr_qml)
    set_source_files_properties(shell/qml/${_file} PROPERTIES QT_RESOURCE_ALIAS ${_file})
    list(APPEND _hypr_qml_files shell/qml/${_file})
endforeach()

qt_add_qml_module(raye-hypr-shell
    URI org.raye.hypr
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    DEPENDENCIES QtQuick
    SOURCES
        shell/src/hyprland.cpp shell/src/hyprland.h
        shell/src/appinfo.cpp shell/src/appinfo.h
        shell/src/views.cpp shell/src/views.h
        shell/src/shellbus.h
    QML_FILES ${_hypr_qml_files})

target_link_libraries(raye-hypr-shell PRIVATE
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Network
    KF6::I18n KF6::I18nQml KF6::KIOGui KF6::Service KF6::CoreAddons
    PW::LibNotificationManager)

install(TARGETS raye-hypr-shell ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES config/hyprland.conf config/hyprlock.conf config/hypridle.conf
        DESTINATION ${KDE_INSTALL_DATADIR}/raye/hyprland)
install(FILES session/raye-hyprland.desktop DESTINATION ${KDE_INSTALL_DATADIR}/wayland-sessions)
install(PROGRAMS session/raye-hyprland-session DESTINATION ${KDE_INSTALL_BINDIR})
install(PROGRAMS session/raye-hyprland-start DESTINATION libexec)
install(PROGRAMS tools/raye-hypr-screenshot DESTINATION ${KDE_INSTALL_BINDIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
