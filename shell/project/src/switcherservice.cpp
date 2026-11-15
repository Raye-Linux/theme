// SPDX-License-Identifier: GPL-3.0-or-later
#include "switcherservice.h"

#include <KIO/ApplicationLauncherJob>
#include <KService>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QTimer>

namespace
{
const QString s_service = QStringLiteral("org.kde.kscreen.osdService");
const QString s_path = QStringLiteral("/org/kde/kscreen/osdService");
// long enough to answer the calls that come together with a new screen
constexpr int s_lifetimeMs = 2000;
}

// Started as an app of its own: a child of this service would be stopped
// with it, two seconds later.
void SwitcherService::showActionSelector()
{
    const KService::Ptr panel = KService::serviceByDesktopName(QStringLiteral("org.raye.project"));
    if (!panel) {
        qWarning() << "raye-project: org.raye.project.desktop is missing";
        return;
    }
    auto *job = new KIO::ApplicationLauncherJob(panel);
    connect(job, &KJob::result, this, [](KJob *done) {
        if (done->error())
            qWarning() << "raye-project: could not open the panel:" << done->errorString();
    });
    job->start();
}

void SwitcherService::hideOsd()
{
    QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall(QStringLiteral("org.raye.Project"), QStringLiteral("/Project"),
                                                                      QStringLiteral("org.raye.Project"), QStringLiteral("dismiss")),
                                       QDBus::NoBlock);
}

int runSwitcherService(QCoreApplication &app)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.registerService(s_service)) {
        qWarning() << "raye-project: could not take" << s_service;
        return 1;
    }
    SwitcherService service;
    bus.registerObject(s_path, &service, QDBusConnection::ExportScriptableSlots);
    QTimer::singleShot(s_lifetimeMs, &app, &QCoreApplication::quit);
    return app.exec();
}
