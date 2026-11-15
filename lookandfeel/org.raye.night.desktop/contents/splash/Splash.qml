// The login splash: the very frame the sign-in screen ended on, so
// greeter -> splash -> desktop reads as one screen. On the first login of
// an account it is the first sign-in frame instead ("Hi").
//
// session/raye-first-login.sh answers "first login?" with a marker picture,
// since ksplash gives QML nothing else to ask. Its path comes from QtCore,
// loaded at run time: a missing module costs the "Hi", never the splash.
//
// ksplash leaves as soon as Plasma says it is ready, so the three seconds
// the first sign-in frame would like are not this file's to hold.
import QtQuick
import org.raye.signin

Item {
    id: root

    // set by ksplash as Plasma starts; nothing here counts steps
    property int stage

    // empty when the state folder could not be found
    property url markerUrl: stateMarker()

    readonly property string mode: marker.status === Image.Ready ? "firstSignIn" : "welcome"

    function stateMarker() {
        try {
            var paths = Qt.createQmlObject(
                "import QtQuick\nimport QtCore\n"
                + "QtObject { function state() { return StandardPaths.writableLocation(StandardPaths.GenericStateLocation) } }",
                root, "Splash")
            var folder = paths.state().toString()
            paths.destroy()
            return folder + "/raye/first-login.png"
        } catch (e) {
            return ""
        }
    }

    function followAnimationSpeed() {
        try {
            var units = Qt.createQmlObject(
                "import QtQuick\nimport org.kde.kirigami as K\n"
                + "QtObject { readonly property bool reduced: K.Units.longDuration <= 1 }",
                root, "Splash")
            Motion.reducedMotion = units.reduced
            units.destroy()
        } catch (e) {
            // no Kirigami here: the default motion
        }
    }

    Component.onCompleted: followAnimationSpeed()

    Image {
        id: marker
        visible: false
        source: root.markerUrl
        sourceSize: Qt.size(1, 1)
    }

    StatusScreen {
        objectName: "status"
        anchors.fill: parent
        mode: root.mode
    }
}
