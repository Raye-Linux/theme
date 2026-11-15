import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

PageBase {
    id: page
    heading: "Menus and tool bars"

    function openPopups() {
        menu.popup(anchor, 0, anchor.height)
    }

    function closePopups() {
        menu.close()
    }

    SampleRow {
        caption: "Menu bar"
        MenuBar {
            Menu { title: "&File"; MenuItem { text: "Item" } }
            Menu { title: "&Edit"; MenuItem { text: "Item" } }
            Menu { title: "&View"; MenuItem { text: "Item" } }
            Menu { title: "&Help"; MenuItem { text: "Item" } }
        }
    }

    SampleRow {
        caption: "Tool bar"
        ToolBar {
            RowLayout {
                ToolButton { icon.name: "document-new"; text: "New" }
                ToolButton { icon.name: "document-open"; text: "Open" }
                ToolSeparator {}
                ToolButton { icon.name: "format-text-bold"; text: "Bold"; checkable: true; checked: true }
                ToolButton { icon.name: "edit-delete"; text: "Delete"; enabled: false }
            }
        }
    }

    SampleRow {
        caption: "Menu (opened)"
        Button {
            id: anchor
            objectName: "focusTarget"
            text: "Menu"
            onClicked: page.openPopups()
        }
    }

    Menu {
        id: menu
        Action { text: "Cut"; icon.name: "edit-cut"; shortcut: "Ctrl+X" }
        MenuItem {
            objectName: "hoverTarget"
            action: Action { text: "Copy"; icon.name: "edit-copy"; shortcut: "Ctrl+C" }
        }
        Action { text: "Paste"; icon.name: "edit-paste"; shortcut: "Ctrl+V"; enabled: false }
        MenuSeparator {}
        Action { text: "Word wrap"; checkable: true; checked: true }
        Menu {
            title: "Sort by"
            Action { text: "Name" }
            Action { text: "Date" }
        }
        MenuSeparator {}
        Action { text: "Properties"; icon.name: "document-properties" }
    }
}
