# raye-app-icons: an app entry in the account's folder always gets an icon
# that works. No compiled code. The user unit is enabled by a link in the
# package (debian/raye-shell.links).

install(PROGRAMS raye-app-icons DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec)
install(FILES raye-app-icons.service raye-app-icons.path DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME appicons COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_app_icons.py)
    set_tests_properties(appicons PROPERTIES TIMEOUT 60)
    add_test(NAME appicons-webapps COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_web_app_rules.py)
    set_tests_properties(appicons-webapps PROPERTIES TIMEOUT 60)
endif()
