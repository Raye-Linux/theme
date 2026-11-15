#!/usr/bin/env python3
"""A stand-in for Klipper on the session bus, to look at the clipboard
page of the panel without a Plasma session:

    dbus-run-session -- sh -c 'python3 fake-klipper.py & sleep 1; raye-inputpanel --clipboard'
"""

import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

IFACE = "org.kde.klipper.klipper"


class Klipper(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, "/klipper")
        self.items = [
            "https://raye.example.org/download",
            "Meeting moved to Thursday at 10:30, room B.",
            "sudo apt install fonts-inter",
            "Dear Ana,\nthank you for the photos of the trip, they are lovely.\nSee you soon!",
            "42 Rue des Lilas, 75011 Paris",
        ]

    @dbus.service.method(IFACE, out_signature="as")
    def getClipboardHistoryMenu(self):
        return self.items

    @dbus.service.method(IFACE, out_signature="s")
    def getClipboardContents(self):
        return self.items[0] if self.items else ""

    @dbus.service.method(IFACE, in_signature="s")
    def setClipboardContents(self, text):
        if text in self.items:
            self.items.remove(text)
        self.items.insert(0, text)
        self.clipboardHistoryUpdated()

    @dbus.service.method(IFACE)
    def clearClipboardHistory(self):
        self.items = []
        self.clipboardHistoryUpdated()

    @dbus.service.signal(IFACE)
    def clipboardHistoryUpdated(self):
        pass


DBusGMainLoop(set_as_default=True)
session = dbus.SessionBus()
name = dbus.service.BusName("org.kde.klipper", session)
service = Klipper(session)
GLib.MainLoop().run()
