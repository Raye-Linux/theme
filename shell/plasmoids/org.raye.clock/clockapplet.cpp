// SPDX-License-Identifier: GPL-3.0-or-later

#include "clockapplet.h"
#include "toastsettings.h"
#include "traybell.h"

#include <KPluginFactory>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QScreen>
#include <QWindow>
#include <KConfigLoader>
#include <KSharedConfig>
#include <Plasma/Containment>
#include <Plasma/Corona>
#include <QTimer>

#include <notificationmanager/server.h>

// the clocks of this plasmashell, in the order they appeared
static QList<RayeClockApplet *> &clocks()
{
    static QList<RayeClockApplet *> list;
    return list;
}

RayeClockApplet::RayeClockApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Applet(parent, data, args)
{
}

RayeClockApplet::~RayeClockApplet()
{
    clocks().removeAll(this);
    electToastOwner();
}

void RayeClockApplet::electToastOwner()
{
    QList<int> screens;
    for (RayeClockApplet *clock : clocks())
        screens.append(clock->containment() ? clock->containment()->screen() : -1);
    const int owner = RayeToasts::ownerOf(screens);
    for (int i = 0; i < clocks().size(); ++i) {
        RayeClockApplet *clock = clocks().at(i);
        if (clock->m_ownsToasts != (i == owner)) {
            clock->m_ownsToasts = (i == owner);
            Q_EMIT clock->ownsToastsChanged();
        }
    }
}

bool RayeClockApplet::ownsToasts() const
{
    return m_ownsToasts;
}

void RayeClockApplet::turnOffNotifications(const QString &desktopEntry, const QString &notifyRcName)
{
    RayeToasts::turnOff(KSharedConfig::openConfig(QStringLiteral("plasmanotifyrc")), desktopEntry, notifyRcName);
}

QRect RayeClockApplet::moveToClockScreen(QWindow *window, QWindow *clockWindow)
{
    QScreen *screen = clockWindow ? clockWindow->screen() : nullptr;
    if (!screen)
        return {};
    if (window && !window->isVisible() && window->screen() != screen)
        window->setScreen(screen);
    return screen->availableGeometry();
}

bool RayeClockApplet::calendarAvailable() const
{
    return QFileInfo(QStringLiteral(RAYE_CALENDAR_TOOL)).isExecutable();
}

void RayeClockApplet::requestAgenda(const QString &from, const QString &to)
{
    if (!calendarAvailable())
        return;
    // a short process, never a blocking wait: the shell keeps drawing
    auto *p = new QProcess(this);
    QPointer<RayeClockApplet> self(this);
    const quint64 ticket = ++m_agendaTicket;
    connect(p, &QProcess::finished, this, [self, p, ticket](int code, QProcess::ExitStatus status) {
        if (self && self->m_agendaTicket == ticket && status == QProcess::NormalExit && code == 0)
            Q_EMIT self->agendaReady(QString::fromUtf8(p->readAllStandardOutput()));
        p->deleteLater();
    });
    connect(p, &QProcess::errorOccurred, p, &QObject::deleteLater);
    p->start(QStringLiteral(RAYE_CALENDAR_TOOL), {QStringLiteral("agenda"), from, to});
}

void RayeClockApplet::init()
{
    Plasma::Applet::init();
    clocks().append(this);
    if (Plasma::Containment *c = containment())
        connect(c, &Plasma::Containment::screenChanged, this, &RayeClockApplet::electToastOwner);
    electToastOwner();
    // One server for the whole session: a second clock (another screen)
    // finds it running and leaves it alone.
    static bool started = false;
    if (!started) {
        started = true;
        NotificationManager::Server::self().init();
    }
    // once the panel has made the rest of its items
    QTimer::singleShot(0, this, &RayeClockApplet::takeBellFromTrays);
}

// every tray of the shell; the tray is a containment held by a panel
void RayeClockApplet::takeBellFromTrays()
{
    Plasma::Containment *own = containment();
    if (!own || !own->corona())
        return;
    const auto all = own->corona()->containments();
    for (Plasma::Containment *panel : all) {
        const auto applets = panel->applets();
        for (Plasma::Applet *applet : applets) {
            if (applet->pluginMetaData().pluginId() == QLatin1String("org.kde.plasma.systemtray"))
                takeBellFrom(applet);
        }
    }
}

void RayeClockApplet::takeBellFrom(Plasma::Applet *tray)
{
    KConfigLoader *scheme = tray->configScheme();
    if (!scheme)
        return;
    auto list = [scheme](const char *name) {
        KConfigSkeletonItem *item = scheme->findItemByName(QLatin1String(name));
        return item ? item->property().toStringList() : QStringList();
    };
    RayeTrayBell::Lists lists{list("extraItems"), list("knownItems"), list("hiddenItems"), list("shownItems")};
    if (!RayeTrayBell::take(lists))
        return;
    auto set = [scheme](const char *name, const QStringList &value) {
        if (KConfigSkeletonItem *item = scheme->findItemByName(QLatin1String(name)))
            item->setProperty(value);
    };
    set("extraItems", lists.extra);
    set("knownItems", lists.known);
    set("hiddenItems", lists.hidden);
    set("shownItems", lists.shown);
    // the mark raye-taskbar-sync reads to put it back for the stock clock
    tray->config().group(QStringLiteral("Raye")).writeEntry("BellTaken", QStringLiteral("true"));
    // was it running? its pop-ups then need telling (StockBell.qml)
    bool running = false;
    if (auto *box = qobject_cast<Plasma::Containment *>(tray)) {
        const auto items = box->applets();
        for (Plasma::Applet *item : items)
            running = running || item->pluginMetaData().pluginId() == RayeTrayBell::id();
    }
    // saving tells the tray, which then drops the item
    scheme->save();
    if (running)
        Q_EMIT stockBellTaken();
}

K_PLUGIN_CLASS_WITH_JSON(RayeClockApplet, "metadata.json")

#include "clockapplet.moc"
