// The link to Hyprland: its request socket (one connection per request,
// JSON answers) and its event socket (one line per event), under
// $XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonValue>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>
#include <QVariant>
#include <qqmlregistration.h>

#include <functional>

class Hyprland : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QVariantList clients READ clients NOTIFY clientsChanged)
    Q_PROPERTY(QVariantList monitors READ monitors NOTIFY monitorsChanged)
    Q_PROPERTY(QString activeAddress READ activeAddress NOTIFY clientsChanged)
    Q_PROPERTY(int activeWorkspace READ activeWorkspace NOTIFY clientsChanged)

public:
    explicit Hyprland(QObject *parent = nullptr);

    // the folder of the two sockets; RAYE_HYPR_SOCKET_DIR replaces it
    static QString socketDir();

    bool available() const { return m_available; }
    QVariantList clients() const { return m_clients; }
    QVariantList monitors() const { return m_monitors; }
    QString activeAddress() const { return m_activeAddress; }
    int activeWorkspace() const { return m_activeWorkspace; }

    // a request as hyprctl writes it ("dispatch ...", "[[BATCH]]...", "j/clients")
    Q_INVOKABLE void request(const QString &text);
    // the list of windows, read again soon (several events, one read)
    Q_INVOKABLE void refresh();
    // the window with the focus and the monitor it is on, read now
    Q_INVOKABLE QVariantMap activeWindow();
    Q_INVOKABLE QVariantMap focusedMonitor();

    // one request, answered before it returns (the CLI side and tests)
    static QByteArray ask(const QByteArray &text, int timeoutMs = 1000);

Q_SIGNALS:
    void availableChanged();
    void clientsChanged();
    void monitorsChanged();
    void event(const QString &name, const QString &data);

private:
    void connectEvents();
    void readEvents();
    void reload();
    void askAsync(const QByteArray &text, std::function<void(const QByteArray &)> done);

    QLocalSocket m_events;
    QByteArray m_pending;
    QTimer m_reload;
    QTimer m_retry;
    bool m_available = false;
    QVariantList m_clients;
    QVariantList m_monitors;
    QString m_activeAddress;
    int m_activeWorkspace = 1;
};
