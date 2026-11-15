### configuration plugin
add_definitions(-DTRANSLATION_DOMAIN=\"raye_style_config\")
set(kstyle_raye_config_PART_SRCS rayestyleconfig.cpp)

set(kstyle_raye_config_PART_SRCS ${kstyle_raye_config_PART_SRCS} rayestyleconfigmodule.cpp)

kconfig_add_kcfg_files(kstyle_raye_config_PART_SRCS ../rayestyleconfigdata.kcfgc)
set(kstyle_raye_config_PART_FORMS ui/rayestyleconfig.ui)
ki18n_wrap_ui(kstyle_raye_config_PART_FORMS_HEADERS ${kstyle_raye_config_PART_FORMS})

kcoreaddons_add_plugin(rayestyleconfig SOURCES
    ${kstyle_raye_config_PART_SRCS}
    ${kstyle_raye_config_PART_FORMS_HEADERS}
    INSTALL_NAMESPACE "kstyle_config"
)

target_link_libraries(rayestyleconfig
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

if(HAVE_QTDBUS)
    target_link_libraries(rayestyleconfig
        PUBLIC
            Qt6::DBus
    )
endif()

target_link_libraries(rayestyleconfig
    PUBLIC
        KF6::CoreAddons
        KF6::ConfigCore
        KF6::ConfigWidgets
        KF6::I18n
)
if(QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(rayestyleconfig
        PUBLIC
    KF6::KCMUtils
)
endif()
kcmutils_generate_desktop_file(rayestyleconfig)

set(raye_settings_SOURCES main.cpp)
add_executable(raye-style-settings ${raye_settings_SOURCES} )
target_link_libraries(raye-style-settings Qt6::Core Qt6::Gui Qt6::Widgets )
target_link_libraries(raye-style-settings KF6::I18n KF6::KCMUtils KF6::CoreAddons)

set_target_properties(raye-style-settings PROPERTIES OUTPUT_NAME raye-style-settings)

install(TARGETS raye-style-settings ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# icon
ecm_install_icons(ICONS sc-apps-raye-style-settings.svgz
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor )
