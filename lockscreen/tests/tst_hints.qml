import QtQuick
import QtTest
import "../hints.js" as Hints

TestCase {
    name: "Hints"

    readonly property int fp: Hints.FINGERPRINT
    readonly property int sc: Hints.SMARTCARD

    // --- which method line shows ----------------------------------------

    function test_noHintWithoutPrompt() {
        var s = Hints.initial()
        // the greeter flags the reader as available as soon as it starts,
        // before PAM has said anything: no line yet
        compare(Hints.hint(s, fp, 0), "")
        compare(Hints.hint(s, fp | sc, 0), "")
        compare(Hints.hint(s, 0, 0), "")
    }

    function test_fingerprintAfterItsPrompt() {
        var s = Hints.spoke(Hints.initial(), fp)
        compare(Hints.hint(s, fp, 0), "fingerprint")
    }

    function test_promptOfAnotherKindDoesNotCount() {
        var s = Hints.spoke(Hints.initial(), sc)
        compare(Hints.hint(s, fp | sc, 0), "")
    }

    function test_needsAvailabilityToo() {
        // spoke, but the greeter no longer lists it
        var s = Hints.spoke(Hints.initial(), fp)
        compare(Hints.hint(s, 0, 0), "")
        compare(Hints.hint(s, sc, 0), "")
    }

    function test_goneAfterUnavailable() {
        var s = Hints.spoke(Hints.initial(), fp)
        s = Hints.available(s, 0)
        compare(Hints.hint(s, 0, 0), "")
        // back in the list, but it has to speak again first
        compare(Hints.hint(s, fp, 0), "")
        s = Hints.spoke(s, fp)
        compare(Hints.hint(s, fp, 0), "fingerprint")
    }

    function test_availabilityOfOtherKindsKeepsIt() {
        var s = Hints.spoke(Hints.initial(), fp)
        s = Hints.available(s, fp | sc)
        compare(Hints.hint(s, fp | sc, 0), "fingerprint")
    }

    function test_smartcardNever() {
        var s = Hints.initial()
        s = Hints.spoke(s, sc)
        s = Hints.failed(s, sc, 0)
        compare(Hints.hint(s, sc, 0), "")
        compare(Hints.hint(s, fp | sc, 0), "")
        compare(Hints.hintText("", "en"), "")
    }

    function test_noMatchShowsRetryForAWhile() {
        var s = Hints.spoke(Hints.initial(), fp)
        s = Hints.failed(s, fp, 1000)
        compare(Hints.hint(s, fp, 1000), "fingerprintRetry")
        compare(Hints.hint(s, fp, 1000 + Hints.RETRY_SHOWN - 1), "fingerprintRetry")
        compare(Hints.hint(s, fp, 1000 + Hints.RETRY_SHOWN), "fingerprint")
        // an error means the reader is there and talking
        var t = Hints.failed(Hints.initial(), fp, 0)
        compare(Hints.hint(t, fp, 10), "fingerprintRetry")
    }

    function test_retryGoneWhenUnavailable() {
        var s = Hints.failed(Hints.spoke(Hints.initial(), fp), fp, 0)
        s = Hints.available(s, 0)
        compare(Hints.hint(s, fp, 10), "")
    }

    function test_stateIsNotChangedInPlace() {
        var s = Hints.initial()
        var t = Hints.spoke(s, fp)
        compare(Hints.hint(s, fp, 0), "")
        compare(Hints.hint(t, fp, 0), "fingerprint")
    }

    function test_hintTexts() {
        compare(Hints.hintText("fingerprint", "en"), "Scan your finger on the fingerprint reader")
        compare(Hints.hintText("fingerprint", "fr"), "Posez votre doigt sur le lecteur d'empreintes")
        verify(Hints.hintText("fingerprintRetry", "en").length > 0)
        verify(Hints.hintText("fingerprintRetry", "fr").length > 0)
        verify(Hints.hintText("fingerprintRetry", "en") !== Hints.hintText("fingerprintRetry", "fr"))
        compare(Hints.hintIcon("fingerprint"), "")
    }

    // --- words of the lock screen's own --------------------------------

    function test_wordsCompleteInBothLanguages() {
        var en = Hints.WORDS.en, fr = Hints.WORDS.fr
        var ek = Object.keys(en).sort(), fk = Object.keys(fr).sort()
        compare(fk, ek)
        for (var i = 0; i < ek.length; ++i) {
            verify(en[ek[i]].length > 0, ek[i])
            verify(fr[ek[i]].length > 0, ek[i])
        }
        compare(Hints.text("play", "fr"), fr.play)
        compare(Hints.text("play", "de"), en.play)
    }

    function test_noForbiddenWords() {
        var all = JSON.stringify(Hints.WORDS)
        var bad = ["KDE", "Plasma", "Wine", "Windows", "kscreenlocker", "PAM"]
        for (var i = 0; i < bad.length; ++i)
            verify(all.indexOf(bad[i]) < 0, bad[i])
    }

    // --- status icons -------------------------------------------------

    function test_batteryIcon() {
        compare(Hints.batteryIcon(false, 50, false), "")
        compare(Hints.batteryIcon(true, 0, false), "battery-000")
        compare(Hints.batteryIcon(true, 4, false), "battery-000")
        compare(Hints.batteryIcon(true, 5, false), "battery-010")
        compare(Hints.batteryIcon(true, 47, true), "battery-050-charging")
        compare(Hints.batteryIcon(true, 100, false), "battery-100")
        compare(Hints.batteryIcon(true, 130, false), "battery-100")
        compare(Hints.batteryIcon(true, -3, false), "battery-000")
        compare(Hints.batteryIcon(true, NaN, false), "")
    }

    function test_networkIcon() {
        compare(Hints.networkIcon(""), "")
        compare(Hints.networkIcon(undefined), "")
        compare(Hints.networkIcon("network-wired-activated"), "network-wired")
        compare(Hints.networkIcon("network-wired"), "network-wired")
        compare(Hints.networkIcon("network-wireless-0"), "network-wireless-0")
        compare(Hints.networkIcon("network-wireless-20"), "network-wireless-1")
        compare(Hints.networkIcon("network-wireless-40-locked"), "network-wireless-2")
        compare(Hints.networkIcon("network-wireless-60"), "network-wireless-3")
        compare(Hints.networkIcon("network-wireless-80"), "network-wireless-4")
        compare(Hints.networkIcon("network-wireless-100"), "network-wireless-4")
        compare(Hints.networkIcon("network-mobile-60"), "network-wireless-3")
        compare(Hints.networkIcon("network-wireless-connected-60"), "network-wireless-3")
        compare(Hints.networkIcon("network-disconnect"), "network-none")
        compare(Hints.networkIcon("network-offline"), "network-none")
        compare(Hints.networkIcon("network-unavailable"), "network-none")
        compare(Hints.networkIcon("network-wireless-disconnected"), "network-none")
        compare(Hints.networkIcon("network-limited"), "network-none")
        compare(Hints.networkIcon("network-vpn"), "network-wired")
        compare(Hints.networkIcon("something-else"), "")
    }

    function test_layoutCode() {
        compare(Hints.layoutCode("fr"), "FR")
        compare(Hints.layoutCode("us"), "US")
        compare(Hints.layoutCode("ca(fr)"), "CA")
        compare(Hints.layoutCode(""), "")
        compare(Hints.layoutCode(undefined), "")
        compare(Hints.layoutCode("abcdef"), "ABC")
    }

    // --- pictures -----------------------------------------------------

    function test_userPictureUrl() {
        compare(Hints.userPictureUrl(""), "")
        compare(Hints.userPictureUrl(undefined), "")
        compare(Hints.userPictureUrl("/var/lib/AccountsService/icons/anna"),
                "file:///var/lib/AccountsService/icons/anna")
        compare(Hints.userPictureUrl("/home/jo ann/.face#1"), "file:///home/jo%20ann/.face%231")
        // only absolute paths
        compare(Hints.userPictureUrl("relative/face"), "")
    }

    function test_pictureFromWallpaper() {
        // nothing set, or Raye's own picture: the module's default picture
        compare(Hints.lockPicture(undefined), "")
        compare(Hints.lockPicture(""), "")
        compare(Hints.lockPicture("file:///usr/share/wallpapers/Raye/"), "")
        compare(Hints.lockPicture("/usr/share/wallpapers/Raye"), "")
        compare(Hints.lockPicture("file:///usr/share/wallpapers/Raye/contents/images/1920x1080.png"), "")
        // a picture file
        compare(Hints.lockPicture("file:///home/a/Pictures/sea.jpg"), "file:///home/a/Pictures/sea.jpg")
        compare(Hints.lockPicture("/home/a/Pictures/sea.PNG"), "file:///home/a/Pictures/sea.PNG")
        compare(Hints.lockPicture("/home/a/my pics/sea.webp"), "file:///home/a/my%20pics/sea.webp")
        // a wallpaper package: its 1920x1080 picture
        compare(Hints.lockPicture("file:///usr/share/wallpapers/Next/"),
                "file:///usr/share/wallpapers/Next/contents/images/1920x1080.png")
        compare(Hints.lockPicture("/usr/share/wallpapers/Next"),
                "file:///usr/share/wallpapers/Next/contents/images/1920x1080.png")
        // nothing remote, nothing odd
        compare(Hints.lockPicture("https://example.com/a.jpg"), "")
        compare(Hints.lockPicture("image://wallpaper/x"), "")
        compare(Hints.lockPicture({}), "")
    }
}
