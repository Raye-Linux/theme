# Raye Snap: snap layouts, the snap bar, snap assist and Windows' arrow
# keys, as a KWin script (QML and JavaScript, no compiled code), installed
# to share/kwin-wayland/scripts/org.raye.snap. kwinrc turns it on
# ([Plugins] org.raye.snapEnabled=true, theme/config/kwinrc).
#
# relay/ answers the org.raye.Snap D-Bus call of the window decoration
# (resting on Maximize) by invoking the script's shortcut: a KWin script
# cannot own a D-Bus name itself. It also hands the script Windows' arrow
# keys, which KWin's quick tiling holds otherwise.

install(DIRECTORY package/
    DESTINATION ${KDE_INSTALL_DATADIR}/kwin-wayland/scripts/org.raye.snap)

add_subdirectory(relay)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
