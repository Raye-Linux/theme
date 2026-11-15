#include "prompt.h"

#include <LayerShellQt/Window>

#include <QCursor>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickView>
#include <QScreen>

#include <pwd.h>
#include <unistd.h>

Prompt::Prompt(const AppInfo::Caller &caller, const QString &actionId, const QString &message,
               const QList<Account> &accounts, Request *request, QObject *parent)
    : QObject(parent)
    , m_caller(caller)
    , m_actionId(actionId)
    , m_message(message)
    , m_accounts(accounts)
    , m_request(request)
{
    m_request->setParent(this);
    connect(m_request, &Request::userChanged, this, &Prompt::userChanged);
    if (const passwd *pw = getpwuid(getuid()))
        m_self = QString::fromLocal8Bit(pw->pw_name);
}

Prompt::~Prompt()
{
    hide();
}

QVariantList Prompt::users() const
{
    QVariantList list;
    for (const Account &a : m_accounts)
        list << QVariantMap{{QStringLiteral("user"), a.user}, {QStringLiteral("name"), a.name}, {QStringLiteral("face"), a.face}};
    return list;
}

void Prompt::setUser(const QString &user)
{
    for (const Account &a : std::as_const(m_accounts)) {
        if (a.user == user) {
            m_request->setUser(user);
            return;
        }
    }
}

void Prompt::setSelf(const QString &self)
{
    m_self = self;
    Q_EMIT userChanged();
}

QString Prompt::preferredUser(const QStringList &users, const QString &self)
{
    if (users.contains(self))
        return self;
    return users.value(0);
}

Prompt::Account Prompt::account(const QString &user)
{
    Account a{user, user, QString()};
    const QByteArray name = user.toLocal8Bit();
    if (const passwd *pw = getpwnam(name.constData())) {
        const QString full = QString::fromLocal8Bit(pw->pw_gecos).section(QLatin1Char(','), 0, 0).trimmed();
        if (!full.isEmpty())
            a.name = full;
        const QString home = QString::fromLocal8Bit(pw->pw_dir);
        for (const QString &face : {QStringLiteral("/var/lib/AccountsService/icons/") + user,
                                    home + QStringLiteral("/.face.icon"), home + QStringLiteral("/.face")}) {
            if (QFileInfo(face).isReadable() && QFileInfo(face).size() > 0) {
                a.face = face;
                break;
            }
        }
    }
    return a;
}

QScreen *Prompt::activeScreen() const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        // KWin's active output: where new windows and its own popups go (the
        // screen of the pointer, or of the active window when Plasma is set
        // so). A Wayland client cannot learn either itself.
        QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"));
        const QDBusReply<QString> reply = kwin.call(QStringLiteral("activeOutputName"));
        const QString output = reply.isValid() ? reply.value() : QString();
        for (QScreen *s : screens) {
            if (s->name() == output)
                return s;
        }
    } else if (QScreen *s = QGuiApplication::screenAt(QCursor::pos())) {
        return s;
    }
    return QGuiApplication::primaryScreen();
}

void Prompt::show(QQmlEngine *engine)
{
    QScreen *active = activeScreen();
    const QList<QScreen *> screens = QGuiApplication::screens();
    // the card first, so it gets the keyboard
    addWindow(engine, active, true);
    for (QScreen *s : screens) {
        if (s != active)
            addWindow(engine, s, false);
    }
}

void Prompt::addWindow(QQmlEngine *engine, QScreen *screen, bool card)
{
    auto *view = new QQuickView(engine, nullptr);
    view->setScreen(screen);
    view->setColor(Qt::transparent);
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setTitle(QStringLiteral("raye-polkit-agent"));
    view->setInitialProperties({{QStringLiteral("prompt"), QVariant::fromValue(this)},
                                {QStringLiteral("showCard"), card}});
    view->setSource(QUrl(QStringLiteral("qrc:/qt/qml/org/raye/polkitagent/Prompt.qml")));

    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        auto *layer = LayerShellQt::Window::get(view);
        layer->setScope(QStringLiteral("raye-polkit-agent"));
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                        | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
        layer->setExclusiveZone(-1);
        layer->setScreen(screen);
        layer->setKeyboardInteractivity(card ? LayerShellQt::Window::KeyboardInteractivityExclusive
                                             : LayerShellQt::Window::KeyboardInteractivityNone);
        view->resize(screen->size());
        view->show();
    } else {
        view->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        view->setGeometry(screen->geometry());
        view->show();
    }
    if (card)
        view->requestActivate();
    m_windows << view;
}

void Prompt::hide()
{
    for (const QPointer<QQuickView> &w : std::as_const(m_windows)) {
        if (w) {
            w->hide();
            w->deleteLater();
        }
    }
    m_windows.clear();
}
