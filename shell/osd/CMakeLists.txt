# The Raye on-screen displays: volume, brightness, caps lock and the other
# short messages, drawn as the pill at the bottom of the screen, with the
# media card above it when a player is on.
#
# plasmashell loads them from its own module, org.kde.plasma.workspace.osd,
# whose qmldir prefers the copies compiled into the plugin. raye-shell moves
# that qmldir, Osd.qml and OsdItem.qml aside and links the files below in
# their place (debian/raye-shell.preinst); this qmldir is the stock one
# without the "prefer" line, so the files on disk are the ones loaded.

install(FILES qmldir Osd.qml OsdItem.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/raye/shell/osd)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
