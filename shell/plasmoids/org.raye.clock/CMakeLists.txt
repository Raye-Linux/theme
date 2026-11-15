# org.raye.clock is a compiled applet: only C++ can start the notification
# server (NotificationManager::Server::init() is not open to QML), as in
# Plasma's own notifications applet. Its QML is built into the plugin, so
# every .qml and .js under contents/ui (toasts/ included) is picked up here.

find_package(LibNotificationManager ${PLASMA_MIN_VERSION} REQUIRED)

# a folder of QML files (toasts/) gets its own qmldir, so its files see each other
qt_policy(SET QTP0004 NEW)

file(GLOB_RECURSE _raye_clock_qml CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/contents/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/*.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/contents/ui/*.js)
list(SORT _raye_clock_qml)
# Plasma looks for main.qml at the root of the module; files in a folder
# (toasts/) keep their folder, which ECM only does through PATH
set(_raye_clock_root_files "")
set(_raye_clock_dirs "")
foreach(_file IN LISTS _raye_clock_qml)
    get_filename_component(_dir ${_file} DIRECTORY)
    if(_dir STREQUAL "")
        list(APPEND _raye_clock_root_files contents/ui/${_file})
    else()
        list(APPEND _raye_clock_dirs ${_dir})
        list(APPEND _raye_clock_files_${_dir} contents/ui/${_file})
    endif()
endforeach()
list(REMOVE_DUPLICATES _raye_clock_dirs)

# the options of the applet, where Plasma looks in a built-in package
set_source_files_properties(contents/config/main.xml PROPERTIES QT_RESOURCE_ALIAS config/main.xml)

plasma_add_applet(org.raye.clock
    QML_SOURCES ${_raye_clock_root_files}
    RESOURCES contents/config/main.xml
    CPP_SOURCES clockapplet.cpp clockapplet.h toastsettings.h)

foreach(_dir IN LISTS _raye_clock_dirs)
    ecm_target_qml_sources(org.raye.clock SOURCES ${_raye_clock_files_${_dir}} PATH ${_dir})
endforeach()

# Raye Calendar's tool, in the same place its package puts it
target_compile_definitions(org.raye.clock PRIVATE
    RAYE_CALENDAR_TOOL="${KDE_INSTALL_FULL_LIBEXECDIR}/raye-calendar-tool")

target_link_libraries(org.raye.clock PRIVATE
    Qt6::Core Qt6::Qml Plasma::Plasma KF6::ConfigCore KF6::ConfigGui KF6::CoreAddons PW::LibNotificationManager)
