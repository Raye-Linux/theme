#include "request.h"

Request::Request(const QString &user, SessionFactory factory, QObject *parent)
    : QObject(parent)
    , m_user(user)
    , m_factory(std::move(factory))
{
    m_idle.setSingleShot(true);
    m_idle.setInterval(idleTimeoutMs);
    connect(&m_idle, &QTimer::timeout, this, &Request::reject);
    m_idle.start();
}

Request::~Request()
{
    if (m_session) {
        m_session->disconnect(this);
        m_session->cancel();
    }
}

void Request::setUser(const QString &user)
{
    if (user == m_user || m_state == Done)
        return;
    // another administrator: what PAM had started was for the first one
    dropSession();
    m_user = user;
    m_wrong = false;
    m_question.clear();
    setState(Waiting);
    Q_EMIT userChanged();
}

void Request::setIdleInterval(int ms)
{
    m_idle.setInterval(ms);
    touch();
}

void Request::touch()
{
    if (m_state != Done)
        m_idle.start();
}

void Request::accept(const QString &answer)
{
    touch();
    if (m_state == Asking && m_session) {
        m_question.clear();
        setState(Checking);
        m_session->respond(answer);
        return;
    }
    if (m_state != Waiting)
        return;

    m_pending = answer;
    m_hasPending = true;
    m_wrong = false;
    if (!m_message.isEmpty()) {
        m_message.clear();
        Q_EMIT messageChanged();
    }
    setState(Checking);

    m_session = m_factory(m_user, this);
    AuthSession *session = m_session;
    connect(session, &AuthSession::question, this, [this, session](const QString &text, bool echo) {
        if (session != m_session)
            return;
        if (m_hasPending && !echo) {
            const QString answer = m_pending;
            m_pending.clear();
            m_hasPending = false;
            session->respond(answer);
            return;
        }
        m_question = text;
        setState(Asking);
    });
    connect(session, &AuthSession::info, this, [this, session](const QString &text) {
        if (session != m_session)
            return;
        m_message = text;
        Q_EMIT messageChanged();
    });
    connect(session, &AuthSession::error, this, [this, session](const QString &text) {
        if (session != m_session)
            return;
        m_message = text;
        Q_EMIT messageChanged();
    });
    connect(session, &AuthSession::finished, this, [this, session](bool authorized) {
        if (session != m_session)
            return;
        m_pending.clear();
        m_hasPending = false;
        m_session = nullptr;
        session->deleteLater();
        if (authorized) {
            finish(true);
            return;
        }
        ++m_failures;
        if (m_failures >= maxFailures) {
            finish(false);
            return;
        }
        m_wrong = true;
        m_question.clear();
        // PAM's own words ("Authentication failure") say less than ours
        m_message.clear();
        Q_EMIT messageChanged();
        setState(Waiting);
    });
    session->start();
}

void Request::reject()
{
    if (m_state == Done)
        return;
    dropSession();
    finish(false);
}

void Request::cancelFromPolkit()
{
    reject();
}

void Request::setState(State state)
{
    if (state == m_state && state != Waiting)
        return;
    m_state = state;
    Q_EMIT stateChanged();
}

void Request::finish(bool authorized)
{
    if (m_state == Done)
        return;
    m_idle.stop();
    m_pending.clear();
    m_hasPending = false;
    m_authorized = authorized;
    setState(Done);
    Q_EMIT finished(authorized);
}

void Request::dropSession()
{
    if (!m_session)
        return;
    AuthSession *session = m_session;
    m_session = nullptr;
    session->disconnect(this);
    session->cancel();
    session->deleteLater();
    m_pending.clear();
    m_hasPending = false;
}
