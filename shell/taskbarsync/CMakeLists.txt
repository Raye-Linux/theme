# Raye taskbars on every screen: raye-taskbar-sync applies
# ~/.config/raye-taskbarrc to the panels (sync.js, a Plasma desktop
# script) and copies the main Start button's pinned apps to the others
# (raye-taskbar-pins); a KWin script starts it as a user service when
# screens change, and an env script gives every account the taskbar's
# Raye right-click menu. raye-taskbar-switch (switch.js) moves a stock
# taskbar over to Raye's own applets. No compiled code; the tests run
# sync.js and switch.js against a stand-in of Plasma's scripting API, and
# the pins helper's logic.

install(PROGRAMS raye-taskbar-sync raye-taskbar-pins raye-taskbar-switch DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec)
install(FILES sync.js switch.js DESTINATION ${KDE_INSTALL_DATADIR}/raye/taskbar)
install(FILES raye-taskbar-sync.service DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})
install(DIRECTORY kwin/
    DESTINATION ${KDE_INSTALL_DATADIR}/kwin-wayland/scripts/org.raye.taskbarsync)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
