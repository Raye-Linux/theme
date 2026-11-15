import QtQuick
import QtTest
import org.raye.signin

TestCase {
    name: "Strings"

    function cleanup() {
        Strings.lang = ""
    }

    function test_complete() {
        var en = Object.keys(Strings.en).sort()
        var fr = Object.keys(Strings.fr).sort()
        compare(fr, en, "the same words in both languages")
        for (var i = 0; i < en.length; ++i) {
            var k = en[i]
            verify(typeof Strings.en[k] === "string" && Strings.en[k].length > 0, "en." + k)
            verify(typeof Strings.fr[k] === "string" && Strings.fr[k].length > 0, "fr." + k)
            compare(Strings.fr[k].indexOf("%1") >= 0, Strings.en[k].indexOf("%1") >= 0, "%1 in " + k)
        }
    }

    function test_no_foreign_names() {
        var bad = /\b(KDE|Plasma|Wine|Breeze|Kirigami|SDDM|PAM|polkit|Windows)\b/
        var tables = [Strings.en, Strings.fr]
        for (var t = 0; t < tables.length; ++t)
            for (var k in tables[t])
                verify(!bad.test(tables[t][k]), k + ": " + tables[t][k])
    }

    function test_language_switch() {
        Strings.lang = "fr"
        compare(Strings.language, "fr")
        compare(Strings.tr.password, "Mot de passe")
        Strings.lang = "en"
        compare(Strings.tr.password, "Password")
    }

    // the words of the design, exactly
    function test_spec_words_data() {
        return [
            { tag: "password", en: "Password", fr: "Mot de passe" },
            { tag: "signIn", en: "Sign in", fr: "Se connecter" },
            { tag: "capsLock", en: "Caps Lock is on", fr: "Verr. Maj. activé" },
            { tag: "wrongPassword", en: "The password is incorrect. Try again.", fr: "Le mot de passe est incorrect. Réessayez." },
            { tag: "welcome", en: "Welcome", fr: "Bienvenue" },
            { tag: "sleep", en: "Sleep", fr: "Mettre en veille" },
            { tag: "shutDown", en: "Shut down", fr: "Arrêter" },
            { tag: "restart", en: "Restart", fr: "Redémarrer" },
            { tag: "accessibility", en: "Accessibility", fr: "Accessibilité" },
            { tag: "onScreenKeyboard", en: "On-screen keyboard", fr: "Clavier visuel" },
            { tag: "largerText", en: "Larger text", fr: "Texte plus grand" },
            { tag: "highContrast", en: "High contrast", fr: "Contraste élevé" },
            { tag: "pressKey", en: "Press a key or click to sign in", fr: "Appuyez sur une touche ou cliquez pour vous connecter" },
            { tag: "networkSignIn", en: "Sign in to connect to a network", fr: "Connectez-vous pour rejoindre un réseau" },
            { tag: "switchUser", en: "Switch user", fr: "Changer d'utilisateur" },
            { tag: "fingerprint", en: "Scan your finger on the fingerprint reader", fr: "Posez votre doigt sur le lecteur d'empreintes" },
            { tag: "hi", en: "Hi", fr: "Bonjour" },
            { tag: "gettingReady", en: "We're getting things ready", fr: "Nous préparons tout pour vous" },
            { tag: "mightTakeMinute", en: "This might take a minute", fr: "Cela peut prendre une minute" },
            { tag: "dontTurnOff", en: "Don't turn off your PC", fr: "N'éteignez pas votre PC" },
            { tag: "rootAccess", en: "Root access needed", fr: "Accès root requis" },
            { tag: "enterOwnPassword", en: "Enter your password to continue as root.", fr: "Entrez votre mot de passe pour continuer en tant que root." },
            { tag: "enterSudoPassword", en: "To continue as root, enter the password of a sudo user.", fr: "Pour continuer en tant que root, entrez le mot de passe d'un utilisateur sudo." }
        ]
    }

    function test_spec_words(data) {
        compare(Strings.en[data.tag], data.en)
        compare(Strings.fr[data.tag], data.fr)
    }

    function test_clock_format_data() {
        return [
            { tag: "en_US", locale: "en_US", format: "h:mm Ap", text: "9:05 PM" },
            { tag: "en_GB", locale: "en_GB", format: "HH:mm", text: "21:05" },
            { tag: "fr_FR", locale: "fr_FR", format: "HH:mm", text: "21:05" },
            { tag: "fr_CA", locale: "fr_CA", format: "HH 'h' mm", text: "21 h 05" },
            { tag: "C", locale: "C", format: "HH:mm", text: "21:05" },
            // the taskbar writes "PM"; the lock screen has to read the same
            { tag: "en_CA", locale: "en_CA", format: "h:mm Ap", text: "9:05 PM" },
            { tag: "es_MX", locale: "es_MX", format: "h:mm Ap", text: "9:05 PM" },
            // the marker is not always last, so it cannot be appended:
            // Korean puts it in front of the digits
            { tag: "ko_KR", locale: "ko_KR", format: "Ap h:mm", text: "\uc624\ud6c4 9:05" },
            { tag: "kn_IN", locale: "kn_IN", format: "hh:mm Ap", text: "09:05 PM" }
        ]
    }

    function test_clock_format(data) {
        var loc = Qt.locale(data.locale)
        compare(Strings.clockFormat(loc).replace(/[\s\u202f]+/g, " "), data.format)
        compare(Strings.timeText(new Date(2026, 9, 29, 21, 5, 30), loc), data.text)
    }

    // The clock used to drop the AM/PM text from the rendered time. In Qt
    // `h` counts 1 to 12 as soon as AP is in the format -- it is `H` that
    // always counts 0 to 23 -- so taking the marker off left 3 in the
    // morning and 3 in the afternoon reading exactly the same string.
    // Measured over the 189 regions Qt has here: 52 were ambiguous.
    function test_clock_marker_data() {
        return [
            { tag: "en_US", locale: "en_US" },
            { tag: "en_CA", locale: "en_CA" },
            { tag: "en_AU", locale: "en_AU" },
            { tag: "es_MX", locale: "es_MX" },
            { tag: "hi_IN", locale: "hi_IN" },
            { tag: "kn_IN", locale: "kn_IN" },
            { tag: "ko_KR", locale: "ko_KR" }
        ]
    }

    function test_clock_marker(data) {
        var loc = Qt.locale(data.locale)
        var morning = Strings.timeText(new Date(2026, 9, 29, 3, 5, 0), loc)
        var afternoon = Strings.timeText(new Date(2026, 9, 29, 15, 5, 0), loc)
        verify(morning !== afternoon,
               data.locale + ": 03:05 and 15:05 both read \"" + morning + "\"")
        // a Latin marker is written AM/PM whatever dots the region gives it
        var plain = t => t.replace(/[\s\u202f.]+/g, "").toLowerCase()
        var am = /^am$/.test(plain(loc.amText)) ? "am" : loc.amText.replace(/[\s\u202f]+/g, " ").toLowerCase()
        var pm = /^pm$/.test(plain(loc.pmText)) ? "pm" : loc.pmText.replace(/[\s\u202f]+/g, " ").toLowerCase()
        verify(morning.toLowerCase().indexOf(am) >= 0,
               data.locale + ": \"" + morning + "\" has no \"" + loc.amText + "\"")
        verify(afternoon.toLowerCase().indexOf(pm) >= 0,
               data.locale + ": \"" + afternoon + "\" has no \"" + loc.pmText + "\"")
    }

    // The other half of the rule: a 24-hour region gains nothing. The
    // marker belongs to the region's own short format; it is never added.
    function test_clock_without_marker_data() {
        return [
            { tag: "en_GB", locale: "en_GB" },
            { tag: "fr_FR", locale: "fr_FR" },
            { tag: "fr_CA", locale: "fr_CA" },
            { tag: "de_DE", locale: "de_DE" },
            { tag: "ja_JP", locale: "ja_JP" },
            { tag: "hu_HU", locale: "hu_HU" },
            { tag: "C", locale: "C" }
        ]
    }

    function test_clock_without_marker(data) {
        var loc = Qt.locale(data.locale)
        var morning = Strings.timeText(new Date(2026, 9, 29, 3, 5, 0), loc)
        var afternoon = Strings.timeText(new Date(2026, 9, 29, 15, 5, 0), loc)
        verify(morning !== afternoon, data.locale + ": both read \"" + morning + "\"")
        var both = (morning + " " + afternoon).toLowerCase()
        verify(both.indexOf(loc.amText.toLowerCase()) < 0,
               data.locale + ": \"" + morning + "\" gained \"" + loc.amText + "\"")
        verify(both.indexOf(loc.pmText.toLowerCase()) < 0,
               data.locale + ": \"" + afternoon + "\" gained \"" + loc.pmText + "\"")
    }

    // The net the defect went through: no region may read the same at 3 in
    // the morning and at 3 in the afternoon. A region Qt has no data for
    // is skipped -- so the number actually reached is asserted too, since
    // a sweep over an empty list asserts nothing and still passes.
    function test_no_region_is_ambiguous() {
        var names = ["en_US", "en_CA", "en_AU", "en_NZ", "en_IN", "en_PH", "en_SG",
                     "en_HK", "en_MY", "en_PK", "en_AE", "en_GB", "en_IE", "en_ZA",
                     "es_US", "es_MX", "es_AR", "es_ES", "ar_EG", "ar_SA", "hi_IN",
                     "bn_IN", "bn_BD", "ko_KR", "el_GR", "ms_MY", "am_ET", "ta_IN",
                     "te_IN", "ml_IN", "kn_IN", "mr_IN", "gu_IN", "pa_IN", "ur_PK",
                     "ne_IN", "km_KH", "sq_AL", "fil_PH", "so_SO", "fr_FR", "fr_CA",
                     "fr_BE", "fr_CH", "de_DE", "de_AT", "de_CH", "ja_JP", "zh_CN",
                     "zh_TW", "ru_RU", "pl_PL", "tr_TR", "sv_SE", "da_DK", "fi_FI",
                     "nb_NO", "cs_CZ", "hu_HU", "ro_RO", "th_TH", "vi_VN", "id_ID",
                     "he_IL", "uk_UA", "pt_BR", "pt_PT", "it_IT", "nl_NL", "C"]
        verify(names.length > 0, "the list of regions is empty")
        var morningAt = new Date(2026, 9, 29, 3, 5, 0)
        var afternoonAt = new Date(2026, 9, 29, 15, 5, 0)
        var reached = 0
        var ambiguous = []
        for (var i = 0; i < names.length; ++i) {
            var loc = Qt.locale(names[i])
            if (loc.name !== names[i])
                continue
            ++reached
            var morning = Strings.timeText(morningAt, loc)
            if (morning === Strings.timeText(afternoonAt, loc))
                ambiguous.push(names[i] + " (\"" + morning + "\")")
        }
        verify(reached >= 40, "only " + reached + " of " + names.length
                              + " regions exist here: the sweep proves nothing")
        compare(ambiguous.join(", "), "",
                "regions whose clock cannot tell the morning from the afternoon")
    }

    function test_date_format_data() {
        return [
            { tag: "en_US", locale: "en_US", format: "dddd, MMMM d", text: "Thursday, October 29" },
            { tag: "fr_FR", locale: "fr_FR", format: "dddd d MMMM", text: "jeudi 29 octobre" }
        ]
    }

    function test_date_format(data) {
        var loc = Qt.locale(data.locale)
        compare(Strings.dateFormat(loc), data.format)
        compare(Strings.dateText(new Date(2026, 9, 29, 21, 5), loc), data.text)
    }
}
