# org.raye.devices: the USB icon of the taskbar's tray. A compiled applet:
# Solid tells it about removable drives, udisks powers a drive off, and
# its toasts go to the notification server. Its QML is built into the
# plugin, like org.raye.clock's.

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Solid I18n KIO)

set_source_files_properties(contents/ui/main.qml PROPERTIES QT_RESOURCE_ALIAS main.qml)

plasma_add_applet(org.raye.devices
    QML_SOURCES contents/ui/main.qml
    CPP_SOURCES devicesapplet.cpp devicesapplet.h devicelogic.cpp devicelogic.h)

target_link_libraries(org.raye.devices PRIVATE
    Qt6::Core Qt6::DBus Qt6::Qml Plasma::Plasma
    KF6::CoreAddons KF6::ConfigCore KF6::I18n KF6::KIOGui KF6::Solid KF6::Service)
target_compile_definitions(org.raye.devices PRIVATE TRANSLATION_DOMAIN="raye_shell_devices")
