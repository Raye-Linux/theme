#include "klipperbackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>

namespace
{
const QString s_service = QStringLiteral("org.kde.klipper");
const QString s_path = QStringLiteral("/klipper");
const QString s_interface = QStringLiteral("org.kde.klipper.klipper");
constexpr int s_timeout = 2000;

QDBusMessage call(const QString &method, const QVariantList &args = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(s_service, s_path, s_interface, method);
    message.setArguments(args);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, s_timeout);
}
} // namespace

KlipperBackend::KlipperBackend(QObject *parent)
    : ClipboardBackend(parent)
{
    QDBusConnection::sessionBus().connect(s_service, s_path, s_interface, QStringLiteral("clipboardHistoryUpdated"), this, SIGNAL(changed()));
}

bool KlipperBackend::available() const
{
    const QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    return bus && bus->isServiceRegistered(s_service);
}

QStringList KlipperBackend::history() const
{
    const QDBusReply<QStringList> reply = call(QStringLiteral("getClipboardHistoryMenu"));
    return reply.isValid() ? reply.value() : QStringList();
}

QString KlipperBackend::current() const
{
    const QDBusReply<QString> reply = call(QStringLiteral("getClipboardContents"));
    return reply.isValid() ? reply.value() : QString();
}

void KlipperBackend::setCurrent(const QString &text)
{
    call(QStringLiteral("setClipboardContents"), {text});
}

void KlipperBackend::clear()
{
    call(QStringLiteral("clearClipboardHistory"));
}
