# Tests of the USB icon: its menu rows, the busy-device check and the
# words of its toasts (devicelogic.cpp), without Solid or D-Bus.

add_executable(tst_devices tst_devices.cpp ../devicelogic.cpp)
target_include_directories(tst_devices PRIVATE ..)
target_link_libraries(tst_devices PRIVATE Qt6::Test KF6::I18n KF6::CoreAddons)
target_compile_definitions(tst_devices PRIVATE TRANSLATION_DOMAIN="raye_shell_devices"
    RAYE_SHELL_PO_DIR="${RAYE_SHELL_PO_DIR}")
add_test(NAME devices-applet COMMAND tst_devices)
set_tests_properties(devices-applet PROPERTIES TIMEOUT 60 ENVIRONMENT "LANGUAGE=en;LANG=C.UTF-8")
