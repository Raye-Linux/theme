// raye-snap-relay: owns org.raye.Snap on the session bus for Raye Snap.
//
// Resting the pointer on a window's Maximize button makes the decoration
// call org.raye.Snap.showLayouts(window, x, y) without waiting. A KWin
// script cannot own a D-Bus name, so this relay turns the call into the
// script's "Raye Snap Layouts Under Pointer" shortcut, again without
// waiting: kglobalaccel lives inside KWin, which must never block on us.
// The script finds the window and the place under the pointer itself, so
// the arguments are not needed. Started by D-Bus when the script loads.
//
// It also gives the script Windows' arrow keys, once per account: KWin's own
// quick tiling holds Meta+Left/Right/Up/Down from the start, so the
// script's shortcuts are left without a key. A user who gives them back to
// quick tiling afterwards keeps that.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusServiceWatcher>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>

using namespace Qt::StringLiterals;

class SnapRelay : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.raye.Snap")

public:
    using QObject::QObject;

public Q_SLOTS:
    void showLayouts(const QString &window, int x, int y)
    {
        Q_UNUSED(window)
        Q_UNUSED(x)
        Q_UNUSED(y)
        // one flyout per rest on the button: drop repeats
        if (m_last.isValid() && m_last.elapsed() < 250)
            return;
        m_last.start();
        QDBusMessage call = QDBusMessage::createMethodCall(u"org.kde.kglobalaccel"_s, u"/component/kwin"_s,
                                                           u"org.kde.kglobalaccel.Component"_s, u"invokeShortcut"_s);
        call.setArguments({u"Raye Snap Layouts Under Pointer"_s});
        call.setAutoStartService(false);
        QDBusConnection::sessionBus().send(call);
    }

private:
    QElapsedTimer m_last;
};

// A key sequence as kglobalaccel sends it: a struct of four combined keys.
// Only one-key sequences are used here.
struct KeySequence {
    QList<int> keys;
};
Q_DECLARE_METATYPE(KeySequence)

static QDBusArgument &operator<<(QDBusArgument &arg, const KeySequence &sequence)
{
    arg.beginStructure();
    arg << sequence.keys;
    arg.endStructure();
    return arg;
}

static const QDBusArgument &operator>>(const QDBusArgument &arg, KeySequence &sequence)
{
    arg.beginStructure();
    arg >> sequence.keys;
    arg.endStructure();
    return arg;
}

static QStringList actionId(const QString &name)
{
    return {u"kwin"_s, name, u"KWin"_s, QString()};
}

static QList<int> keysOf(const QString &name)
{
    QDBusMessage call = QDBusMessage::createMethodCall(u"org.kde.kglobalaccel"_s, u"/kglobalaccel"_s,
                                                       u"org.kde.KGlobalAccel"_s, u"shortcutKeys"_s);
    call.setArguments({actionId(name)});
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 5000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};
    QList<KeySequence> sequences;
    reply.arguments().constFirst().value<QDBusArgument>() >> sequences;
    QList<int> keys;
    for (const KeySequence &s : std::as_const(sequences)) {
        if (!s.keys.isEmpty() && s.keys.first() != 0)
            keys << s.keys.first();
    }
    return keys;
}

static void setKeys(const QString &name, const QList<int> &keys)
{
    QDBusMessage call = QDBusMessage::createMethodCall(u"org.kde.kglobalaccel"_s, u"/kglobalaccel"_s,
                                                       u"org.kde.KGlobalAccel"_s, u"setForeignShortcutKeys"_s);
    QList<KeySequence> sequences;
    for (int key : keys)
        sequences << KeySequence{{key, 0, 0, 0}};
    call.setArguments({actionId(name), QVariant::fromValue(sequences)});
    QDBusConnection::sessionBus().call(call, QDBus::Block, 5000);
}

static void takeArrowKeys()
{
    const QString stateDir = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + u"/raye"_s;
    QFile done(stateDir + u"/snap-arrow-keys"_s);
    if (done.exists())
        return;
    // the script's shortcuts must be there to take the keys
    QDBusMessage names = QDBusMessage::createMethodCall(u"org.kde.kglobalaccel"_s, u"/component/kwin"_s,
                                                        u"org.kde.kglobalaccel.Component"_s, u"shortcutNames"_s);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(names, QDBus::Block, 5000);
    if (reply.arguments().isEmpty() || !reply.arguments().constFirst().toStringList().contains(u"Raye Snap Left"_s))
        return;
    struct Move {
        QString from;
        QString to;
        int key;
    };
    const Move moves[] = {
        {u"Window Quick Tile Left"_s, u"Raye Snap Left"_s, (Qt::META | Qt::Key_Left).toCombined()},
        {u"Window Quick Tile Right"_s, u"Raye Snap Right"_s, (Qt::META | Qt::Key_Right).toCombined()},
        {u"Window Quick Tile Top"_s, u"Raye Snap Up"_s, (Qt::META | Qt::Key_Up).toCombined()},
        {u"Window Quick Tile Bottom"_s, u"Raye Snap Down"_s, (Qt::META | Qt::Key_Down).toCombined()},
    };
    for (const Move &m : moves) {
        QList<int> held = keysOf(m.from);
        if (!held.contains(m.key) || !keysOf(m.to).isEmpty())
            continue;
        held.removeAll(m.key);
        setKeys(m.from, held);
        setKeys(m.to, {m.key});
    }
    if (QDir().mkpath(stateDir))
        (void)done.open(QIODevice::WriteOnly);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    qDBusRegisterMetaType<KeySequence>();
    qDBusRegisterMetaType<QList<KeySequence>>();
    QDBusConnection bus = QDBusConnection::sessionBus();
    SnapRelay relay;
    if (!bus.registerObject(u"/Snap"_s, &relay, QDBusConnection::ExportAllSlots))
        return 1;
    // another relay already runs: nothing to do
    if (!bus.registerService(u"org.raye.Snap"_s))
        return 0;
    // gone with KWin (logout, or a KWin restart: the script wakes a new
    // relay when it loads again), and with the session bus
    QDBusServiceWatcher kwin(u"org.kde.KWin"_s, bus, QDBusServiceWatcher::WatchForUnregistration);
    QObject::connect(&kwin, &QDBusServiceWatcher::serviceUnregistered, &app, &QCoreApplication::quit);
    QTimer busCheck;
    QObject::connect(&busCheck, &QTimer::timeout, &app, [&bus] {
        if (!bus.isConnected())
            QCoreApplication::quit();
    });
    busCheck.start(10000);
    // after the Ping that started us has its answer
    QTimer::singleShot(0, &app, takeArrowKeys);
    return app.exec();
}

#include "main.moc"
