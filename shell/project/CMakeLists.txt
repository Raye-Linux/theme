# raye-project: the Project panel (Win+P).

find_package(LayerShellQt REQUIRED)
find_package(KF6Screen REQUIRED)

# Which screen does what, without any window: shared with the tests.
add_library(raye-project-core STATIC src/displayplan.cpp)
target_include_directories(raye-project-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(raye-project-core PUBLIC Qt6::Core)

add_executable(raye-project src/main.cpp src/displaycontroller.cpp src/switcherservice.cpp)
target_link_libraries(raye-project PRIVATE
    raye-project-core
    Qt6::Gui Qt6::Quick Qt6::DBus
    KF6::I18n KF6::I18nQml KF6::Screen KF6::KIOGui KF6::Service
    LayerShellQt::Interface)

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/ProjectPanel.qml PROPERTIES QT_RESOURCE_ALIAS ProjectPanel.qml)
set_source_files_properties(qml/ProjectGlyph.qml PROPERTIES QT_RESOURCE_ALIAS ProjectGlyph.qml)

qt_add_qml_module(raye-project
    URI org.raye.shell.project
    NO_PLUGIN
    QML_FILES
        qml/Main.qml
        qml/ProjectPanel.qml
        qml/ProjectGlyph.qml)

install(TARGETS raye-project ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
configure_file(org.raye.project.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/org.raye.project.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.raye.project.desktop DESTINATION ${KDE_INSTALL_APPDIR})
# the stock switcher's service starts this instead
install(FILES data/raye-project-osd.conf
    DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR}/plasma-kscreen-osd.service.d)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
