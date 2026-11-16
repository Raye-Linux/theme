# New app windows open maximized (a KWin script): only resizable app
# windows, never dialogs, the shell's menus or fixed-size windows.
install(DIRECTORY kwin/
    DESTINATION ${KDE_INSTALL_DATADIR}/kwin-wayland/scripts/org.raye.openmaximized)
