// SPDX-License-Identifier: GPL-3.0-or-later

#include "hyprland.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <memory>

namespace
{
// the events after which the windows are read again: the rest (a layer
// opening, the keyboard layout) changes nothing the shell shows
const QList<QByteArray> clientEvents = {
    "openwindow", "closewindow", "movewindow", "movewindowv2", "windowtitle", "windowtitlev2",
    "activewindowv2", "urgent", "workspacev2", "fullscreen", "changefloatingmode",
    "focusedmonv2", "monitoraddedv2", "monitorremovedv2",
};
}

Hyprland::Hyprland(QObject *parent)
    : QObject(parent)
{
    m_reload.setSingleShot(true);
    m_reload.setInterval(30);
    connect(&m_reload, &QTimer::timeout, this, &Hyprland::reload);

    // Hyprland may still be starting when the shell is
    m_retry.setInterval(1000);
    connect(&m_retry, &QTimer::timeout, this, &Hyprland::connectEvents);

    connect(&m_events, &QLocalSocket::readyRead, this, &Hyprland::readEvents);
    connect(&m_events, &QLocalSocket::connected, this, [this] {
        m_retry.stop();
        m_available = true;
        Q_EMIT availableChanged();
        reload();
    });
    connect(&m_events, &QLocalSocket::disconnected, this, [this] {
        m_available = false;
        Q_EMIT availableChanged();
        m_retry.start();
    });
    connect(&m_events, &QLocalSocket::errorOccurred, this, [this] {
        if (!m_retry.isActive())
            m_retry.start();
    });
    connectEvents();
}

QString Hyprland::socketDir()
{
    const QString forced = qEnvironmentVariable("RAYE_HYPR_SOCKET_DIR");
    if (!forced.isEmpty())
        return forced;
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    return runtime + QStringLiteral("/hypr/") + qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
}

void Hyprland::connectEvents()
{
    if (m_events.state() != QLocalSocket::UnconnectedState)
        return;
    m_events.connectToServer(socketDir() + QStringLiteral("/.socket2.sock"));
}

void Hyprland::readEvents()
{
    m_pending += m_events.readAll();
    bool changed = false;
    qsizetype end;
    while ((end = m_pending.indexOf('\n')) >= 0) {
        const QByteArray line = m_pending.left(end);
        m_pending.remove(0, end + 1);
        const qsizetype split = line.indexOf(">>");
        if (split < 0)
            continue;
        const QByteArray name = line.left(split);
        Q_EMIT event(QString::fromUtf8(name), QString::fromUtf8(line.mid(split + 2)));
        if (clientEvents.contains(name))
            changed = true;
    }
    if (changed)
        refresh();
}

void Hyprland::refresh()
{
    m_reload.start();
}

QByteArray Hyprland::ask(const QByteArray &text, int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(socketDir() + QStringLiteral("/.socket.sock"));
    if (!socket.waitForConnected(timeoutMs))
        return {};
    socket.write(text);
    socket.flush();
    QByteArray answer;
    // Hyprland closes the connection once it has answered
    while (socket.waitForReadyRead(timeoutMs))
        answer += socket.readAll();
    answer += socket.readAll();
    return answer;
}

void Hyprland::askAsync(const QByteArray &text, std::function<void(const QByteArray &)> done)
{
    auto *socket = new QLocalSocket(this);
    auto answer = std::make_shared<QByteArray>();
    auto finished = std::make_shared<bool>(false);
    auto finish = [socket, answer, finished, done] {
        if (*finished)
            return;
        *finished = true;
        *answer += socket->readAll();
        if (done)
            done(*answer);
        socket->deleteLater();
    };
    connect(socket, &QLocalSocket::connected, socket, [socket, text] {
        socket->write(text);
        socket->flush();
    });
    connect(socket, &QLocalSocket::readyRead, socket, [socket, answer] {
        *answer += socket->readAll();
    });
    connect(socket, &QLocalSocket::disconnected, socket, finish);
    connect(socket, &QLocalSocket::errorOccurred, socket, finish);
    socket->connectToServer(socketDir() + QStringLiteral("/.socket.sock"));
}

void Hyprland::request(const QString &text)
{
    askAsync(text.toUtf8(), nullptr);
}

void Hyprland::reload()
{
    // three requests one after the other: the list only goes out once all
    // three answers are in, so the active window always matches the list
    askAsync("j/clients", [this](const QByteArray &clients) {
        askAsync("j/activewindow", [this, clients](const QByteArray &active) {
            askAsync("j/monitors", [this, clients, active](const QByteArray &monitors) {
                const QJsonDocument list = QJsonDocument::fromJson(clients);
                if (!list.isArray())
                    return;
                m_clients = list.array().toVariantList();
                m_activeAddress = QJsonDocument::fromJson(active).object().value(QLatin1String("address")).toString();
                m_monitors = QJsonDocument::fromJson(monitors).array().toVariantList();
                for (const QVariant &m : std::as_const(m_monitors)) {
                    const QVariantMap mon = m.toMap();
                    if (mon.value(QStringLiteral("focused")).toBool())
                        m_activeWorkspace = mon.value(QStringLiteral("activeWorkspace")).toMap().value(QStringLiteral("id")).toInt();
                }
                Q_EMIT clientsChanged();
                Q_EMIT monitorsChanged();
            });
        });
    });
}

QVariantMap Hyprland::activeWindow()
{
    return QJsonDocument::fromJson(ask("j/activewindow")).object().toVariantMap();
}

QVariantMap Hyprland::focusedMonitor()
{
    // the list read with the windows is current: a monitor event reads it again
    QVariantList monitors = m_monitors;
    if (monitors.isEmpty())
        monitors = QJsonDocument::fromJson(ask("j/monitors")).array().toVariantList();
    for (const QVariant &m : std::as_const(monitors)) {
        if (m.toMap().value(QStringLiteral("focused")).toBool())
            return m.toMap();
    }
    return monitors.isEmpty() ? QVariantMap() : monitors.first().toMap();
}
