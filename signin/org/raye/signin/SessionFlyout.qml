// Desktop flyout: the Raye desktops an account can sign in to, the one
// in use checked. Only shown when there is more than one.
//
// In:  choices  [{ key }] ("plasma", "hyprland"), current  the key in use
// Out: sessionPicked(key)
import QtQuick

Flyout {
    id: sessions

    property var choices: []
    property string current: ""

    signal sessionPicked(string key)

    // a pick sends the keys back to the password field (the owner does it)
    focusBack: false

    function label(key) {
        return key === "hyprland" ? Strings.tr.sessionHyprland : Strings.tr.sessionPlasma
    }

    rows: {
        var r = []
        for (var i = 0; i < (choices ? choices.length : 0); ++i)
            r.push({ id: choices[i].key, text: label(choices[i].key),
                     icon: choices[i].key === current ? "check" : "", selected: choices[i].key === current })
        return r
    }

    onTriggered: id => sessionPicked(id)
}
