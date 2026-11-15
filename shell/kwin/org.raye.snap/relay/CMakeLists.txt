# raye-snap-relay and its D-Bus activation file: org.raye.Snap for the
# window decoration, relayed to Raye Snap's shortcut.
#
# Always built; installed only with RAYE_SHELL_SNAP_RELAY=ON, which the
# package turns on together with its install lines for
# usr/lib/*/libexec/raye-snap-relay and usr/share/dbus-1/services/org.raye.Snap.service.
# Without it, Win+Z works, resting on Maximize does nothing and Win+arrows
# stay with KWin's quick tiling.

option(RAYE_SHELL_SNAP_RELAY "Install the org.raye.Snap relay of Raye Snap" OFF)

add_executable(raye-snap-relay main.cpp)
target_link_libraries(raye-snap-relay PRIVATE Qt6::Core Qt6::DBus)

if(RAYE_SHELL_SNAP_RELAY)
    install(TARGETS raye-snap-relay DESTINATION ${KDE_INSTALL_LIBEXECDIR})
    set(RAYE_SNAP_RELAY "${KDE_INSTALL_FULL_LIBEXECDIR}/raye-snap-relay")
    configure_file(org.raye.Snap.service.in ${CMAKE_CURRENT_BINARY_DIR}/org.raye.Snap.service @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.raye.Snap.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
endif()
