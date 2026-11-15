// SPDX-License-Identifier: GPL-3.0-or-later

#include "recentlyadded.h"

#include <KConfig>
#include <KConfigGroup>
#include <KService>
#include <KSycoca>

#include <PlasmaActivities/Stats/Query>
#include <PlasmaActivities/Stats/ResultSet>
#include <PlasmaActivities/Stats/Terms>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::StringLiterals;
namespace Stats = KActivities::Stats;
using namespace KActivities::Stats;
using namespace KActivities::Stats::Terms;

namespace
{
constexpr qint64 Day = 24 * 60 * 60;
// how many apps the store keeps (a computer has a few hundred)
constexpr int MaxSeen = 5000;
}

QHash<QString, qint64> RecentlyAdded::updateSeen(const QHash<QString, qint64> &seen, const QStringList &installed, qint64 now, bool firstRun)
{
    QHash<QString, qint64> out;
    for (const QString &id : installed) {
        if (out.size() >= MaxSeen)
            break;
        if (firstRun)
            out.insert(id, 0);
        else
            out.insert(id, seen.value(id, now));
    }
    return out;
}

QStringList RecentlyAdded::select(const QHash<QString, qint64> &seen, const QHash<QString, qint64> &lastLaunched, qint64 now, int maxAgeDays, int limit)
{
    QList<std::pair<qint64, QString>> fresh;
    for (auto it = seen.cbegin(); it != seen.cend(); ++it) {
        const qint64 added = it.value();
        if (added <= 0 || added > now + Day || now - added > qint64(maxAgeDays) * Day)
            continue;
        if (lastLaunched.value(it.key(), 0) >= added)
            continue;
        fresh.append({added, it.key()});
    }
    std::sort(fresh.begin(), fresh.end(), [](const auto &a, const auto &b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    QStringList out;
    for (const auto &f : std::as_const(fresh)) {
        if (out.size() >= limit)
            break;
        out << f.second;
    }
    return out;
}

RecentlyAddedModel::RecentlyAddedModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_apps(&RecentlyAddedModel::installedApps)
    , m_launches(&RecentlyAddedModel::launchedApps)
    , m_clock([] { return QDateTime::currentSecsSinceEpoch(); })
    , m_storePath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/raye/shell/apps-seen"_s)
{
    m_later.setSingleShot(true);
    m_later.setInterval(0);
    connect(&m_later, &QTimer::timeout, this, &RecentlyAddedModel::refresh);
    connect(KSycoca::self(), &KSycoca::databaseChanged, &m_later, qOverload<>(&QTimer::start));
    // an app stops being new a week on, or once opened: look again now and then
    auto *hourly = new QTimer(this);
    hourly->setInterval(60 * 60 * 1000);
    connect(hourly, &QTimer::timeout, this, &RecentlyAddedModel::refresh);
    hourly->start();
    m_later.start();
}

QList<AppEntry> RecentlyAddedModel::installedApps()
{
    QList<AppEntry> out;
    const KService::List all = KService::allServices();
    for (const KService::Ptr &s : all) {
        if (!s->isApplication() || s->noDisplay() || !s->showOnCurrentPlatform())
            continue;
        out.append({s->storageId(), s->name(), s->icon()});
    }
    return out;
}

QHash<QString, qint64> RecentlyAddedModel::launchedApps()
{
    // what Plasma's menus and the shell's Launcher record as used
    const auto query = UsedResources | Agent::any() | Type::any() | Activity::any()
        | Url::startsWith(u"applications:"_s) | Limit(1000);
    QHash<QString, qint64> out;
    const Stats::ResultSet results(query);
    for (const Stats::ResultSet::Result &r : results) {
        const QString id = r.resource().mid(qsizetype(strlen("applications:")));
        out.insert(id, qMax<qint64>(out.value(id), r.lastUpdate()));
    }
    return out;
}

void RecentlyAddedModel::refresh()
{
    const qint64 now = m_clock();
    const QList<AppEntry> apps = m_apps();
    QStringList ids;
    QHash<QString, AppEntry> byId;
    for (const AppEntry &a : apps) {
        ids << a.desktopId;
        byId.insert(a.desktopId, a);
    }

    const bool firstRun = !QFileInfo::exists(m_storePath);
    KConfig store(m_storePath, KConfig::SimpleConfig);
    KConfigGroup group = store.group(u"Seen"_s);
    QHash<QString, qint64> seen;
    for (const QString &k : group.keyList())
        seen.insert(k, group.readEntry(k, qint64(0)));
    const QHash<QString, qint64> updated = RecentlyAdded::updateSeen(seen, ids, now, firstRun);
    if (updated != seen) {
        QDir().mkpath(QFileInfo(m_storePath).absolutePath());
        group.deleteGroup();
        KConfigGroup fresh = store.group(u"Seen"_s);
        for (auto it = updated.cbegin(); it != updated.cend(); ++it)
            fresh.writeEntry(it.key(), it.value());
        store.sync();
    }

    const QStringList picked = RecentlyAdded::select(updated, m_launches(), now, m_maxAgeDays, m_limit);
    QList<AppEntry> rows;
    for (const QString &id : picked)
        rows.append(byId.value(id));

    beginResetModel();
    m_rows = rows;
    m_added = updated;
    endResetModel();
    Q_EMIT countChanged();
}

int RecentlyAddedModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant RecentlyAddedModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const AppEntry &a = m_rows.at(index.row());
    switch (role) {
    case DesktopIdRole:
        return a.desktopId;
    case Qt::DisplayRole:
    case NameRole:
        return a.name;
    case Qt::DecorationRole:
    case IconNameRole:
        return a.iconName;
    case AddedRole:
        return QDateTime::fromSecsSinceEpoch(m_added.value(a.desktopId));
    }
    return {};
}

QHash<int, QByteArray> RecentlyAddedModel::roleNames() const
{
    return {{DesktopIdRole, "desktopId"}, {NameRole, "name"}, {IconNameRole, "iconName"}, {AddedRole, "added"}};
}

void RecentlyAddedModel::setLimit(int limit)
{
    limit = qBound(0, limit, 50);
    if (limit == m_limit)
        return;
    m_limit = limit;
    Q_EMIT limitChanged();
    m_later.start();
}

void RecentlyAddedModel::setMaxAgeDays(int days)
{
    days = qBound(1, days, 90);
    if (days == m_maxAgeDays)
        return;
    m_maxAgeDays = days;
    Q_EMIT maxAgeDaysChanged();
    m_later.start();
}

void RecentlyAddedModel::setAppSource(std::function<QList<AppEntry>()> source)
{
    m_apps = std::move(source);
}

void RecentlyAddedModel::setLaunchSource(std::function<QHash<QString, qint64>()> source)
{
    m_launches = std::move(source);
}

void RecentlyAddedModel::setStorePath(const QString &path)
{
    m_storePath = path;
}

void RecentlyAddedModel::setClock(std::function<qint64()> clock)
{
    m_clock = std::move(clock);
}
