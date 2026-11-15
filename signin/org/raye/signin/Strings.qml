// Words of the sign-in screens, in English and French.
//
// No gettext: the SDDM greeter, the lock screen, the logout screen, the
// splash and the admin prompt all load this module, and some of them
// have no translation catalogue. The language follows the locale of the
// process (SDDM: /etc/default/locale; the others: the user's), or `lang`
// when an adapter or the gallery sets it.
//
// Use: Strings.tr.password, Strings.tr.verifiedPublisher.arg(name).
pragma Singleton

import QtQuick

QtObject {
    id: strings

    // "en" or "fr"; empty follows the locale
    property string lang: ""

    readonly property string language: lang !== "" ? lang
                                     : (Qt.locale().name.indexOf("fr") === 0 ? "fr" : "en")

    // Date and time formats follow the region when the language is the
    // locale's own; a forced language uses its main region.
    readonly property var locale: {
        var own = Qt.locale()
        if (own.name.indexOf(language) === 0)
            return own
        return Qt.locale(language === "fr" ? "fr_FR" : "en_US")
    }

    readonly property var tr: language === "fr" ? fr : en

    // The clock of the lock view: the region's short time, without the
    // seconds and without a time zone, and for the rest exactly as the
    // region writes it -- its AM/PM marker included, where the region puts
    // it (Korean puts it in front of the digits, so it cannot simply be
    // appended).
    //
    // The marker used to be taken off the rendered time. It cannot be: in
    // Qt, `h` and `hh` count 1 to 12 as soon as AP is in the format, and
    // only `H`/`HH` always count 0 to 23. Without its marker a 12-hour
    // region therefore read the same at 3 in the morning and at 3 in the
    // afternoon -- measured on 52 of the 189 regions Qt carries here,
    // en_US and en_CA among them. A 24-hour region has no AP in its short
    // format, so it gains no marker: nothing is added below, the region's
    // own format decides. tst_strings sweeps the regions for this.
    function clockFormat(loc) {
        var f = loc.timeFormat(Locale.ShortFormat)
        f = f.replace(/[:.]ss/g, "")
        f = f.replace(/\s*t+\s*/g, " ")
        return f.trim()
    }

    function timeText(date, loc) {
        loc = loc || locale
        // Some regions separate the time from its marker with a narrow
        // no-break space; the display font of the clock wants a plain one.
        var text = loc.toString(date, clockFormat(loc)).replace(/[\s\u202f]+/g, " ").trim()
        // The taskbar always writes AM and PM. Some regions spell the same
        // marker "p.m." or "p. m.": read one way on the bar and another on
        // the lock screen. Other scripts keep their own word.
        return text.replace(/(^|\s)([ap])\.?\s?m\.?(?=\s|$)/i,
                            (all, before, letter) => before + letter.toUpperCase() + "M")
    }

    // "Wednesday, October 29" / "mercredi 29 octobre": the region's long
    // date without the year.
    function dateFormat(loc) {
        var f = loc.dateFormat(Locale.LongFormat)
        f = f.replace(/[\s,.\/-]*y+[.]?/g, "")
        f = f.replace(/^[\s,]+|[\s,]+$/g, "")
        return f
    }

    function dateText(date, loc) {
        loc = loc || locale
        return loc.toString(date, dateFormat(loc))
    }

    readonly property var en: ({
        password: "Password",
        signIn: "Sign in",
        showPassword: "Show password",
        hidePassword: "Hide password",
        capsLock: "Caps Lock is on",
        wrongPassword: "The password is incorrect. Try again.",
        ok: "OK",
        welcome: "Welcome",
        signingOut: "Signing out",
        shuttingDown: "Shutting down",
        restarting: "Restarting",
        workingOnUpdates: "Working on updates",
        percent: "%1%",
        dontTurnOff: "Don't turn off your PC",
        hi: "Hi",
        gettingReady: "We're getting things ready",
        mightTakeMinute: "This might take a minute",
        pressKey: "Press a key or click to sign in",
        power: "Power",
        sleep: "Sleep",
        hibernate: "Hibernate",
        shutDown: "Shut down",
        restart: "Restart",
        updateAndShutDown: "Update and shut down",
        updateAndRestart: "Update and restart",
        accessibility: "Accessibility",
        onScreenKeyboard: "On-screen keyboard",
        largerText: "Larger text",
        highContrast: "High contrast",
        network: "Network",
        networkSignIn: "Sign in to connect to a network",
        connected: "Connected",
        notConnected: "Not connected",
        keyboardLayout: "Keyboard layout",
        signInOptions: "Sign-in options",
        switchUser: "Switch user",
        otherUsers: "Other users",
        fingerprint: "Scan your finger on the fingerprint reader",
        lock: "Lock",
        signOut: "Sign out",
        changePassword: "Change a password",
        changePasswordElsewhere: "A password is changed in Settings, not from this screen.",
        taskManager: "Task Manager",
        taskManagerMissing: "Task Manager is not installed.",
        cancel: "Cancel",
        emergencyRestart: "Emergency restart",
        emergencyWarning: "Click OK to restart right away. Any unsaved data will be lost.",
        rootAccess: "Root access needed",
        allowChanges: "Do you want to allow this app to make changes to your device?",
        verifiedPublisher: "Verified publisher: %1",
        fileOrigin: "File origin: This computer",
        unknownPublisher: "Publisher: Unknown",
        showDetails: "Show more details",
        hideDetails: "Hide details",
        enterOwnPassword: "Enter your password to continue as root.",
        enterSudoPassword: "To continue as root, enter the password of a sudo user.",
        moreChoices: "More choices",
        yes: "Yes",
        no: "No",
        pleaseWait: "Please wait"
    })

    readonly property var fr: ({
        password: "Mot de passe",
        signIn: "Se connecter",
        showPassword: "Afficher le mot de passe",
        hidePassword: "Masquer le mot de passe",
        capsLock: "Verr. Maj. activé",
        wrongPassword: "Le mot de passe est incorrect. Réessayez.",
        ok: "OK",
        welcome: "Bienvenue",
        signingOut: "Déconnexion",
        shuttingDown: "Arrêt en cours",
        restarting: "Redémarrage",
        workingOnUpdates: "Mises à jour en cours",
        percent: "%1 %",
        dontTurnOff: "N'éteignez pas votre PC",
        hi: "Bonjour",
        gettingReady: "Nous préparons tout pour vous",
        mightTakeMinute: "Cela peut prendre une minute",
        pressKey: "Appuyez sur une touche ou cliquez pour vous connecter",
        power: "Alimentation",
        sleep: "Mettre en veille",
        hibernate: "Mettre en veille prolongée",
        shutDown: "Arrêter",
        restart: "Redémarrer",
        updateAndShutDown: "Mettre à jour et arrêter",
        updateAndRestart: "Mettre à jour et redémarrer",
        accessibility: "Accessibilité",
        onScreenKeyboard: "Clavier visuel",
        largerText: "Texte plus grand",
        highContrast: "Contraste élevé",
        network: "Réseau",
        networkSignIn: "Connectez-vous pour rejoindre un réseau",
        connected: "Connecté",
        notConnected: "Non connecté",
        keyboardLayout: "Disposition du clavier",
        signInOptions: "Options de connexion",
        switchUser: "Changer d'utilisateur",
        otherUsers: "Autres utilisateurs",
        fingerprint: "Posez votre doigt sur le lecteur d'empreintes",
        lock: "Verrouiller",
        signOut: "Se déconnecter",
        changePassword: "Modifier un mot de passe",
        changePasswordElsewhere: "Un mot de passe se modifie dans les Paramètres, pas depuis cet écran.",
        taskManager: "Gestionnaire des tâches",
        taskManagerMissing: "Le Gestionnaire des tâches n'est pas installé.",
        cancel: "Annuler",
        emergencyRestart: "Redémarrage d'urgence",
        emergencyWarning: "Cliquez sur OK pour redémarrer immédiatement. Toutes les données non enregistrées seront perdues.",
        rootAccess: "Accès root requis",
        allowChanges: "Voulez-vous autoriser cette application à apporter des modifications à votre appareil ?",
        verifiedPublisher: "Éditeur vérifié : %1",
        fileOrigin: "Origine du fichier : cet ordinateur",
        unknownPublisher: "Éditeur : inconnu",
        showDetails: "Afficher plus de détails",
        hideDetails: "Masquer les détails",
        enterOwnPassword: "Entrez votre mot de passe pour continuer en tant que root.",
        enterSudoPassword: "Pour continuer en tant que root, entrez le mot de passe d'un utilisateur sudo.",
        moreChoices: "Plus de choix",
        yes: "Oui",
        no: "Non",
        pleaseWait: "Veuillez patienter"
    })
}
