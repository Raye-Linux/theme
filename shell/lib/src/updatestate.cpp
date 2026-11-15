// SPDX-License-Identifier: GPL-3.0-or-later

#include "updatestate.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <grp.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

using namespace Qt::StringLiterals;

namespace
{
// a state file is a few kB; anything far bigger is not one
constexpr qint64 MaxStateSize = 4 * 1024 * 1024;

const QStringList Phases = {u"idle"_s, u"checking"_s, u"waiting"_s, u"downloading"_s, u"ready"_s,
                            u"armed"_s, u"installing"_s, u"paused"_s, u"error"_s};

qint64 number(const QJsonValue &v)
{
    if (!v.isDouble())
        return 0;
    const double d = v.toDouble();
    return d > 0 && d < 1e15 ? qint64(d) : 0;
}

QString token(const QJsonValue &v)
{
    const QString s = v.toString();
    // codes and reasons are short plain words
    for (QChar c : s) {
        if (!(c.isLetterOrNumber() || c == u'-' || c == u'_'))
            return {};
    }
    return s.left(64);
}
} // namespace

UpdateInfo UpdateInfo::parse(const QByteArray &json)
{
    UpdateInfo i;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return i;
    const QJsonObject o = doc.object();

    // schema version 1: only the old keys
    if (!o.contains("phase"_L1) && o.contains("state"_L1)) {
        const QString state = o.value("state"_L1).toString();
        i.phase = Phases.contains(state) ? state : u"idle"_s;
        i.count = int(qMin<qint64>(number(o.value("updates"_L1)), 100000));
        i.needsRestart = i.phase == u"ready"_s;
        i.readyId = o.value("ready_id"_L1).toString().left(128);
        i.pausedUntil = number(o.value("paused_until"_L1));
        i.lastCheck = number(o.value("checked"_L1));
        for (const QJsonValue &p : o.value("packages"_L1).toArray())
            i.pendingNames << p.toString();
        return i;
    }

    const QString phase = o.value("phase"_L1).toString();
    if (!Phases.contains(phase))
        return i;
    i.phase = phase;
    i.waitReason = token(o.value("waitReason"_L1));
    i.count = int(qMin<qint64>(number(o.value("count"_L1)), 100000));
    i.downloadedBytes = number(o.value("downloadedBytes"_L1));
    i.totalBytes = number(o.value("totalBytes"_L1));
    i.needsRestart = o.value("needsRestart"_L1).toBool();
    i.readyId = o.value("readyId"_L1).toString().left(128);
    i.pausedUntil = number(o.value("pausedUntil"_L1));
    i.lastCheck = number(o.value("lastCheck"_L1));
    i.kernelChange = o.value("kernelChange"_L1).toBool();
    const QJsonObject armed = o.value("armed"_L1).toObject();
    const QString then = armed.value("then"_L1).toString();
    if (then == u"restart"_s || then == u"shutdown"_s) {
        i.armed = true;
        i.armedThen = then;
        i.armedAt = number(armed.value("at"_L1));
    }
    i.errorCode = token(o.value("error"_L1).toObject().value("code"_L1));
    i.firmwareCount = int(qMin<qsizetype>(o.value("firmware"_L1).toArray().size(), 1000));
    for (const QJsonValue &p : o.value("pending"_L1).toArray())
        i.pendingNames << p.toObject().value("name"_L1).toString();
    return i;
}

bool UpdateInfo::canArm() const
{
    return needsRestart && !armed && phase == u"ready"_s;
}

bool UpdateInfo::restartPending() const
{
    return (needsRestart && phase == u"ready"_s) || phase == u"armed"_s;
}

double UpdateInfo::progress() const
{
    if (totalBytes <= 0)
        return phase == u"ready"_s ? 1.0 : 0.0;
    return qBound(0.0, double(downloadedBytes) / double(totalBytes), 1.0);
}

UpdateState::UpdateState(QObject *parent)
    : QObject(parent)
{
    // the file is replaced on every write: wait for the new one to land
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &UpdateState::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    m_restart = [] {
        // Plasma's own restart: apps are asked to save their work first
        auto msg = QDBusMessage::createMethodCall(u"org.kde.Shutdown"_s, u"/Shutdown"_s,
                                                  u"org.kde.Shutdown"_s, u"logoutAndReboot"_s);
        QDBusConnection::sessionBus().asyncCall(msg);
    };
    rewatch();
    refresh();
}

UpdateState::~UpdateState()
{
    if (m_process) {
        m_process->disconnect(this);
        // pkexec may still wait for a password: let it end on its own
        m_process->setParent(nullptr);
        connect(m_process, &QProcess::finished, m_process, &QObject::deleteLater);
    }
}

QDateTime UpdateState::pausedUntil() const
{
    return m_info.pausedUntil > 0 ? QDateTime::fromSecsSinceEpoch(m_info.pausedUntil) : QDateTime();
}

QDateTime UpdateState::lastCheck() const
{
    return m_info.lastCheck > 0 ? QDateTime::fromSecsSinceEpoch(m_info.lastCheck) : QDateTime();
}

bool UpdateState::busy() const
{
    return m_process != nullptr;
}

bool UpdateState::armsWithoutPassword() const
{
    const struct group *g = getgrnam(m_adminGroup.constData());
    if (!g)
        return false;
    if (getegid() == g->gr_gid)
        return true;
    const int n = getgroups(0, nullptr);
    if (n <= 0)
        return false;
    std::vector<gid_t> ids(n);
    const int got = getgroups(n, ids.data());
    return got > 0 && std::find(ids.begin(), ids.begin() + got, g->gr_gid) != ids.begin() + got;
}

void UpdateState::setAdminGroup(const QByteArray &name)
{
    m_adminGroup = name;
    Q_EMIT changed();
}

void UpdateState::setStatePath(const QString &path)
{
    m_statePath = path;
    rewatch();
    refresh();
}

void UpdateState::setToolPath(const QString &path)
{
    m_toolPath = path;
}

void UpdateState::setRestartRequest(std::function<void()> request)
{
    m_restart = std::move(request);
}

void UpdateState::rewatch()
{
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    // the folder: a watch on the file alone stops at the first replace
    const QString dir = QFileInfo(m_statePath).absolutePath();
    if (QFileInfo::exists(dir))
        m_watcher.addPath(dir);
    if (QFileInfo::exists(m_statePath))
        m_watcher.addPath(m_statePath);
}

void UpdateState::refresh()
{
    UpdateInfo info;
    QFile f(m_statePath);
    if (f.size() <= MaxStateSize && f.open(QIODevice::ReadOnly))
        info = UpdateInfo::parse(f.read(MaxStateSize));
    // a replaced file is a new inode: watch it again
    if (QFileInfo::exists(m_statePath) && !m_watcher.files().contains(m_statePath))
        m_watcher.addPath(m_statePath);
    m_info = info;
    Q_EMIT changed();
}

void UpdateState::updateAndRestart()
{
    arm(u"restart"_s);
}

void UpdateState::updateAndShutdown()
{
    arm(u"shutdown"_s);
}

void UpdateState::arm(const QString &then)
{
    if (m_process)
        return;
    refresh();
    if (!m_info.canArm()) {
        Q_EMIT armFailed(u"nothing-ready"_s);
        return;
    }
    auto *p = new QProcess(this);
    m_process = p;
    p->setProgram(m_toolPath);
    p->setArguments({u"arm"_s, u"--then"_s, then});
    p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    p->setStandardOutputFile(QProcess::nullDevice());
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus status) {
        p->deleteLater();
        m_process = nullptr;
        Q_EMIT busyChanged();
        refresh();
        if (status != QProcess::NormalExit) {
            Q_EMIT armFailed(u"failed"_s);
            return;
        }
        switch (code) {
        case 0:
            Q_EMIT armSucceeded();
            requestRestart();
            return;
        case 3:
            Q_EMIT armFailed(u"nothing-ready"_s);
            return;
        case 4:
            Q_EMIT armFailed(u"busy"_s);
            return;
        case 126:
        case 127:
            Q_EMIT armFailed(u"cancelled"_s);
            return;
        default:
            Q_EMIT armFailed(u"failed"_s);
        }
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        p->deleteLater();
        m_process = nullptr;
        Q_EMIT busyChanged();
        Q_EMIT armFailed(u"failed"_s);
    });
    Q_EMIT busyChanged();
    p->start();
}

void UpdateState::requestRestart()
{
    if (m_restart)
        m_restart();
}
