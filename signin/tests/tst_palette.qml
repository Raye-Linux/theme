import QtQuick
import QtTest
import org.raye.signin
import "Setup.js" as Setup

TestCase {
    id: test
    name: "Palette"
    width: 640
    height: 360
    when: windowShown
    // a TestCase is hidden by default, and so is everything made in it
    visible: true

    function initTestCase() {
        Setup.assets(RayePalette)
    }

    function cleanup() {
        RayePalette.highContrast = false
        RayePalette.largerText = false
    }

    function test_colours() {
        verify(Qt.colorEqual(RayePalette.text, "#f1f0f4"))
        verify(Qt.colorEqual(RayePalette.textSecondary, "#b5b3bc"))
        verify(Qt.colorEqual(RayePalette.accent, "#7b3cff"))
        verify(Qt.colorEqual(RayePalette.glow, RayePalette.accent))
        verify(Qt.colorEqual(RayePalette.error, "#ff7a9a"))
        compare(RayePalette.lockTint, 0.45)
        compare(RayePalette.signInTint, 0.62)
    }

    function test_high_contrast() {
        RayePalette.highContrast = true
        verify(Qt.colorEqual(RayePalette.text, "#ffffff"))
        verify(Qt.colorEqual(RayePalette.focus, "#ffd400"))
        compare(RayePalette.focusWidth, 3)
    }

    function test_larger_text() {
        compare(RayePalette.textScale, 1.0)
        RayePalette.largerText = true
        compare(RayePalette.textScale, 1.25)
    }

    function test_pictures() {
        verify(RayePalette.icon("power").toString().match(/assets\/signin\/icons\/power\.svg$/))
        verify(RayePalette.defaultPicture(1080).toString().match(/1920x1080\.png$/))
        verify(RayePalette.defaultPicture(2160).toString().match(/3840x2160\.png$/))
        verify(RayePalette.defaultBlurred(1080).toString().match(/raye-night-blur-1920\.jpg$/))
        verify(RayePalette.isDefaultPicture(RayePalette.defaultPicture(1080)))
        verify(!RayePalette.isDefaultPicture("file:///tmp/other.png"))
    }

    Component {
        id: backdropComponent
        Backdrop {
            width: 640
            height: 360
        }
    }

    // the software scene graph draws no blur: the pre-blurred picture of
    // the default wallpaper stands in, or a stronger tint
    function test_backdrop_software_fallback() {
        var b = createTemporaryObject(backdropComponent, test)
        verify(b)
        if (!b.software) {
            // with a GPU the blur is drawn live
            verify(!b.usesBlurEffect)
            b.blurred = true
            verify(b.usesBlurEffect)
            verify(!b.usesPreblurred)
            return
        }
        verify(!b.usesPreblurred)
        b.blurred = true
        tryCompare(b, "usesPreblurred", true)
        b.source = Qt.resolvedUrl("../../assets/signin/face-512.png")
        compare(b.blurredSource.toString(), "")
        verify(!b.usesPreblurred)
    }

    Component {
        id: pictureComponent
        UserPicture {}
    }

    function test_user_picture_fallback() {
        var p = createTemporaryObject(pictureComponent, test)
        compare(p.shown, RayePalette.defaultFace)
        p.source = "file:///nonexistent/face.png"
        tryCompare(p, "shown", RayePalette.defaultFace)
        var other = Qt.resolvedUrl("../../assets/signin/face-256.png")
        p.source = other
        tryVerify(function () { return p.shown.toString() === other.toString() })
        compare(p.ring, true)
        p.size = 40
        compare(p.ring, false)
        compare(p.glow, false)
    }
}
