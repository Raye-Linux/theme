################# includes #################
include_directories(
  animations
  debug
)

include_directories(${CMAKE_SOURCE_DIR}/libraye-common)
include_directories(${CMAKE_BINARY_DIR}/libraye-common)

################# configuration #################
configure_file(config-raye.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-raye.h )

########### next target ###############
set(raye_PART_SRCS
    animations/rayeanimation.cpp
    animations/rayeanimations.cpp
    animations/rayeanimationdata.cpp
    animations/rayebaseengine.cpp
    animations/rayebusyindicatordata.cpp
    animations/rayebusyindicatorengine.cpp
    animations/rayedialdata.cpp
    animations/rayedialengine.cpp
    animations/rayeenabledata.cpp
    animations/rayegenericdata.cpp
    animations/rayeheaderviewdata.cpp
    animations/rayeheaderviewengine.cpp
    animations/rayescrollbardata.cpp
    animations/rayescrollbarengine.cpp
    animations/rayespinboxengine.cpp
    animations/rayespinboxdata.cpp
    animations/rayestackedwidgetdata.cpp
    animations/rayestackedwidgetengine.cpp
    animations/rayetabbarengine.cpp
    animations/rayetabbardata.cpp
    animations/rayetoolboxengine.cpp
    animations/rayetransitiondata.cpp
    animations/rayetransitionwidget.cpp
    animations/rayewidgetstateengine.cpp
    animations/rayewidgetstatedata.cpp
    debug/rayewidgetexplorer.cpp
    rayeaddeventfilter.cpp
    rayeblurhelper.cpp
    rayeframeshadow.cpp
    rayehelper.cpp
    rayemdiwindowshadow.cpp
    rayemnemonics.cpp
    rayepropertynames.cpp
    rayeshadowhelper.cpp
    rayesplitterproxy.cpp
    rayestyle.cpp
    rayestyleplugin.cpp
    rayetileset.cpp
    rayetokens.cpp
    rayewindowmanager.cpp
    rayetoolsareamanager.cpp
)

kconfig_add_kcfg_files(raye_PART_SRCS ../kdecoration/rayesettings.kcfgc)
kconfig_add_kcfg_files(raye_PART_SRCS rayestyleconfigdata.kcfgc)


add_library(raye${QT_MAJOR_VERSION} MODULE ${raye_PART_SRCS})

ecm_qt_declare_logging_category(raye${QT_MAJOR_VERSION}
    HEADER
        raye_logging.h
    IDENTIFIER
        RAYE
    CATEGORY_NAME
        raye${QT_MAJOR_VERSION}
    DEFAULT_SEVERITY
        Warning
)

target_link_libraries(raye${QT_MAJOR_VERSION}
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Widgets
)

if(HAVE_QTDBUS)
    target_link_libraries(raye${QT_MAJOR_VERSION}
        Qt${QT_MAJOR_VERSION}::DBus
    )
endif()

if( RAYE_HAVE_QTQUICK )
    target_link_libraries(raye${QT_MAJOR_VERSION} 
        Qt${QT_MAJOR_VERSION}::Quick
        KF${QT_MAJOR_VERSION}::CoreAddons
    )
endif()

target_link_libraries(raye${QT_MAJOR_VERSION}
    KF${QT_MAJOR_VERSION}::CoreAddons
    KF${QT_MAJOR_VERSION}::ConfigCore
    KF${QT_MAJOR_VERSION}::ConfigGui
    KF${QT_MAJOR_VERSION}::GuiAddons
    KF${QT_MAJOR_VERSION}::IconThemes
    KF${QT_MAJOR_VERSION}::WindowSystem
)

if(QT_MAJOR_VERSION STREQUAL "5")
    target_link_libraries(raye5 KF5::ConfigWidgets)
    if (RAYE_HAVE_QTQUICK)
        target_link_libraries(raye5 KF5::Kirigami2)
    endif()
else()
    target_link_libraries(raye6 KF6::ColorScheme)
    if (RAYE_HAVE_QTQUICK)
        target_link_libraries(raye6 KF6::KirigamiPlatform)
    endif()
endif()

target_link_libraries(raye${QT_MAJOR_VERSION} rayecommon${QT_MAJOR_VERSION})

if(KF${QT_MAJOR_VERSION}FrameworkIntegration_FOUND)
    target_link_libraries(raye${QT_MAJOR_VERSION}  KF${QT_MAJOR_VERSION}::Style)
endif()


if (WIN32)
    # As stated in https://docs.microsoft.com/en-us/cpp/c-runtime-library/math-constants M_PI only gets defined
    # when if _USE_MATH_DEFINES is defined
    target_compile_definitions(raye${QT_MAJOR_VERSION}  PRIVATE _USE_MATH_DEFINES _BSD_SOURCE)
endif()


########### install files ###############
install(TARGETS raye${QT_MAJOR_VERSION}  DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/styles/)
install(FILES raye.themerc  DESTINATION  ${KDE_INSTALL_DATADIR}/kstyle/themes)
# system defaults of the style (and the decoration), /etc/xdg/rayerc
install(FILES rayerc DESTINATION ${KDE_INSTALL_CONFDIR})

########### subdirectories ###############

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

if (QT_MAJOR_VERSION EQUAL "6" AND TARGET "KF6::KCMUtils")
    add_subdirectory(config)
endif()
