#include "listener.h"

#include "iconprovider.h"
#include "prompt.h"
#include "request.h"

#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkitagent/polkitagent.h>

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include <malloc.h>

#include <pwd.h>
#include <unistd.h>

#include <memory>

Q_LOGGING_CATEGORY(lcAgent, "raye.polkitagent", QtWarningMsg)

// ---- the GObject that polkit-agent-1 talks to -------------------------

struct RayeListener {
    PolkitAgentListener parent;
    Listener *owner;
};

struct RayeListenerClass {
    PolkitAgentListenerClass parent_class;
};

G_DEFINE_TYPE(RayeListener, raye_listener, POLKIT_AGENT_TYPE_LISTENER)

namespace
{

QString str(const gchar *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

void onCancelled(GCancellable *, gpointer task)
{
    auto *self = reinterpret_cast<RayeListener *>(g_task_get_source_object(G_TASK(task)));
    self->owner->cancel(task);
}

void initiateAuthentication(PolkitAgentListener *listener, const gchar *actionId, const gchar *message,
                            const gchar *iconName, PolkitDetails *details, const gchar *cookie, GList *identities,
                            GCancellable *cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    auto *self = reinterpret_cast<RayeListener *>(listener);
    GTask *task = g_task_new(listener, cancellable, callback, userData);

    Listener::Pending p;
    p.task = task;
    p.actionId = str(actionId);
    p.message = str(message);
    p.iconName = str(iconName);
    p.cookie = str(cookie);
    for (GList *l = identities; l; l = l->next) {
        auto *identity = POLKIT_IDENTITY(l->data);
        if (POLKIT_IS_UNIX_USER(identity)) {
            gint uid = polkit_unix_user_get_uid(POLKIT_UNIX_USER(identity));
            if (const passwd *pw = getpwuid(uid))
                p.users << QString::fromLocal8Bit(pw->pw_name);
        }
    }
    p.subjectPid = str(polkit_details_lookup(details, "polkit.subject-pid")).toLongLong();
    if (p.subjectPid <= 0)
        p.subjectPid = str(polkit_details_lookup(details, "polkit.caller-pid")).toLongLong();
    if (gchar **keys = polkit_details_get_keys(details)) {
        QStringList names;
        for (gchar **k = keys; *k; ++k)
            names << str(*k);
        g_strfreev(keys);
        qCDebug(lcAgent) << "request" << p.actionId << "details" << names;
    }
    p.program = str(polkit_details_lookup(details, "program"));
    p.command = str(polkit_details_lookup(details, "command_line"));
    if (p.program.isEmpty()) {
        const qint64 caller = str(polkit_details_lookup(details, "polkit.caller-pid")).toLongLong();
        const QStringList run = AppInfo::pkexecCommandOf(caller);
        if (!run.isEmpty()) {
            p.program = run.first().startsWith(QLatin1Char('/')) ? run.first()
                                                                  : QStandardPaths::findExecutable(run.first());
            if (p.program.isEmpty())
                p.program = run.first();
            p.command = run.join(QLatin1Char(' '));
        }
    }

    self->owner->add(p);
}

gboolean initiateAuthenticationFinish(PolkitAgentListener *, GAsyncResult *result, GError **error)
{
    return g_task_propagate_boolean(G_TASK(result), error);
}

} // namespace

static void raye_listener_init(RayeListener *self)
{
    self->owner = nullptr;
}

static void raye_listener_class_init(RayeListenerClass *klass)
{
    auto *listener = POLKIT_AGENT_LISTENER_CLASS(klass);
    listener->initiate_authentication = initiateAuthentication;
    listener->initiate_authentication_finish = initiateAuthenticationFinish;
}

// ---- one PAM conversation ---------------------------------------------

namespace
{

class PolkitSession : public AuthSession
{
public:
    PolkitSession(const QString &user, const QString &cookie, QObject *parent)
        : AuthSession(parent)
        , m_user(user)
        , m_cookie(cookie)
    {
    }

    ~PolkitSession() override { drop(); }

    void start() override
    {
        GError *error = nullptr;
        PolkitIdentity *identity = polkit_unix_user_new_for_name(m_user.toLocal8Bit().constData(), &error);
        if (!identity) {
            g_clear_error(&error);
            Q_EMIT finished(false);
            return;
        }
        m_session = polkit_agent_session_new(identity, m_cookie.toUtf8().constData());
        g_object_unref(identity);
        g_signal_connect(m_session, "request", G_CALLBACK(+[](PolkitAgentSession *, gchar *text, gboolean echo, gpointer self) {
                             Q_EMIT static_cast<PolkitSession *>(self)->question(str(text), echo);
                         }), this);
        g_signal_connect(m_session, "show-info", G_CALLBACK(+[](PolkitAgentSession *, gchar *text, gpointer self) {
                             Q_EMIT static_cast<PolkitSession *>(self)->info(str(text));
                         }), this);
        g_signal_connect(m_session, "show-error", G_CALLBACK(+[](PolkitAgentSession *, gchar *text, gpointer self) {
                             Q_EMIT static_cast<PolkitSession *>(self)->error(str(text));
                         }), this);
        g_signal_connect(m_session, "completed", G_CALLBACK(+[](PolkitAgentSession *, gboolean gained, gpointer self) {
                             auto *s = static_cast<PolkitSession *>(self);
                             s->drop();
                             Q_EMIT s->finished(gained);
                         }), this);
        polkit_agent_session_initiate(m_session);
    }

    void respond(const QString &answer) override
    {
        if (m_session)
            polkit_agent_session_response(m_session, answer.toUtf8().constData());
    }

    void cancel() override
    {
        if (!m_session)
            return;
        PolkitAgentSession *session = m_session;
        g_object_ref(session);
        polkit_agent_session_cancel(session); // emits "completed"
        g_object_unref(session);
    }

private:
    void drop()
    {
        if (!m_session)
            return;
        g_signal_handlers_disconnect_by_data(m_session, this);
        g_object_unref(m_session);
        m_session = nullptr;
    }

    QString m_user;
    QString m_cookie;
    PolkitAgentSession *m_session = nullptr;
};

QString selfName()
{
    const passwd *pw = getpwuid(getuid());
    return pw ? QString::fromLocal8Bit(pw->pw_name) : QString();
}

} // namespace

// ---- the agent ----------------------------------------------------------

Listener::Listener(QObject *parent)
    : QObject(parent)
{
    auto *gobject = static_cast<RayeListener *>(g_object_new(raye_listener_get_type(), nullptr));
    gobject->owner = this;
    m_gobject = gobject;
}

Listener::~Listener()
{
    if (m_registration)
        polkit_agent_listener_unregister(m_registration);
    g_object_unref(m_gobject);
}

bool Listener::registerAgent(QString *errorText)
{
    GError *error = nullptr;
    PolkitSubject *subject = polkit_unix_session_new_for_process_sync(QCoreApplication::applicationPid(), nullptr, &error);
    if (!subject) {
        *errorText = str(error ? error->message : nullptr);
        g_clear_error(&error);
        return false;
    }
    m_registration = polkit_agent_listener_register(POLKIT_AGENT_LISTENER(m_gobject), POLKIT_AGENT_REGISTER_FLAGS_NONE,
                                                    subject, "/org/raye/PolkitAgent", nullptr, &error);
    g_object_unref(subject);
    if (!m_registration) {
        *errorText = str(error ? error->message : nullptr);
        g_clear_error(&error);
        return false;
    }
    return true;
}

void Listener::add(const Pending &pending)
{
    m_queue << pending;
    GCancellable *cancellable = g_task_get_cancellable(G_TASK(pending.task));
    // runs onCancelled right away when polkit has already cancelled it
    if (cancellable) {
        const gulong id = g_cancellable_connect(cancellable, G_CALLBACK(onCancelled), pending.task, nullptr);
        for (Pending &p : m_queue) {
            if (p.task == pending.task)
                p.cancelHandler = id;
        }
    }
    if (m_queue.size() == 1 && m_queue.first().task == pending.task)
        next();
}

void Listener::next()
{
    if (m_queue.isEmpty() || m_prompt || m_queue.first().resolving)
        return;
    Pending &head = m_queue.first();
    if (head.users.isEmpty()) {
        finish(false, false);
        return;
    }

    // dpkg and apt take a moment: the app is looked up off the main
    // thread, where polkit's cancel must still get through
    head.resolving = true;
    auto caller = std::make_shared<AppInfo::Caller>();
    const qint64 pid = head.subjectPid;
    const QString program = head.program;
    const QString command = head.command;
    void *task = head.task;
    QThread *thread = QThread::create([caller, pid, program, command] {
        *caller = AppInfo::resolve(pid, program, command);
    });
    connect(thread, &QThread::finished, this, [this, thread, caller, task] {
        thread->deleteLater();
        if (m_queue.isEmpty() || m_queue.first().task != task)
            return;
        m_queue.first().resolving = false;
        if (g_task_return_error_if_cancelled(G_TASK(task))) {
            // polkit took it back while we looked: the answer is given
            g_object_unref(task);
            m_queue.first().task = nullptr;
            finish(false, true);
            return;
        }
        open(*caller);
    });
    thread->start();
}

void Listener::open(const AppInfo::Caller &caller)
{
    const Pending &head = m_queue.first();
    const QString self = selfName();
    // the person signed in first, then the others in polkit's order
    QStringList order = head.users;
    if (order.removeAll(self) > 0)
        order.prepend(self);
    QList<Prompt::Account> accounts;
    for (const QString &u : std::as_const(order))
        accounts << Prompt::account(u);

    const QString cookie = head.cookie;
    auto *request = new Request(Prompt::preferredUser(order, self), [cookie](const QString &user, QObject *parent) {
        return new PolkitSession(user, cookie, parent);
    });
    connect(request, &Request::finished, this, [this, request](bool authorized) {
        // three wrong passwords end as "not authorized", No as "dismissed"
        finish(authorized, !authorized && request->failures() < Request::maxFailures);
    });

    AppInfo::Caller shown = caller;
    if (shown.icon.isEmpty())
        shown.icon = head.iconName;
    m_prompt = new Prompt(shown, head.actionId, head.message, accounts, request, this);
    QQmlEngine *qml = engine();
    // "Animation speed" all the way to instant in Settings
    const QSettings kdeglobals(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                                   + QStringLiteral("/kdeglobals"),
                               QSettings::IniFormat);
    const bool still = kdeglobals.value(QStringLiteral("KDE/AnimationDurationFactor"), 1.0).toDouble() == 0.0;
    if (QObject *motion = qml->singletonInstance<QObject *>(QStringLiteral("org.raye.signin"), QStringLiteral("Motion")))
        motion->setProperty("reducedMotion", still);
    m_prompt->show(qml);
}

void Listener::finish(bool authorized, bool dismissed)
{
    if (m_prompt) {
        m_prompt->hide();
        m_prompt->deleteLater();
        m_prompt = nullptr;
    }
    if (m_queue.isEmpty())
        return;
    const Pending head = m_queue.takeFirst();
    if (head.task) {
        GTask *task = G_TASK(head.task);
        if (head.cancelHandler)
            g_cancellable_disconnect(g_task_get_cancellable(task), head.cancelHandler);
        // polkitd learns from its helper whether the password was right;
        // the agent says it is done, or that the person said No
        if (dismissed && !authorized)
            g_task_return_new_error(task, POLKIT_ERROR, POLKIT_ERROR_CANCELLED, "The request was dismissed");
        else
            g_task_return_boolean(task, TRUE);
        g_object_unref(task);
    }
    if (m_queue.isEmpty())
        release();
    next();
}

void Listener::cancel(void *task)
{
    for (qsizetype i = 0; i < m_queue.size(); ++i) {
        Pending &p = m_queue[i];
        if (p.task != task)
            continue;
        // called from the cancellable's own handler: disconnecting from
        // inside it would wait for itself
        p.cancelHandler = 0;
        if (i > 0) {
            g_task_return_new_error(G_TASK(task), POLKIT_ERROR, POLKIT_ERROR_CANCELLED, "The request was cancelled");
            g_object_unref(task);
            m_queue.removeAt(i);
        } else if (m_prompt) {
            m_prompt->request()->cancelFromPolkit();
        } else if (!p.resolving) {
            finish(false, true);
        }
        // still looking the app up: handled when the lookup ends
        return;
    }
}

// Prompts are rare: between them the agent gives back the QML engine and
// the memory the windows used, and builds them again for the next one.
void Listener::release()
{
    if (m_engine) {
        m_engine->deleteLater(); // after the windows, deleted just before
        m_engine = nullptr;
    }
    QTimer::singleShot(1000, this, [this] {
        if (!m_prompt)
            malloc_trim(0);
    });
}

QQmlEngine *Listener::engine()
{
    if (!m_engine) {
        m_engine = new QQmlEngine(this);
        m_engine->addImageProvider(QStringLiteral("icon"), new IconProvider);
    }
    return m_engine;
}
