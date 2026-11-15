// What Raye Update is doing, for the Start menu's power button and the
// rest of the shell: /var/lib/raye-update/state.json (the schema is in
// the update repo, docs/state.md), and "Update and restart" / "Update and
// shut down", which arm the downloaded updates through raye-update and
// then ask Plasma to restart so apps can save their work.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <qqmlregistration.h>

#include <functional>

// One reading of the state file. A file that cannot be read means "idle,
// nothing known"; unknown keys are ignored, missing keys take their
// default (docs/state.md).
struct UpdateInfo {
    QString phase = QStringLiteral("idle");
    QString waitReason;
    int count = 0;
    qint64 downloadedBytes = 0;
    qint64 totalBytes = 0;
    bool needsRestart = false;
    QString readyId;
    qint64 pausedUntil = 0;
    qint64 lastCheck = 0;
    bool armed = false;
    QString armedThen; // "restart" or "shutdown"
    qint64 armedAt = 0; // 0: at the next restart
    bool kernelChange = false;
    QString errorCode;
    int firmwareCount = 0;
    QStringList pendingNames;

    static UpdateInfo parse(const QByteArray &json);

    // downloaded and waiting for "Update and restart"
    bool canArm() const;
    // the power button shows "Update and restart"
    bool restartPending() const;
    // 0..1 while downloading
    double progress() const;
};

class UpdateState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString waitReason READ waitReason NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(qint64 downloadedBytes READ downloadedBytes NOTIFY changed)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(bool needsRestart READ needsRestart NOTIFY changed)
    Q_PROPERTY(bool restartPending READ restartPending NOTIFY changed)
    Q_PROPERTY(bool canArm READ canArm NOTIFY changed)
    Q_PROPERTY(bool armed READ armed NOTIFY changed)
    Q_PROPERTY(QString armedThen READ armedThen NOTIFY changed)
    Q_PROPERTY(bool kernelChange READ kernelChange NOTIFY changed)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY changed)
    Q_PROPERTY(QDateTime pausedUntil READ pausedUntil NOTIFY changed)
    Q_PROPERTY(QDateTime lastCheck READ lastCheck NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // in the group 50-raye-update.rules lets arm without a password: the
    // lock and Ctrl+Alt+Del screens cannot show a password prompt
    Q_PROPERTY(bool armsWithoutPassword READ armsWithoutPassword NOTIFY changed)

public:
    static constexpr const char *DefaultStatePath = "/var/lib/raye-update/state.json";
    static constexpr const char *DefaultToolPath = "/usr/bin/raye-update";

    explicit UpdateState(QObject *parent = nullptr);
    ~UpdateState() override;

    QString phase() const { return m_info.phase; }
    QString waitReason() const { return m_info.waitReason; }
    int count() const { return m_info.count; }
    qint64 downloadedBytes() const { return m_info.downloadedBytes; }
    qint64 totalBytes() const { return m_info.totalBytes; }
    double progress() const { return m_info.progress(); }
    bool needsRestart() const { return m_info.needsRestart; }
    bool restartPending() const { return m_info.restartPending(); }
    bool canArm() const { return m_info.canArm(); }
    bool armed() const { return m_info.armed; }
    QString armedThen() const { return m_info.armedThen; }
    bool kernelChange() const { return m_info.kernelChange; }
    QString errorCode() const { return m_info.errorCode; }
    QDateTime pausedUntil() const;
    QDateTime lastCheck() const;
    bool busy() const;
    bool armsWithoutPassword() const;

    // Arm the downloaded set, then restart the session. armFailed() says
    // why when it does not: "nothing-ready" (nothing downloaded),
    // "busy" (another install is armed), "cancelled" (the administrator
    // password was not given), "failed".
    Q_INVOKABLE void updateAndRestart();
    Q_INVOKABLE void updateAndShutdown();
    Q_INVOKABLE void refresh();

    // C++ only, for the tests: where the state file and the tool are,
    // and what "restart the session" does
    void setStatePath(const QString &path);
    void setToolPath(const QString &path);
    void setRestartRequest(std::function<void()> request);
    void setAdminGroup(const QByteArray &name);

Q_SIGNALS:
    void changed();
    void busyChanged();
    void armSucceeded();
    void armFailed(const QString &reason);

private:
    void arm(const QString &then);
    void rewatch();
    void requestRestart();

    QString m_statePath = QString::fromLatin1(DefaultStatePath);
    QString m_toolPath = QString::fromLatin1(DefaultToolPath);
    std::function<void()> m_restart;
    QByteArray m_adminGroup = QByteArrayLiteral("sudo");
    UpdateInfo m_info;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QPointer<QProcess> m_process;
};
