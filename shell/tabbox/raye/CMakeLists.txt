# The Raye window switcher (Alt+Tab): a KWin tabbox package, installed to
# share/kwin-wayland/tabbox/raye and picked by [TabBox] LayoutName=raye.

install(DIRECTORY package/
    DESTINATION ${KDE_INSTALL_DATADIR}/kwin-wayland/tabbox/raye)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
