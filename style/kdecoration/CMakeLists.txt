add_definitions(-DTRANSLATION_DOMAIN="raye_decoration")

################# includes #################
include_directories(${CMAKE_SOURCE_DIR}/libraye-common)
include_directories(${CMAKE_BINARY_DIR}/libraye-common)

################# newt target #################
### plugin classes
set(rayedecoration_SRCS
    rayebutton.cpp
    rayedecoration.cpp
    rayesettingsprovider.cpp
    rayeexceptionlist.cpp
    rayedecorationmetrics.cpp
)

### build library
kconfig_add_kcfg_files(rayedecoration_SRCS rayesettings.kcfgc)
add_library(rayedecoration MODULE
    ${rayedecoration_SRCS}
    ${rayedecoration_config_PART_FORMS_HEADERS})

set_target_properties(rayedecoration PROPERTIES
    OUTPUT_NAME org.raye.decoration
)

target_link_libraries(rayedecoration
    PRIVATE
        rayecommon6
        Qt6::DBus
        KF6::CoreAddons
        KF6::ConfigGui
        KF6::GuiAddons
        KF6::I18n
        KF6::IconThemes
        KDecoration3::KDecoration
        KF6::ColorScheme
)

install(TARGETS rayedecoration DESTINATION ${KDE_INSTALL_PLUGINDIR}/${KDECORATION_PLUGIN_DIR})

add_subdirectory(config)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

