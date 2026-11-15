# org.raye.desktopmenu: the right-click menu of the desktop and the
# taskbar (a Plasma containment actions plugin), and Ctrl + wheel on the
# desktop to resize the icons.
#
# Nothing here says where it is bound. All of that belongs to the desktop's
# own shell package, in the Raye plasma-desktop (raye/desktop): the Ctrl +
# wheel binding is one of its defaults, the taskbar's menu is set by its
# default layout, and an account made before gets it from its updates
# script.

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS KIO)

set(_raye_desktopmenu_libs
    Plasma::Plasma KF6::ConfigCore KF6::ConfigGui KF6::CoreAddons KF6::I18n
    KF6::KIOCore KF6::KIOGui KF6::KIOFileWidgets KF6::Service Qt6::Widgets)

kcoreaddons_add_plugin(org.raye.desktopmenu
    SOURCES src/desktopmenu.cpp src/menumodel.cpp
    INSTALL_NAMESPACE "plasma/containmentactions")
target_link_libraries(org.raye.desktopmenu PRIVATE ${_raye_desktopmenu_libs})
target_compile_definitions(org.raye.desktopmenu PRIVATE TRANSLATION_DOMAIN="raye_shell_desktopmenu")


if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
