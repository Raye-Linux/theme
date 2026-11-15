// SPDX-License-Identifier: GPL-3.0-or-later

#include "launcher.h"

#include <QCoreApplication>

#include <csignal>
#include <sys/stat.h>
#include <unistd.h>

#include <KIO/ApplicationLauncherJob>
#include <KIO/CommandLauncherJob>
#include <KIO/OpenUrlJob>
#include <KNotificationJobUiDelegate>
#include <KService>

#include <PlasmaActivities/ResourceInstance>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

Launcher::Launcher(QObject *parent)
    : QObject(parent)
{
}

void Launcher::forgetShellIntegration()
{
    if (qgetenv("QT_WAYLAND_SHELL_INTEGRATION").contains("layer-shell"))
        qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");
}

bool Launcher::launch(const QString &desktopId)
{
    forgetShellIntegration();
    QString id = desktopId.trimmed();
    if (id.isEmpty() || id.contains(u'/'))
        return false;
    KService::Ptr service = KService::serviceByStorageId(id);
    if (!service && !id.endsWith(u".desktop"_s))
        service = KService::serviceByStorageId(id + u".desktop"_s);
    if (!service || !service->isApplication()) {
        Q_EMIT failed(desktopId);
        return false;
    }
    auto *job = new KIO::ApplicationLauncherJob(service);
    job->setUiDelegate(new KNotificationJobUiDelegate(KJobUiDelegate::AutoErrorHandlingEnabled));
    connect(job, &KJob::result, this, [this, desktopId](KJob *j) {
        if (j->error())
            Q_EMIT failed(desktopId);
    });
    job->start();
    // what Plasma's own menus record, so "Most used" and "Recently added" know
    KActivities::ResourceInstance::notifyAccessed(QUrl(u"applications:"_s + service->storageId()), u"org.raye.shell"_s);
    return true;
}

bool Launcher::openUrl(const QUrl &url)
{
    forgetShellIntegration();
    if (!url.isValid() || url.isEmpty() || url.isRelative())
        return false;
    auto *job = new KIO::OpenUrlJob(url);
    job->setUiDelegate(new KNotificationJobUiDelegate(KJobUiDelegate::AutoErrorHandlingEnabled));
    // a script or a program is never run from here, only opened
    job->setRunExecutables(false);
    connect(job, &KJob::result, this, [this, url](KJob *j) {
        if (j->error())
            Q_EMIT failed(url.toDisplayString());
    });
    job->start();
    return true;
}

bool Launcher::isPageId(const QString &page)
{
    static const QRegularExpression re(u"^[A-Za-z0-9_][A-Za-z0-9_./-]{0,127}$"_s);
    return re.match(page).hasMatch() && !page.contains(u".."_s) && !page.contains(u"//"_s);
}

bool Launcher::isComponentName(const QString &component)
{
    static const QRegularExpression re(u"^[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$"_s);
    return re.match(component).hasMatch();
}

QStringList Launcher::settingsArguments(const QString &page)
{
    if (page.isEmpty())
        return {};
    return {u"--page"_s, page};
}

bool Launcher::openSettings(const QString &page)
{
    if (!page.isEmpty() && !isPageId(page)) {
        Q_EMIT failed(page);
        return false;
    }
    forgetShellIntegration();
    // no shell: the page goes to raye-settings as one argument
    auto *job = new KIO::CommandLauncherJob(u"raye-settings"_s, settingsArguments(page));
    job->setDesktopName(u"org.raye.settings"_s);
    job->setUiDelegate(new KNotificationJobUiDelegate(KJobUiDelegate::AutoErrorHandlingEnabled));
    connect(job, &KJob::result, this, [this, page](KJob *j) {
        if (j->error())
            Q_EMIT failed(page);
    });
    job->start();
    return true;
}

bool Launcher::invokeShortcut(const QString &component, const QString &name)
{
    if (!isComponentName(component) || name.isEmpty() || name.size() > 256) {
        Q_EMIT failed(name);
        return false;
    }
    // kglobalaccel names its objects after the component, with '_' for
    // anything that cannot be in a D-Bus path
    QString path = component;
    for (QChar &c : path) {
        if (!(c.isLetterOrNumber() && c.unicode() < 128) && c != u'_')
            c = u'_';
    }
    auto msg = QDBusMessage::createMethodCall(u"org.kde.kglobalaccel"_s, u"/component/"_s + path,
                                              u"org.kde.kglobalaccel.Component"_s, u"invokeShortcut"_s);
    msg << name;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, name](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (QDBusPendingReply<> reply = *w; reply.isError())
            Q_EMIT failed(name);
    });
    return true;
}

bool Launcher::mayEnd(qint64 pid, uint ownerUid, uint myUid)
{
    return pid > 1 && ownerUid == myUid && myUid != 0;
}

bool Launcher::endProcess(qint64 pid)
{
    if (pid <= 1 || pid == QCoreApplication::applicationPid())
        return false;
    struct stat st;
    const QByteArray proc = "/proc/" + QByteArray::number(pid);
    if (::stat(proc.constData(), &st) != 0 || !mayEnd(pid, st.st_uid, ::getuid()))
        return false;
    return ::kill(pid_t(pid), SIGKILL) == 0;
}

