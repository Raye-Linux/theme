// The clock's decisions, as plain functions so they can be tested without
// Plasma: 12 or 24 hours, the time format, the unread count on the bell,
// how old a notification is, the month grid of the calendar and the
// Focus session (its steps, what is left of it).
.pragma library

// --- the time on the taskbar -----------------------------------------------

// "auto" follows the language's own clock (localeFormat: the short time
// format of the locale, "h:mm AP" or "HH:mm"); "true"/"false" force it.
function use24h(setting, localeFormat) {
    if (setting === "true" || setting === true)
        return true
    if (setting === "false" || setting === false)
        return false
    return !/a/i.test(String(localeFormat || "").replace(/'[^']*'/g, ""))
}

// A format for Qt.formatTime().
function timeFormat(twentyFour, seconds) {
    if (twentyFour)
        return seconds ? "HH:mm:ss" : "HH:mm"
    return seconds ? "h:mm:ss AP" : "h:mm AP"
}

// The clock's text as Windows writes it: "9:24 PM" with the plain AM/PM
// whatever the locale's own words ("p.m."), or "21:24".
function timeText(hours, minutes, seconds, twentyFour, showSeconds) {
    function two(n) { return (n < 10 ? "0" : "") + n }
    var tail = showSeconds ? ":" + two(seconds) : ""
    if (twentyFour)
        return two(hours) + ":" + two(minutes) + tail
    var h = hours % 12
    return (h === 0 ? 12 : h) + ":" + two(minutes) + tail + (hours < 12 ? " AM" : " PM")
}

// --- the two lines on the taskbar ------------------------------------------

// The room between the time and the date. They used to sit flush: the
// Column set no spacing at all, and one line of Inter at 12 px measures
// 15 px, so the stack was exactly 30 px with nothing between the two
// (measured 2026-11-06). "The date is too close to the time" was that
// zero, and the fix is this number -- named, so a type change can be
// weighed against it instead of silently undoing it.
var LINE_GAP = 3

// The gap to really use. The two lines and the gap have to fit inside
// the taskbar button, and a line is not 15 px for ever: a larger type
// size, or a script whose fallback font has taller metrics, makes it
// grow. Then the GAP gives way, never the button -- down to 0, never
// negative, so the clock cannot overflow the taskbar.
//
// `available` is the height the button really has; 0 or nothing
// measured yet (a Text reports 0 before its first layout) keeps the
// designed gap, since there is nothing to clamp against and collapsing
// to 0 at that moment would be the old defect, back for one frame.
function lineGap(timeHeight, dateHeight, available, designed) {
    var want = Math.round(Number(designed === undefined ? LINE_GAP : designed))
    if (!isFinite(want) || want < 0)
        want = LINE_GAP
    var lines = Number(timeHeight) + Number(dateHeight)
    var room = Number(available)
    if (!isFinite(lines) || lines <= 0 || !isFinite(room) || room <= 0)
        return want
    return Math.max(0, Math.min(want, Math.floor(room - lines)))
}

// How long until the clock must change: the next second, or the next minute.
function nextTick(now, seconds) {
    var ms = now.getMilliseconds()
    if (seconds)
        return 1000 - ms
    return (60 - now.getSeconds()) * 1000 - ms
}

// The count in the bell's circle.
function countText(count) {
    count = Math.floor(Number(count) || 0)
    if (count <= 0)
        return ""
    return count > 9 ? "9+" : String(count)
}

// --- notifications ---------------------------------------------------------

// How old a notification is, for its time label:
//   { kind: "now" }                   under a minute
//   { kind: "minutes", value: n }     under an hour ("12m")
//   { kind: "today" }                 earlier today (show the time)
//   { kind: "yesterday" }
//   { kind: "date" }                  older (show the date)
function age(created, now) {
    if (!(created instanceof Date) || isNaN(created.getTime()))
        return { kind: "now" }
    var diff = now.getTime() - created.getTime()
    if (diff < 60 * 1000)
        return { kind: "now" }
    if (diff < 60 * 60 * 1000)
        return { kind: "minutes", value: Math.floor(diff / 60000) }
    var startOfToday = new Date(now.getFullYear(), now.getMonth(), now.getDate())
    if (created >= startOfToday)
        return { kind: "today" }
    var startOfYesterday = new Date(now.getFullYear(), now.getMonth(), now.getDate() - 1)
    if (created >= startOfYesterday)
        return { kind: "yesterday" }
    return { kind: "date" }
}

// Do not disturb is on until a time in the future, or an app asked for it.
function dndActive(inhibitedUntil, now, byApplication) {
    if (byApplication)
        return true
    return inhibitedUntil instanceof Date && !isNaN(inhibitedUntil.getTime()) && inhibitedUntil > now
}

// --- calendar --------------------------------------------------------------

// The 6 x 7 days shown for a month (month 0-11), weeks starting on
// firstDay (0 Sunday ... 6 Saturday, Qt's Locale.firstDayOfWeek with 7
// read as Sunday). Each cell: { year, month, day, inMonth }.
function monthGrid(year, month, firstDay) {
    firstDay = ((Number(firstDay) || 0) % 7 + 7) % 7
    var first = new Date(year, month, 1)
    var lead = (first.getDay() - firstDay + 7) % 7
    var cells = []
    for (var i = 0; i < 42; ++i) {
        var d = new Date(year, month, 1 - lead + i)
        cells.push({ year: d.getFullYear(), month: d.getMonth(), day: d.getDate(),
                     inMonth: d.getMonth() === month })
    }
    return cells
}

// The order of the weekday names in the header: day numbers 0-6.
function weekdays(firstDay) {
    firstDay = ((Number(firstDay) || 0) % 7 + 7) % 7
    var out = []
    for (var i = 0; i < 7; ++i)
        out.push((firstDay + i) % 7)
    return out
}

function sameDay(a, b) {
    return a.getFullYear() === b.getFullYear() && a.getMonth() === b.getMonth() && a.getDate() === b.getDate()
}

// A month later or earlier: { year, month }.
function addMonths(year, month, delta) {
    var m = month + delta
    return { year: year + Math.floor(m / 12), month: ((m % 12) + 12) % 12 }
}

// --- Focus -----------------------------------------------------------------

var focusMin = 5
var focusMax = 240
var focusDefault = 30

// The "+" and "-" of the Focus strip: 5 minutes at a time up to 30, then
// 15 up to an hour, then 30, between 5 minutes and 4 hours.
function focusStep(minutes, up) {
    var m = Math.max(focusMin, Math.min(focusMax, Math.round(minutes)))
    if (up) {
        var step = m < 30 ? 5 : m < 60 ? 15 : 30
        return Math.min(focusMax, m + step)
    }
    var down = m <= 30 ? 5 : m <= 60 ? 15 : 30
    return Math.max(focusMin, m - down)
}

function focusEnd(now, minutes) {
    return new Date(now.getTime() + minutes * 60000)
}

// Seconds left of a session, 0 when over or off.
function focusRemaining(until, now) {
    if (!(until instanceof Date) || isNaN(until.getTime()))
        return 0
    return Math.max(0, Math.ceil((until.getTime() - now.getTime()) / 1000))
}

// What the ring on the clock shows: the part of the session still to go,
// 0 to 1; -1 when no session runs.
function focusFraction(remainingSeconds, totalMinutes) {
    if (remainingSeconds <= 0 || totalMinutes <= 0)
        return -1
    return Math.min(1, remainingSeconds / (totalMinutes * 60))
}

// "25:00" or "1:05:00" for the time left.
function focusText(remainingSeconds) {
    var s = Math.max(0, Math.floor(remainingSeconds))
    var h = Math.floor(s / 3600)
    var m = Math.floor((s % 3600) / 60)
    var sec = s % 60
    var two = function (n) { return n < 10 ? "0" + n : String(n) }
    return h > 0 ? h + ":" + two(m) + ":" + two(sec) : m + ":" + two(sec)
}

// --- notification history ---------------------------------------------

// The apps whose notifications stay out of the centre. Plasma's default
// puts every app without a desktop file ("@other") there, so a toast that
// ended on its own was gone from the centre; Windows lists them all. Apps
// the user took out by name stay out.
function historyBlacklist(apps) {
    var out = []
    for (var i = 0; apps && i < apps.length; ++i) {
        if (apps[i] !== "@other")
            out.push(apps[i])
    }
    return out
}

// --- Raye Calendar ---------------------------------------------------------

// "2026-11-05"
function isoDay(year, month, day) {
    var m = month + 1
    return year + "-" + (m < 10 ? "0" : "") + m + "-" + (day < 10 ? "0" : "") + day
}

// What raye-calendar-tool agenda prints: a JSON list whose times have no
// zone, which Date reads as local time, as the tool wrote them.
function parseAgenda(text) {
    var list
    try {
        list = JSON.parse(text)
    } catch (e) {
        return []
    }
    if (!Array.isArray(list))
        return []
    var out = []
    for (var i = 0; i < list.length; ++i) {
        var e = list[i]
        var start = new Date(e.start), end = new Date(e.end)
        if (isNaN(start.getTime()) || isNaN(end.getTime()))
            continue
        out.push({ title: String(e.title || ""), start: start, end: end, allDay: !!e.allDay,
                   color: String(e.color || ""), holiday: !!e.holiday })
    }
    return out
}

// The events that touch a day: all-day ones first, then by time.
function eventsOn(events, year, month, day) {
    var s = new Date(year, month, day), e = new Date(year, month, day + 1)
    var out = (events || []).filter(function (ev) {
        return ev.start < e && (ev.end > s || (ev.end.getTime() === ev.start.getTime() && ev.start >= s))
    })
    out.sort(function (a, b) {
        if (a.allDay !== b.allDay)
            return a.allDay ? -1 : 1
        return a.start - b.start
    })
    return out
}

// --- the centre on a small screen ------------------------------------------

// The room for the centre's cards and the gap between them: from the top
// of the clock's screen down to the taskbar, kept inside the free area of
// that screen when Qt knows it, with `margin` above and below. All rects
// are in desktop coordinates.
function centreRoom(screen, panelTop, area, margin) {
    var top = screen.y
    var bottom = panelTop
    if (area && area.width > 0 && area.height > 0) {
        top = Math.max(top, area.y)
        bottom = Math.min(bottom, area.y + area.height)
    }
    return Math.max(300, bottom - top - 2 * margin)
}

// What of the calendar card fits under the notifications in `room` (the
// height of both cards and the gap between them): first the chosen day's
// events go, then the month folds away. The card is never cut. parts: the
// heights of the card's header, month, events and Focus strip, the gap,
// the least the notifications card may have, and what it would like
// (up to a few notifications) before the events give it their place.
function calendarFit(room, parts) {
    var card = parts.header + parts.focus + parts.gap
    var wanted = Math.max(parts.notificationsMin, Math.min(parts.notificationsWanted, notificationsBeforeEvents))
    if (room >= card + parts.month + parts.agenda + wanted)
        return { month: true, agenda: true }
    if (room >= card + parts.month + parts.notificationsMin)
        return { month: true, agenda: false }
    return { month: false, agenda: false }
}
var notificationsBeforeEvents = 300

// Where the centre's window goes: its right edge `edge` px from the right of
// the screen, its bottom `edge` px above the taskbar. The window holds the
// cards and their shadow `margin` around them.
function centreGeometry(screen, panelTop, width, height, margin, edge) {
    var w = Math.ceil(width)
    var h = Math.ceil(height)
    return { x: Math.round(screen.x + screen.width - w + margin - edge),
             y: Math.round(panelTop - edge - h + margin), width: w, height: h }
}
