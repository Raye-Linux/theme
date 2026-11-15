// The polkit side: polkitd asks this agent to authenticate someone, and
// the prompt answers.
//
// Written on libpolkit-agent-1 rather than PolkitQt1 for two things the
// Qt wrapper cannot do: answer "cancelled" when the person says No (polkit
// then reports the request as dismissed, and pkexec exits with 126, which
// the Raye apps read as "the prompt was closed"), and tell which request
// polkit takes back. Requests that come while a prompt is open wait their
// turn.
#pragma once

#include "appinfo.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>

class Prompt;
class QQmlEngine;

class Listener : public QObject
{
    Q_OBJECT
public:
    explicit Listener(QObject *parent = nullptr);
    ~Listener() override;

    // Registers with polkitd for the session of this process.
    bool registerAgent(QString *error);

    // What polkitd sent; task is the GTask that carries the answer back.
    struct Pending {
        void *task = nullptr;
        QString actionId;
        QString message;
        QString iconName;
        QString cookie;
        QStringList users;
        qint64 subjectPid = 0;
        QString program;
        QString command;
        unsigned long cancelHandler = 0;
        bool resolving = false;
    };

    void add(const Pending &pending);
    void cancel(void *task);

private:
    void next();
    void open(const AppInfo::Caller &caller);
    // authorized: PAM said yes; dismissed: the person said No (or polkit
    // took the request back)
    void finish(bool authorized, bool dismissed);
    QQmlEngine *engine();
    void release();

    QList<Pending> m_queue;
    void *m_gobject = nullptr;
    void *m_registration = nullptr;
    QPointer<Prompt> m_prompt;
    QQmlEngine *m_engine = nullptr;
};
