// SPDX-License-Identifier: GPL-3.0-or-later

#include "activeoutput.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QGuiApplication>

using namespace Qt::StringLiterals;

int ActiveOutputLogic::pick(const QStringList &names, const QString &active, int fallback)
{
    if (active.isEmpty())
        return fallback;
    const qsizetype at = names.indexOf(active);
    return at >= 0 ? int(at) : fallback;
}

ActiveOutput::ActiveOutput(QObject *parent)
    : QObject(parent)
{
}

QString ActiveOutput::kwinActiveOutput()
{
    QDBusMessage call = QDBusMessage::createMethodCall(u"org.kde.KWin"_s, u"/KWin"_s, u"org.kde.KWin"_s, u"activeOutputName"_s);
    const QDBusReply<QString> reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 200);
    return reply.isValid() ? reply.value() : QString();
}

QScreen *ActiveOutput::screen() const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
        return nullptr;
    QStringList names;
    for (const QScreen *s : screens)
        names << s->name();
    const int primary = int(screens.indexOf(QGuiApplication::primaryScreen()));
    const int at = ActiveOutputLogic::pick(names, kwinActiveOutput(), primary < 0 ? 0 : primary);
    return screens.at(at);
}

QRect ActiveOutput::geometry() const
{
    const QScreen *s = screen();
    return s ? s->geometry() : QRect();
}
