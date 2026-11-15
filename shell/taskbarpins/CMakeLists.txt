# org.raye.taskbarpins: one list of pinned apps for every taskbar, and
# settings written by raye-taskbar-sync or Raye Settings applied at once,
# and the menu of an app button in the taskbar's words, without Plasma's
# settings entries.
# A Plasma containment actions plugin, loaded with every panel; the
# shell package's default layout sets it up for a new account, and its
# updates script for an account made before (raye/desktop).

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Config I18n)

kcoreaddons_add_plugin(org.raye.taskbarpins
    SOURCES src/taskbarpins.cpp src/pinrules.cpp
    INSTALL_NAMESPACE "plasma/containmentactions")
target_link_libraries(org.raye.taskbarpins PRIVATE
    Plasma::Plasma KF6::ConfigCore KF6::ConfigGui KF6::ConfigQml KF6::CoreAddons KF6::I18n
    Qt6::Qml Qt6::Widgets)
target_compile_definitions(org.raye.taskbarpins PRIVATE TRANSLATION_DOMAIN="raye_shell_taskbarpins")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
