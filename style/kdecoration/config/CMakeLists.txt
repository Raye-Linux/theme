set(rayedecoration_config_SRCS
    ../rayeexceptionlist.cpp
    rayeconfigwidget.cpp
    rayedetectwidget.cpp
    rayeexceptiondialog.cpp
    rayeexceptionlistwidget.cpp
    rayeexceptionmodel.cpp
    rayeitemmodel.cpp
)
ki18n_wrap_ui(rayedecoration_config_SRCS
   ui/rayeconfigurationui.ui
   ui/rayeexceptiondialog.ui
   ui/rayeexceptionlistwidget.ui
)

kcoreaddons_add_plugin(kcm_rayedecoration SOURCES kcm_rayedecoration.cpp ${rayedecoration_config_SRCS} INSTALL_NAMESPACE "${KDECORATION_KCM_PLUGIN_DIR}")
kconfig_add_kcfg_files(kcm_rayedecoration ../rayesettings.kcfgc)
target_include_directories(kcm_rayedecoration PRIVATE ${CMAKE_SOURCE_DIR}/kdecoration ${CMAKE_BINARY_DIR}/kdecoration/)
target_link_libraries(kcm_rayedecoration
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
    PRIVATE
        KF6::ConfigCore
        KF6::CoreAddons
        KF6::GuiAddons
        KF6::I18n
        KF6::IconThemes
        KF6::KCMUtils
)
kcmutils_generate_desktop_file(kcm_rayedecoration)
