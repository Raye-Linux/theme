// What the QML prompt shows, and the windows it is shown in: one per
// screen, all dimmed, the card on the screen with the active window.
#pragma once

#include "appinfo.h"
#include "request.h"

#include <QObject>
#include <QPointer>
#include <QVariantList>

class QQmlEngine;
class QQuickView;
class QScreen;

class Prompt : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QString appIcon READ appIcon CONSTANT)
    Q_PROPERTY(QString publisher READ publisher CONSTANT)
    Q_PROPERTY(QString program READ program CONSTANT)
    Q_PROPERTY(QString command READ command CONSTANT)
    Q_PROPERTY(QString actionId READ actionId CONSTANT)
    Q_PROPERTY(QString message READ message CONSTANT)
    Q_PROPERTY(QVariantList users READ users CONSTANT)
    Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)
    Q_PROPERTY(bool userIsSelf READ userIsSelf NOTIFY userChanged)
    Q_PROPERTY(Request *request READ request CONSTANT)

public:
    struct Account {
        QString user;
        QString name;
        QString face;
    };

    Prompt(const AppInfo::Caller &caller, const QString &actionId, const QString &message,
           const QList<Account> &accounts, Request *request, QObject *parent = nullptr);
    ~Prompt() override;

    QString appName() const { return m_caller.name; }
    QString appIcon() const { return m_caller.icon; }
    QString publisher() const { return m_caller.publisher; }
    QString program() const { return m_caller.program; }
    QString command() const { return m_caller.command; }
    QString actionId() const { return m_actionId; }
    QString message() const { return m_message; }
    QVariantList users() const;
    QString user() const { return m_request->user(); }
    void setUser(const QString &user);
    bool userIsSelf() const { return m_request->user() == m_self; }
    // the account signed in (the preview pretends)
    void setSelf(const QString &self);
    Request *request() const { return m_request; }

    void show(QQmlEngine *engine);
    void hide();

    // The account polkit would pick: the person signed in when they are
    // an administrator, otherwise the first administrator.
    static QString preferredUser(const QStringList &users, const QString &self);
    static Account account(const QString &user);

Q_SIGNALS:
    void userChanged();

private:
    QScreen *activeScreen() const;
    void addWindow(QQmlEngine *engine, QScreen *screen, bool card);

    AppInfo::Caller m_caller;
    QString m_actionId;
    QString m_message;
    QList<Account> m_accounts;
    Request *m_request;
    QString m_self;
    QList<QPointer<QQuickView>> m_windows;
};
