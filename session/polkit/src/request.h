// One admin prompt from the moment it opens until polkit gets its answer.
//
// The PAM conversation only starts when the person presses Yes: the
// password typed before is kept here and handed to the first password
// question, so nothing is ever granted without a click on Yes, even for
// an account that needs no password. A wrong password ends the PAM
// session; the next Yes starts a new one. After three wrong passwords, or
// on No, the prompt closes without the permission.
//
// The password lives in the field, in m_pending until PAM asks for it,
// and nowhere else; it is never logged.
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>

// What the request needs from a PAM conversation (PolkitQt1's Session in
// the agent, a fake in the tests).
class AuthSession : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void start() = 0;
    virtual void respond(const QString &answer) = 0;
    virtual void cancel() = 0;

Q_SIGNALS:
    void question(const QString &text, bool echo);
    void info(const QString &text);
    void error(const QString &text);
    void finished(bool authorized);
};

class Request : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool wrongPassword READ wrongPassword NOTIFY stateChanged)
    Q_PROPERTY(QString question READ question NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)

public:
    enum State {
        Waiting, // for Yes or No
        Checking, // PAM has the answer
        Asking, // PAM asked something else than the password (question())
        Done,
    };
    Q_ENUM(State)

    using SessionFactory = std::function<AuthSession *(const QString &user, QObject *parent)>;

    Request(const QString &user, SessionFactory factory, QObject *parent = nullptr);
    ~Request() override;

    State state() const { return m_state; }
    bool busy() const { return m_state == Checking; }
    bool wrongPassword() const { return m_wrong; }
    QString question() const { return m_question; }
    QString message() const { return m_message; }
    int failures() const { return m_failures; }
    bool authorized() const { return m_authorized; }

    QString user() const { return m_user; }
    void setUser(const QString &user);

    static constexpr int maxFailures = 3;
    // closes as No after this long without a key or a click
    static constexpr int idleTimeoutMs = 2 * 60 * 1000;
    void setIdleInterval(int ms);

public Q_SLOTS:
    void accept(const QString &answer);
    void reject();
    // polkit took the request back (the app went away)
    void cancelFromPolkit();
    // any key or click: the idle timer starts again
    void touch();

Q_SIGNALS:
    void stateChanged();
    void messageChanged();
    void userChanged();
    // the answer for polkit; emitted once
    void finished(bool authorized);

private:
    void setState(State state);
    void finish(bool authorized);
    void dropSession();

    QString m_user;
    SessionFactory m_factory;
    AuthSession *m_session = nullptr;
    State m_state = Waiting;
    QString m_pending;
    bool m_hasPending = false;
    bool m_wrong = false;
    bool m_authorized = false;
    int m_failures = 0;
    QString m_question;
    QString m_message;
    QTimer m_idle;
};
