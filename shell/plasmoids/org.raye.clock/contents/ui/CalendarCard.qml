// The lower card of the notification centre: today's long date and a
// chevron that folds the month away; the month with its arrows, the
// weekday names and the 6 x 7 days (today in an accent circle, the days
// of the months around it dimmed); at the bottom the Focus
// strip, which turns Do not disturb on for a while and shows what is left.
// With Raye Calendar installed, a dot marks the days that hold something
// and the chosen day's events show under the month, with "Add an event".
import QtQuick
import QtQuick.Templates as T
import org.raye.ui
import org.raye.shell

import "clocklogic.js" as Logic

Item {
    id: card

    property var controller
    property date now: new Date()
    property bool collapsed: false
    // what the screen leaves room for (Centre.qml decides)
    property bool roomForMonth: true
    property bool roomForAgenda: true
    readonly property bool folded: collapsed || !roomForMonth
    // the parts' heights, for Centre.qml to work out what fits
    readonly property int headerHeight: header.height
    readonly property int monthHeight: month.height
    readonly property int agendaHeight: calendarAvailable ? agenda.height : 0
    readonly property int focusHeight: focusStrip.height
    // seconds left of a Focus session, 0 when none runs
    property int focusRemaining: 0
    property int focusMinutes: Logic.focusDefault

    property int shownYear: now.getFullYear()
    property int shownMonth: now.getMonth()
    readonly property var cells: Logic.monthGrid(shownYear, shownMonth, Qt.locale().firstDayOfWeek)

    // Raye Calendar: there or not, and the events of the days around the shown month
    property bool calendarAvailable: false
    property var events: []
    property date selected: now
    readonly property var dayEvents: Logic.eventsOn(events, selected.getFullYear(), selected.getMonth(), selected.getDate())
    readonly property string selectedIso: Logic.isoDay(selected.getFullYear(), selected.getMonth(), selected.getDate())
    // three rows, always: the card keeps its height when the events
    // arrive after it opened, so the window never has to grow under it
    readonly property int rows: 3
    readonly property int shownEvents: dayEvents.length > rows ? rows - 1 : rows

    signal collapseToggled()

    // the 42 days of any month page, whatever the first day of the week
    function wantRange() {
        if (!calendarAvailable || !controller || !controller.requestAgenda)
            return
        const from = new Date(shownYear, shownMonth, 1 - 6)
        const to = new Date(shownYear, shownMonth + 1, 13)
        controller.requestAgenda(Logic.isoDay(from.getFullYear(), from.getMonth(), from.getDate()),
                                 Logic.isoDay(to.getFullYear(), to.getMonth(), to.getDate()))
    }
    onShownMonthChanged: Qt.callLater(wantRange)
    onShownYearChanged: Qt.callLater(wantRange)
    onControllerChanged: Qt.callLater(wantRange)
    onCalendarAvailableChanged: Qt.callLater(wantRange)
    Component.onCompleted: {
        heightBehavior.enabled = animate
        Qt.callLater(wantRange)
    }

    function showToday() {
        shownYear = now.getFullYear()
        shownMonth = now.getMonth()
    }
    function moveMonth(delta) {
        const m = Logic.addMonths(shownYear, shownMonth, delta)
        shownYear = m.year
        shownMonth = m.month
    }

    implicitWidth: 364
    // the height it is going to: implicitHeight animates towards it
    readonly property int targetHeight: header.height + (folded ? 0 : month.height + (agenda.visible ? agenda.height : 0)) + focusStrip.height
    implicitHeight: targetHeight
    clip: true
    // off while the centre is laid out for a new opening: it must be at its
    // final height before its window shows
    property bool animate: true
    // set here, not by a binding: the Behavior must be off before the height
    // is set anew, and a binding on `enabled` may only follow this handler.
    // A disabled Behavior also lets a running animation finish: stop it.
    onAnimateChanged: {
        heightBehavior.enabled = animate
        if (!animate) {
            heightAnimation.stop()
            implicitHeight = Qt.binding(() => card.targetHeight)
        }
    }
    Behavior on implicitHeight {
        id: heightBehavior
        NumberAnimation { id: heightAnimation; duration: ShellTokens.durationNormal; easing: Motion.pointToPoint }
    }

    AcrylicSurface {
        anchors.fill: parent
        focused: card.activeFocus
    }

    // --- header ----------------------------------------------------------
    Item {
        id: header
        width: parent.width
        height: 52

        Text {
            objectName: "longDate"
            anchors { left: parent.left; leftMargin: 16; right: chevron.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
            // "Sunday, November 1" in the language of the session, without the year
            text: {
                const f = Qt.locale().dateFormat(Locale.LongFormat).replace(/[,.\s]*y+[,.\s]*/g, " ").trim()
                return card.now.toLocaleDateString(Qt.locale(), f)
            }
            elide: Text.ElideRight
            color: ShellTokens.text
            font: ShellTokens.bodyStrong
        }
        FlyoutButton {
            id: chevron
            objectName: "collapse"
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            // no room for the month: nothing to unfold
            visible: card.roomForMonth
            icon.name: card.collapsed ? "go-up-symbolic" : "go-down-symbolic"
            toolTipText: card.collapsed ? i18nd("raye_shell_clock", "Show the calendar") : i18nd("raye_shell_clock", "Hide the calendar")
            onClicked: card.collapseToggled()
        }
    }

    // --- the month -----------------------------------------------------------
    Item {
        id: month
        anchors.top: header.bottom
        width: parent.width
        height: 36 + 28 + 6 * 40 + 8
        opacity: card.folded ? 0 : 1
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: ShellTokens.durationFast } }

        Rectangle {
            width: parent.width
            height: 1
            color: ShellTokens.separator
        }

        Text {
            objectName: "monthTitle"
            anchors { left: parent.left; leftMargin: 16; top: parent.top; topMargin: 8 }
            height: 28
            verticalAlignment: Text.AlignVCenter
            text: new Date(card.shownYear, card.shownMonth, 1).toLocaleDateString(Qt.locale(), "MMMM yyyy")
            color: ShellTokens.text
            font: ShellTokens.bodyStrong
            MouseArea {
                anchors.fill: parent
                onClicked: card.showToday()
            }
        }
        Row {
            anchors { right: parent.right; rightMargin: 12; top: parent.top; topMargin: 6 }
            FlyoutButton {
                objectName: "previousMonth"
                icon.name: "go-up-symbolic"
                toolTipText: i18nd("raye_shell_clock", "Previous month")
                onClicked: card.moveMonth(-1)
            }
            FlyoutButton {
                objectName: "nextMonth"
                icon.name: "go-down-symbolic"
                toolTipText: i18nd("raye_shell_clock", "Next month")
                onClicked: card.moveMonth(1)
            }
        }

        Grid {
            id: grid
            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 40 }
            columns: 7
            columnSpacing: 4

            Repeater {
                model: Logic.weekdays(Qt.locale().firstDayOfWeek)
                Text {
                    required property var modelData
                    width: 40
                    height: 28
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    // Qt's dayName counts Monday as 1 and Sunday as 0 or 7
                    text: Qt.locale().dayName(modelData, Locale.ShortFormat).substring(0, 2)
                    color: ShellTokens.text
                    font: ShellTokens.caption
                }
            }

            Repeater {
                model: card.cells
                Item {
                    id: cell
                    required property var modelData
                    readonly property bool today: modelData.year === card.now.getFullYear()
                                                  && modelData.month === card.now.getMonth()
                                                  && modelData.day === card.now.getDate()
                    readonly property string iso: Logic.isoDay(modelData.year, modelData.month, modelData.day)
                    readonly property bool chosen: card.calendarAvailable && iso === card.selectedIso
                    readonly property bool busy: card.calendarAvailable
                        && Logic.eventsOn(card.events, modelData.year, modelData.month, modelData.day).some(e => !e.holiday)
                    objectName: "day-" + iso
                    width: 40
                    height: 40

                    Rectangle {
                        anchors.centerIn: parent
                        width: 32
                        height: 32
                        radius: 16
                        color: cell.today ? ShellTokens.accent : dayHover.hovered ? ShellTokens.hoverFill : "transparent"
                        border.width: cell.chosen && !cell.today ? 1.5 : 0
                        border.color: ShellTokens.accent
                    }
                    HoverHandler { id: dayHover }
                    TapHandler {
                        enabled: card.calendarAvailable
                        onTapped: card.selected = new Date(cell.modelData.year, cell.modelData.month, cell.modelData.day)
                    }
                    Repeater {
                        model: cell.busy ? 1 : 0
                        Rectangle {
                            objectName: "dot-" + cell.iso
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 6
                            width: 4
                            height: 4
                            radius: 2
                            color: cell.today ? ShellTokens.textOnAccent : Tokens.accentText
                        }
                    }
                    Text {
                        objectName: cell.today ? "today" : ""
                        anchors.centerIn: parent
                        text: cell.modelData.day
                        color: cell.today ? ShellTokens.textOnAccent
                             : cell.modelData.inMonth ? ShellTokens.text : Tokens.textDisabled
                        font.family: "Inter"
                        font.pixelSize: 13
                        font.weight: cell.today ? Font.DemiBold : Font.Normal
                        font.features: { "tnum": 1 }
                    }
                }
            }
        }
    }

    // --- the chosen day's events (Raye Calendar) ---------------------------
    Item {
        id: agenda
        objectName: "agenda"
        anchors.top: month.bottom
        width: parent.width
        visible: card.calendarAvailable && !card.folded && card.roomForAgenda
        readonly property int more: Math.max(0, card.dayEvents.length - card.shownEvents)
        height: 48 + card.rows * 32 + 8

        Rectangle {
            width: parent.width
            height: 1
            color: ShellTokens.separator
        }
        Text {
            objectName: "agendaDay"
            anchors { left: parent.left; leftMargin: 16; right: addEvent.left; rightMargin: 8 }
            y: 8
            height: 32
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            text: {
                const d = card.selected, t = card.now
                if (Logic.sameDay(d, t))
                    return i18nd("raye_shell_clock", "Today")
                if (Logic.sameDay(d, new Date(t.getFullYear(), t.getMonth(), t.getDate() + 1)))
                    return i18nd("raye_shell_clock", "Tomorrow")
                const f = Qt.locale().dateFormat(Locale.LongFormat).replace(/[,.\s]*y+[,.\s]*/g, " ").trim()
                return d.toLocaleDateString(Qt.locale(), f)
            }
            color: ShellTokens.text
            font: ShellTokens.bodyStrong
        }
        FlyoutButton {
            id: addEvent
            objectName: "addEvent"
            anchors { right: parent.right; rightMargin: 12 }
            y: 8
            text: "+"
            toolTipText: i18nd("raye_shell_clock", "Add an event")
            onClicked: card.controller.openCalendar(card.selectedIso, true)
        }

        Text {
            objectName: "noEvents"
            visible: card.dayEvents.length === 0
            x: 16
            y: 48
            height: 32
            verticalAlignment: Text.AlignVCenter
            text: i18nd("raye_shell_clock", "No events")
            color: Tokens.textSecondary
            font: ShellTokens.caption
        }

        Column {
            x: 8
            y: 48
            width: parent.width - 16
            Repeater {
                model: card.dayEvents.slice(0, card.shownEvents)
                T.AbstractButton {
                    id: row
                    required property var modelData
                    width: parent.width
                    height: 32
                    hoverEnabled: true
                    Accessible.name: modelData.title
                    onClicked: card.controller.openCalendar(card.selectedIso, false)
                    background: Rectangle {
                        radius: ShellTokens.radiusButton
                        color: row.hovered ? ShellTokens.hoverFill : "transparent"
                    }
                    contentItem: Item {
                        Rectangle {
                            x: 8
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3
                            height: 18
                            radius: 1.5
                            color: row.modelData.holiday ? Tokens.textSecondary
                                 : row.modelData.color !== "" ? row.modelData.color : ShellTokens.accent
                        }
                        Text {
                            id: when
                            x: 20
                            // "Toute la journée" fits
                            width: 100
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                            text: row.modelData.allDay ? i18nd("raye_shell_clock", "All day")
                                : Qt.formatTime(row.modelData.start, Qt.locale().timeFormat(Locale.ShortFormat).replace(/[:.]ss/, ""))
                            color: Tokens.textSecondary
                            font: ShellTokens.caption
                        }
                        Text {
                            objectName: "agendaTitle"
                            anchors { left: when.right; leftMargin: 8; right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                            elide: Text.ElideRight
                            text: row.modelData.title
                            color: ShellTokens.text
                            font: ShellTokens.body
                        }
                    }
                }
            }
            T.AbstractButton {
                objectName: "moreEvents"
                visible: agenda.more > 0
                width: parent.width
                height: 32
                hoverEnabled: true
                onClicked: card.controller.openCalendar(card.selectedIso, false)
                contentItem: Text {
                    leftPadding: 20
                    verticalAlignment: Text.AlignVCenter
                    text: i18ndp("raye_shell_clock", "%1 more", "%1 more", agenda.more)
                    color: parent.hovered ? Tokens.accentText : Tokens.textSecondary
                    font: ShellTokens.caption
                }
                background: null
            }
        }
    }

    // --- Focus -------------------------------------------------------------
    Item {
        id: focusStrip
        objectName: "focusStrip"
        anchors.bottom: parent.bottom
        width: parent.width
        height: 56

        readonly property bool running: card.focusRemaining > 0

        Rectangle {
            width: parent.width
            height: 1
            color: ShellTokens.separator
        }

        Row {
            visible: !focusStrip.running
            anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
            spacing: 4
            FlyoutButton {
                objectName: "focusLess"
                text: "\u2212"
                toolTipText: i18nd("raye_shell_clock", "Less time")
                enabled: card.focusMinutes > Logic.focusMin
                onClicked: card.focusMinutes = Logic.focusStep(card.focusMinutes, false)
            }
            Text {
                objectName: "focusLength"
                width: 76
                height: 32
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: i18ndp("raye_shell_clock", "%1 min", "%1 mins", card.focusMinutes)
                color: ShellTokens.text
                font.family: "Inter"
                font.pixelSize: 14
                font.features: { "tnum": 1 }
            }
            FlyoutButton {
                objectName: "focusMore"
                text: "+"
                toolTipText: i18nd("raye_shell_clock", "More time")
                enabled: card.focusMinutes < Logic.focusMax
                onClicked: card.focusMinutes = Logic.focusStep(card.focusMinutes, true)
            }
        }
        Text {
            visible: focusStrip.running
            anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
            text: i18ndc("raye_shell_clock", "time left of a focus session", "Focus, %1 left", Logic.focusText(card.focusRemaining))
            color: ShellTokens.text
            font.family: "Inter"
            font.pixelSize: 14
            font.features: { "tnum": 1 }
        }
        FlyoutButton {
            objectName: "focusButton"
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            accent: !focusStrip.running
            text: focusStrip.running ? i18nd("raye_shell_clock", "End session") : i18nd("raye_shell_clock", "Focus")
            onClicked: focusStrip.running ? card.controller.stopFocus() : card.controller.startFocus(card.focusMinutes)
        }
    }
}
