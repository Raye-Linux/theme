// The desktop each account last signed in to, kept by the greeter next
// to SDDM's own state (the sddm user's home). Loaded through a Loader:
// without QtCore the screen still works, it just forgets.
import QtCore

Settings {
    location: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation) + "/raye-signin.conf"
}
