// SPDX-License-Identifier: GPL-3.0-or-later

#include "appjumplist.h"

#include <KIO/ApplicationLauncherJob>
#include <KNotificationJobUiDelegate>
#include <KService>

#include <PlasmaActivities/Stats/Query>
#include <PlasmaActivities/Stats/ResultSet>
#include <PlasmaActivities/Stats/Terms>

#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>

using namespace Qt::StringLiterals;
namespace Stats = KActivities::Stats;
using namespace KActivities::Stats;
using namespace KActivities::Stats::Terms;

QString JumpList::storageId(const QString &desktopId)
{
    static const QRegularExpression re(u"^[A-Za-z0-9][A-Za-z0-9._+-]*\\.desktop$"_s);
    if (desktopId.size() > 255 || !re.match(desktopId).hasMatch())
        return {};
    return desktopId.chopped(8);
}

QList<QUrl> JumpList::pickRecent(const QList<QUrl> &newestFirst, int limit, const std::function<bool(const QUrl &)> &exists)
{
    QList<QUrl> out;
    for (const QUrl &url : newestFirst) {
        if (out.size() >= limit)
            break;
        if (!url.isValid() || !url.isLocalFile() || out.contains(url))
            continue;
        if (exists && !exists(url))
            continue;
        out << url;
    }
    return out;
}

AppJumpList::AppJumpList(QObject *parent)
    : QObject(parent)
{
}

void AppJumpList::setDesktopId(const QString &id)
{
    if (m_desktopId == id)
        return;
    m_desktopId = id;
    Q_EMIT desktopIdChanged();
    reload();
}

void AppJumpList::setRecentLimit(int limit)
{
    limit = qBound(0, limit, 20);
    if (m_recentLimit == limit)
        return;
    m_recentLimit = limit;
    Q_EMIT recentLimitChanged();
    reload();
}

static KService::Ptr serviceFor(const QString &desktopId)
{
    const QString storage = JumpList::storageId(desktopId);
    if (storage.isEmpty())
        return {};
    KService::Ptr s = KService::serviceByDesktopName(storage);
    if (!s)
        s = KService::serviceByStorageId(desktopId);
    return s;
}

void AppJumpList::reload()
{
    const KService::Ptr service = serviceFor(m_desktopId);
    m_valid = service && service->isValid();
    m_name = m_valid ? service->name() : QString();
    m_iconName = m_valid ? service->icon() : QString();
    m_actions.clear();
    m_recent.clear();
    if (m_valid) {
        const auto actions = service->actions();
        for (const KServiceAction &a : actions) {
            if (a.noDisplay() || a.isSeparator() || a.text().isEmpty())
                continue;
            m_actions << QVariantMap{{u"text"_s, a.text()}, {u"icon"_s, a.icon().isEmpty() ? m_iconName : a.icon()}};
        }
    }
    if (m_valid && m_recentLimit > 0) {
        // the files this app opened, from the activity manager (the same
        // record Start's "Recommended" reads)
        const auto query = UsedResources | RecentlyUsedFirst | Agent(JumpList::storageId(m_desktopId))
            | Type::any() | Activity::current() | Url::file() | Limit(m_recentLimit * 3);
        QList<QUrl> urls;
        const Stats::ResultSet results(query);
        for (const Stats::ResultSet::Result &r : results)
            urls << QUrl::fromUserInput(r.resource());
        static QMimeDatabase mimes;
        const auto picked = JumpList::pickRecent(urls, m_recentLimit, [](const QUrl &u) {
            return QFileInfo::exists(u.toLocalFile());
        });
        for (const QUrl &u : picked) {
            m_recent << QVariantMap{{u"url"_s, u},
                                    {u"name"_s, u.fileName()},
                                    {u"icon"_s, mimes.mimeTypeForUrl(u).iconName()}};
        }
    }
    Q_EMIT changed();
}

static bool start(KIO::ApplicationLauncherJob *job)
{
    job->setUiDelegate(new KNotificationJobUiDelegate(KJobUiDelegate::AutoErrorHandlingEnabled));
    job->start();
    return true;
}

bool AppJumpList::launch()
{
    const KService::Ptr service = serviceFor(m_desktopId);
    if (!service)
        return false;
    return start(new KIO::ApplicationLauncherJob(service));
}

bool AppJumpList::runAction(int index)
{
    const KService::Ptr service = serviceFor(m_desktopId);
    if (!service)
        return false;
    int shown = -1;
    const auto actions = service->actions();
    for (const KServiceAction &a : actions) {
        if (a.noDisplay() || a.isSeparator() || a.text().isEmpty())
            continue;
        if (++shown == index)
            return start(new KIO::ApplicationLauncherJob(a));
    }
    return false;
}

bool AppJumpList::openRecent(const QUrl &url)
{
    const KService::Ptr service = serviceFor(m_desktopId);
    if (!service || !url.isLocalFile())
        return false;
    auto *job = new KIO::ApplicationLauncherJob(service);
    job->setUrls({url});
    return start(job);
}
