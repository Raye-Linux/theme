// The notification centre (Win+N): the notifications card over the
// calendar card, 364 wide, 12 px apart, both on the Raye acrylic with a
// soft shadow. It slides up and fades in as it opens. `margin` is the
// room left around the cards for their shadow, inside the window.
import QtQuick
import org.raye.ui
import org.raye.shell

import "clocklogic.js" as Logic

FocusScope {
    id: centre

    property var model
    property var controller
    property date now: new Date()
    property bool doNotDisturb: false
    property bool calendarCollapsed: false
    property int focusRemaining: 0
    property int focusMinutes: 30
    // Raye Calendar: installed or not, and the events around the shown month
    property bool calendarAvailable: false
    property var calendarEvents: []
    // the tallest the centre may be (cards and gap, without the margin)
    property int maxHeight: 900
    property bool shown: true
    // the calendar card animates its height; off while preparing an opening
    property bool animated: true

    readonly property int margin: 12
    readonly property int gap: 12
    readonly property Item notificationsCard: notificationsCard
    readonly property Item calendarCard: calendarCard
    // what of the calendar the room leaves: the card is folded rather than cut
    readonly property var fit: Logic.calendarFit(maxHeight, {
        header: calendarCard.headerHeight, month: calendarCard.monthHeight, agenda: calendarCard.agendaHeight,
        focus: calendarCard.focusHeight, gap: gap, notificationsMin: notificationsCard.minimumHeight,
        notificationsWanted: notificationsCard.naturalHeight })

    // Lays the centre out for `room` at once, with no animation left running,
    // so its height is final before its window shows. Returns that height.
    function prepare(room) {
        animated = false
        maxHeight = room
        notificationsCard.layoutNow()
        notificationsCard.layoutNow()
        return height
    }

    signal requestClose()
    signal collapseToggled()
    // the closing slide is over
    signal closed()

    width: 364 + 2 * margin
    height: notificationsCard.height + gap + calendarCard.height + 2 * margin
    focus: true
    Keys.onEscapePressed: centre.requestClose()

    component Shadow: Item {
        id: shadowItem
        property Item target
        x: target.x
        y: target.y
        width: target.width
        height: target.height
        Repeater {
            model: 4
            Rectangle {
                required property int index
                anchors.fill: parent
                anchors.margins: -(index + 1) * 3
                anchors.topMargin: -(index + 1) * 3 + 6
                radius: ShellTokens.radiusFlyout + (index + 1) * 3
                color: Qt.rgba(0, 0, 0, Tokens.night ? 0.07 : 0.03)
            }
        }
    }

    Item {
        id: content
        anchors.fill: parent
        anchors.margins: centre.margin
        opacity: centre.shown ? 1 : 0
        Behavior on opacity {
            SequentialAnimation {
                // a straight fade, under the slide
                NumberAnimation { duration: ShellTokens.durationFast }
                ScriptAction { script: if (!centre.shown) centre.closed() }
            }
        }
        transform: Translate {
            y: centre.shown ? 0 : ShellTokens.flyoutSlide
            Behavior on y {
                NumberAnimation {
                    duration: centre.shown ? ShellTokens.durationNormal : ShellTokens.durationFast
                    easing: centre.shown ? Motion.entrance : Motion.exit
                }
            }
        }

        Shadow { target: notificationsCard }
        Shadow { target: calendarCard }

        NotificationsCard {
            id: notificationsCard
            width: parent.width
            height: implicitHeight
            model: centre.model
            controller: centre.controller
            now: centre.now
            doNotDisturb: centre.doNotDisturb
            maxHeight: Math.max(minimumHeight, centre.maxHeight - centre.gap - calendarCard.targetHeight)
        }

        CalendarCard {
            id: calendarCard
            anchors.top: notificationsCard.bottom
            anchors.topMargin: centre.gap
            width: parent.width
            height: implicitHeight
            controller: centre.controller
            now: centre.now
            collapsed: centre.calendarCollapsed
            animate: centre.animated
            roomForMonth: centre.fit.month
            roomForAgenda: centre.fit.agenda
            focusRemaining: centre.focusRemaining
            focusMinutes: centre.focusMinutes
            calendarAvailable: centre.calendarAvailable
            events: centre.calendarEvents
            onCollapseToggled: centre.collapseToggled()
            onFocusMinutesChanged: centre.focusMinutes = focusMinutes
        }
    }
}
