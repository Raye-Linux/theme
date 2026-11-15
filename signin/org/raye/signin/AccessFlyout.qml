// Accessibility flyout: On-screen keyboard, Larger text, High contrast,
// three switches. Larger text and High contrast act on the whole module
// (RayePalette); the keyboard is the owner's (keyboardOn, keyboardToggled).
import QtQuick

Flyout {
    id: access

    property bool keyboardOn: false
    // false when QtQuick.VirtualKeyboard is not there
    property bool keyboardAvailable: true

    signal keyboardToggled(bool on)

    rows: {
        var r = []
        if (keyboardAvailable)
            r.push({ id: "keyboard", text: Strings.tr.onScreenKeyboard, icon: "keyboard",
                     checkable: true, checked: keyboardOn })
        r.push({ id: "largerText", text: Strings.tr.largerText, icon: "text-larger",
                 checkable: true, checked: RayePalette.largerText })
        r.push({ id: "highContrast", text: Strings.tr.highContrast, icon: "contrast",
                 checkable: true, checked: RayePalette.highContrast })
        return r
    }

    onTriggered: id => {
        if (id === "keyboard")
            keyboardToggled(!keyboardOn)
        else if (id === "largerText")
            RayePalette.largerText = !RayePalette.largerText
        else if (id === "highContrast")
            RayePalette.highContrast = !RayePalette.highContrast
    }
}
