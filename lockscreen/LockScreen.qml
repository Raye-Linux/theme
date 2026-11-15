// The Raye lock screen: the main file the screen locker's greeter loads
// from the desktop shell package. Installed in place of the stock file
// (which is kept beside it under another name).
//
// The root keeps the names the greeter looks for (the properties it
// writes and the signals it connects); everything else comes from the
// org.raye.signin module and the files next to this one, never from the
// stock lock screen files, so their updates cannot break this one. If
// this file ever fails to load, the greeter shows its own plain lock
// screen: the session stays locked.
//
// Context the greeter gives: authenticator, config, wallpaper,
// kscreenlocker_userName, kscreenlocker_userImage.
import QtQuick
import org.raye.signin
import "screens.js" as Screens

Item {
    id: root

    property bool debug: false
    property string notification
    signal clearPassword()
    signal notificationRepeated()

    // These are magical properties that kscreenlocker looks for
    property bool viewVisible: false
    property bool suspendToRamSupported: false
    property bool suspendToDiskSupported: false
    // written by the greeter: false during the grace time after locking
    property bool locked: true

    // These are magical signals that kscreenlocker looks for
    signal suspendToDisk()
    signal suspendToRam()

    // the greeter's context, read safely so a missing piece never stops
    // the file from loading
    readonly property QtObject pam: typeof authenticator !== "undefined" ? authenticator : null
    readonly property string userName: typeof kscreenlocker_userName !== "undefined"
                                       && kscreenlocker_userName ? kscreenlocker_userName : ""
    readonly property string userImage: typeof kscreenlocker_userImage !== "undefined"
                                        && kscreenlocker_userImage ? kscreenlocker_userImage : ""
    readonly property var wallpaperItem: typeof wallpaper !== "undefined" ? wallpaper : null
    readonly property string wallpaperImage: {
        var w = wallpaperItem
        if (!w || !w.configuration)
            return ""
        var image = w.configuration.Image
        return typeof image === "string" ? image : ""
    }
    readonly property bool showMedia: typeof config === "undefined" || !config
                                      || config.showMediaControls !== false

    implicitWidth: 800
    implicitHeight: 600

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    onClearPassword: ui.clearPassword()

    // the wallpaper plugin's own item sits under everything and is fully
    // covered by the Raye picture: do not draw it
    function hideWallpaperItem() {
        if (wallpaperItem && wallpaperItem.visible !== undefined)
            wallpaperItem.visible = false
    }
    onWallpaperItemChanged: hideWallpaperItem()
    Component.onCompleted: hideWallpaperItem()

    LockAdapter {
        id: lockAdapter
    }

    Binding {
        target: Motion
        property: "reducedMotion"
        value: lockAdapter.reducedMotion
    }

    LockUi {
        id: ui
        anchors.fill: parent
        focus: true

        authenticator: root.pam
        userName: root.userName
        userImagePath: root.userImage
        wallpaperImage: root.wallpaperImage
        locked: root.locked
        canSuspend: root.suspendToRamSupported
        canHibernate: root.suspendToDiskSupported
        showMedia: root.showMedia
        adapter: lockAdapter
        // the greeter draws one of these per screen: they all show one
        // sign-in state (see LockShare.qml)
        share: Screens.share(Qt.resolvedUrl("LockShare.qml"))

        onSuspendRequested: root.suspendToRam()
        onHibernateRequested: root.suspendToDisk()
        // the greeter exits only when the authenticator really succeeded
        onUnlocked: Qt.quit()
    }
}
