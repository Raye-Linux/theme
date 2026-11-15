# raye-run: the Run box (Win+R).

find_package(LayerShellQt REQUIRED)

# The parsing and the history, without any window: shared with the tests.
add_library(raye-run-core STATIC src/runparser.cpp src/runhistory.cpp)
target_include_directories(raye-run-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(raye-run-core PUBLIC Qt6::Core KF6::ConfigCore KF6::CoreAddons KF6::KIOCore KF6::Service)

add_executable(raye-run src/main.cpp src/runapp.cpp src/runbox.cpp)
target_link_libraries(raye-run PRIVATE
    raye-run-core
    Qt6::Widgets Qt6::Quick Qt6::DBus
    KF6::I18n KF6::I18nQml KF6::KIOGui KF6::KIOWidgets
    LayerShellQt::Interface)

# Every file sits at the root of the module, so they see each other.
set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/RunButton.qml PROPERTIES QT_RESOURCE_ALIAS RunButton.qml)
# The theme's design tokens, compiled in until org.raye.ui is installed.
set_source_files_properties(${RAYE_THEME_QML_DIR}/org/raye/ui/Tokens.qml PROPERTIES
    QT_RESOURCE_ALIAS Tokens.qml
    QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(raye-run
    URI org.raye.shell.run
    NO_PLUGIN
    QML_FILES
        qml/Main.qml
        qml/RunButton.qml
        ${RAYE_THEME_QML_DIR}/org/raye/ui/Tokens.qml)

install(TARGETS raye-run ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
configure_file(org.raye.run.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/org.raye.run.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.raye.run.desktop DESTINATION ${KDE_INSTALL_APPDIR})
# once per user: an empty Win+R left by an upgrade inside a session goes
install(FILES data/raye-run-shortcut.upd DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
install(PROGRAMS data/raye-run-shortcut.sh DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
