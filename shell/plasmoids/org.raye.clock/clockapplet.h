// The clock applet's C++ part: it starts the notification server of the
// session, once, before its QML loads, says which clock draws the toasts
// (every screen has a clock, one set of toasts is enough) and writes the
// "turn off notifications" of a toast's menu, asks Raye Calendar's tool
// for the events of the month shown, puts the centre on its clock's
// screen and takes the stock notifications item out of the trays.
// Everything else is QML.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Plasma/Applet>
#include <QRect>

class QWindow;

class RayeClockApplet : public Plasma::Applet
{
    Q_OBJECT
    Q_PROPERTY(bool ownsToasts READ ownsToasts NOTIFY ownsToastsChanged)
    Q_PROPERTY(bool calendarAvailable READ calendarAvailable CONSTANT)

public:
    RayeClockApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~RayeClockApplet() override;

    void init() override;

    bool ownsToasts() const;

    Q_INVOKABLE void turnOffNotifications(const QString &desktopEntry, const QString &notifyRcName);

    // Puts the centre's window on the screen of the clock that opens it (a
    // window keeps the screen it was made on, and is sized against it) and
    // gives that screen's free area; an empty rect when there is no clock.
    Q_INVOKABLE QRect moveToClockScreen(QWindow *window, QWindow *clockWindow);

    bool calendarAvailable() const;
    // dates as yyyy-MM-dd, both days in; the answer comes as agendaReady
    Q_INVOKABLE void requestAgenda(const QString &from, const QString &to);

Q_SIGNALS:
    void ownsToastsChanged();
    void agendaReady(const QString &json);
    // the stock notifications item was running in a tray and is gone
    void stockBellTaken();

private:
    static void electToastOwner();
    void takeBellFromTrays();
    void takeBellFrom(Plasma::Applet *tray);

    bool m_ownsToasts = false;
    // only the last month asked for may answer: the tool runs are concurrent
    quint64 m_agendaTicket = 0;
};
