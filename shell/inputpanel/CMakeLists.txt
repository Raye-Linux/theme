# raye-inputpanel: emoji, kaomoji, symbols and clipboard history (Win+. and Win+V).

# the protocol code generated for fake input is C
enable_language(C)

find_package(LayerShellQt REQUIRED)
find_package(PlasmaWaylandProtocols REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS WaylandClient)

# Everything that needs no window: shared with the tests.
add_library(raye-inputpanel-core STATIC
    src/emojidict.cpp
    src/chargroups.cpp
    src/clipboardstore.cpp
    src/placement.cpp)
target_include_directories(raye-inputpanel-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(raye-inputpanel-core PUBLIC Qt6::Core KF6::ConfigCore)

add_executable(raye-inputpanel
    src/main.cpp
    src/panel.cpp
    src/emojimodel.cpp
    src/klipperbackend.cpp
    src/fakeinput.cpp)
qt6_generate_wayland_protocol_client_sources(raye-inputpanel
    FILES ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml)
target_link_libraries(raye-inputpanel PRIVATE
    raye-inputpanel-core
    Qt6::Gui Qt6::Quick Qt6::DBus Qt6::WaylandClient
    KF6::I18n KF6::I18nQml
    LayerShellQt::Interface)

qt_add_resources(raye-inputpanel inputpanel_data
    PREFIX /raye-inputpanel
    BASE data
    FILES data/kaomoji.txt data/symbols.txt)

# Every file sits at the root of the module, so they see each other.
set(_raye_inputpanel_qml Main.qml PanelButton.qml SearchField.qml EmojiPage.qml CharPage.qml ClipboardPage.qml)
set(_raye_inputpanel_qml_files "")
foreach(_file IN LISTS _raye_inputpanel_qml)
    set_source_files_properties(qml/${_file} PROPERTIES QT_RESOURCE_ALIAS ${_file})
    list(APPEND _raye_inputpanel_qml_files qml/${_file})
endforeach()
# The theme's design tokens, compiled in until org.raye.ui is installed.
set_source_files_properties(${RAYE_THEME_QML_DIR}/org/raye/ui/Tokens.qml PROPERTIES
    QT_RESOURCE_ALIAS Tokens.qml
    QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(raye-inputpanel
    URI org.raye.shell.inputpanel
    NO_PLUGIN
    QML_FILES ${_raye_inputpanel_qml_files} ${RAYE_THEME_QML_DIR}/org/raye/ui/Tokens.qml)

install(TARGETS raye-inputpanel ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
configure_file(org.raye.inputpanel.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/org.raye.inputpanel.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.raye.inputpanel.desktop DESTINATION ${KDE_INSTALL_APPDIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
