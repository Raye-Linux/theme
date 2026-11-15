// Raye's own QML pieces (org.raye.ui), when the module is there: each
// one with its defaults, then every colour token. A piece the module
// does not have yet is listed as missing instead of failing the page.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

PageBase {
    id: page
    heading: "Raye QML pieces"

    readonly property var pieces: ["AccentButton", "Switch", "Card", "SettingsCard", "Pill", "InfoBar", "FocusRing", "Acrylic"]
    property var missing: []
    property var tokenColours: []
    property string moduleState: ""
    // the page width less the caption column, the margins and a scroll bar
    readonly property real flowWidth: Math.max(320, width - 168 - 16 - 48 - 28)

    function tryCreate(source, parentItem, name) {
        try {
            return Qt.createQmlObject(source, parentItem, "raye-gallery-" + name)
        } catch (e) {
            return null
        }
    }

    Component.onCompleted: {
        const tokensHolder = tryCreate("import org.raye.ui\nimport QtQuick\nQtObject { readonly property var tokens: Tokens }", page, "tokens")
        if (!tokensHolder || !tokensHolder.tokens) {
            moduleState = "org.raye.ui cannot be imported here: no qmldir yet, or the module is not installed."
            missing = pieces
            return
        }
        moduleState = "org.raye.ui loaded."
        const absent = []
        let first = true
        for (const name of pieces) {
            // each piece sits under its name in a holder of fixed width, so a
            // piece that misbehaves on its own sizes cannot move the page
            const holder = tryCreate("import QtQuick\nimport QtQuick.Controls\nColumn { width: 360; spacing: 4\n"
                                     + "Label { text: \"" + name + "\"; font.pixelSize: 11; color: palette.placeholderText } }",
                                     flow, "holder-" + name)
            const piece = holder ? tryCreate("import org.raye.ui\n" + name + " {}", holder, name) : null
            if (!piece) {
                if (holder) holder.destroy()
                absent.push(name)
                continue
            }
            // the capture points at the first piece and gives it the focus
            if (first) {
                first = false
                piece.objectName = "hoverTarget"
                if ("activeFocusOnTab" in piece) piece.activeFocusOnTab = true
            }
            if ("text" in piece) piece.text = name
            if ("title" in piece) piece.title = name
            if ("description" in piece) piece.description = "Default look of " + name
            piece.width = Qt.binding(() => piece.implicitWidth > 0 ? Math.min(piece.implicitWidth, holder.width) : holder.width)
            if (piece.implicitHeight === 0) piece.height = 48
        }
        missing = absent
        const colours = []
        const t = tokensHolder.tokens
        for (const key in t) {
            const v = t[key]
            if (v !== null && typeof v === "object" && v.r !== undefined && v.a !== undefined)
                colours.push({ name: key, colour: v })
        }
        colours.sort((a, b) => a.name.localeCompare(b.name))
        tokenColours = colours
    }

    SampleRow {
        caption: "Module"
        Label { text: page.moduleState }
    }

    SampleRow {
        caption: "Pieces"
        Flow {
            id: flow
            // a Flow needs its width to know its height
            Layout.preferredWidth: page.flowWidth
            spacing: 12
        }
    }

    SampleRow {
        caption: "Missing"
        visible: page.missing.length > 0
        Label { text: page.missing.join(", ") }
    }

    SampleRow {
        caption: "Colour tokens"
        visible: page.tokenColours.length > 0
        Flow {
            Layout.preferredWidth: page.flowWidth
            spacing: 8
            Repeater {
                model: page.tokenColours
                delegate: Column {
                    required property var modelData
                    width: 112
                    spacing: 2
                    Rectangle {
                        width: 104
                        height: 28
                        radius: 4
                        color: modelData.colour
                        border.width: 1
                        border.color: palette.mid
                    }
                    Label { text: modelData.name; font.pixelSize: 11; elide: Text.ElideRight; width: 104 }
                    Label { text: modelData.colour.toString(); font.pixelSize: 11; color: palette.placeholderText }
                }
            }
        }
    }
}
