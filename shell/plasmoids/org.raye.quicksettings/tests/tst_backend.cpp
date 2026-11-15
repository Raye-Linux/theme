// Quick settings' Backend.qml on stand-ins for the system services
// (mock/): the VPN list keeps only saved VPNs, the network page lists the
// cable and the Wi-Fi networks, Bluetooth its paired devices and sound its
// real outputs, and the brightness slider reaches the screen. These are the parts that read the models' roles, so
// the panel tests, which run on FakeBackend.qml, never see them.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QStandardItemModel>
#include <QTest>

using namespace Qt::StringLiterals;

namespace
{
// a role looked up on the wrong object only says so in a warning
QStringList s_roleWarnings;
QtMessageHandler s_previous = nullptr;

void watchWarnings(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (message.contains(u"KRoleNames"_s))
        s_roleWarnings << message;
    s_previous(type, context, message);
}
}

class TestBackend : public QObject
{
    Q_OBJECT

    std::unique_ptr<QQmlEngine> m_engine;
    // one screen whose brightness goes to 1000, set at 300
    QStandardItemModel m_screens;
    std::unique_ptr<QObject> m_backend;

    QObject *child(const char *type) const
    {
        const auto all = m_backend->findChildren<QObject *>();
        for (QObject *o : all) {
            if (QByteArray(o->metaObject()->className()).startsWith(type))
                return o;
        }
        return nullptr;
    }

    // the sink the volume goes to, as the sound server gives it
    void setSink(bool muted)
    {
        QQmlComponent c(m_engine.get());
        c.setData(QByteArrayLiteral("import QtQuick\nimport org.kde.plasma.private.volume\nQtObject { Component.onCompleted: PreferredDevice.sink = ({ index: 4, name: 'speakers', volume: 0, muted: ")
                      + (muted ? "true" : "false") + "}) }",
                  QUrl());
        std::unique_ptr<QObject> o(c.create());
        QVERIFY2(o, qPrintable(c.errorString()));
    }

    QVariantList plays() const
    {
        QObject *feedback = child("VolumeFeedback");
        return feedback ? feedback->property("plays").toList() : QVariantList{QStringLiteral("no feedback")};
    }

private Q_SLOTS:
    void initTestCase()
    {
        m_screens.setItemRoleNames({{Qt::UserRole + 1, "displayName"}, {Qt::UserRole + 2, "brightness"}, {Qt::UserRole + 3, "maxBrightness"}});
        auto *screen = new QStandardItem;
        screen->setData(u"eDP-1"_s, Qt::UserRole + 1);
        screen->setData(300, Qt::UserRole + 2);
        screen->setData(1000, Qt::UserRole + 3);
        m_screens.appendRow(screen);
    }

    void init()
    {
        s_roleWarnings.clear();
        s_previous = qInstallMessageHandler(watchWarnings);
        m_engine = std::make_unique<QQmlEngine>();
        m_engine->addImportPath(QStringLiteral(RAYE_QS_TESTS "/mock"));
        m_engine->addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        m_engine->addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        m_engine->rootContext()->setContextProperty(u"screenList"_s, &m_screens);
        QQmlComponent c(m_engine.get(), QUrl::fromLocalFile(QStringLiteral(RAYE_QS_DIR "/contents/ui/Backend.qml")));
        m_backend.reset(c.create());
        QVERIFY2(m_backend, qPrintable(c.errorString()));
    }

    void cleanup()
    {
        qInstallMessageHandler(s_previous);
        m_backend.reset();
        m_engine.reset();
    }

    void volumeSoundOnTheChosenOutput()
    {
        setSink(false);
        QVERIFY(QMetaObject::invokeMethod(m_backend.get(), "setVolume", Q_ARG(QVariant, 40)));
        QCOMPARE(plays(), QVariantList({4}));
        // the slider: nothing while it moves, one sound when let go
        QTest::qWait(700);
        QVERIFY(QMetaObject::invokeMethod(m_backend.get(), "holdVolume"));
        for (int v = 41; v < 60; ++v)
            QVERIFY(QMetaObject::invokeMethod(m_backend.get(), "setVolume", Q_ARG(QVariant, v)));
        QCOMPARE(plays().size(), 1);
        QVERIFY(QMetaObject::invokeMethod(m_backend.get(), "releaseVolume"));
        QCOMPARE(plays(), QVariantList({4, 4}));
    }

    void noVolumeSoundWhenMutedOrAtZero()
    {
        setSink(true);
        QVERIFY(QMetaObject::invokeMethod(m_backend.get(), "setVolume", Q_ARG(QVariant, 0)));
        QTest::qWait(300);
        QCOMPARE(plays(), QVariantList());
    }

    void vpnListHasTheSavedVpnsOnly()
    {
        QStringList names;
        QTRY_VERIFY_WITH_TIMEOUT(m_backend->property("vpnAvailable").toBool(), 2000);
        const QVariantList list = m_backend->property("vpnConnections").toList();
        for (const QVariant &v : list)
            names << v.toMap().value(u"name"_s).toString();
        QCOMPARE(names, (QStringList{u"Office"_s, u"Tunnel"_s}));
        QVERIFY(m_backend->property("vpnOn").toBool());
        QCOMPARE(s_roleWarnings, QStringList());
    }

    void theNetworkListPutsTheCableFirstAndSaysEthernet()
    {
        QTRY_VERIFY_WITH_TIMEOUT(m_backend->property("networkConnections").toList().size() == 3, 2000);
        const QVariantList list = m_backend->property("networkConnections").toList();
        QStringList names;
        for (const QVariant &v : list)
            names << v.toMap().value(u"name"_s).toString();
        // the cable, then the strongest Wi-Fi; VPNs are not on this page
        QCOMPARE(names, (QStringList{u"Wired connection 1"_s, u"Cafe"_s, u"Home"_s}));
        QCOMPARE(list.at(0).toMap().value(u"kind"_s).toString(), u"wired"_s);
        QCOMPARE(list.at(0).toMap().value(u"state"_s).toString(), u"active"_s);
        // read from the details NetworkManager gives for the connection
        QTRY_COMPARE_WITH_TIMEOUT(m_backend->property("networkConnections").toList().at(0).toMap().value(u"address"_s).toString(),
                                  u"192.168.50.50"_s, 2000);
        // a network never used asks for its key, a saved one does not
        QVERIFY(list.at(1).toMap().value(u"askPassword"_s).toBool());
        QVERIFY(!list.at(2).toMap().value(u"askPassword"_s).toBool());
        QVERIFY(list.at(2).toMap().value(u"secured"_s).toBool());
        QCOMPARE(m_backend->property("networkState").toMap().value(u"kind"_s).toString(), u"wired"_s);
        QCOMPARE(s_roleWarnings, QStringList());
    }

    void connectingGoesThroughTheRightCall()
    {
        QTRY_VERIFY_WITH_TIMEOUT(m_backend->property("networkConnections").toList().size() == 3, 2000);
        QObject *handler = child("Handler");
        QVERIFY(handler);
        QVERIFY(QMetaObject::invokeMethod(m_backend.get(), "connectNetwork", Q_ARG(QVariant, 1), Q_ARG(QVariant, u"secret-key"_s)));
        QVERIFY(QMetaObject::invokeMethod(m_backend.get(), "connectNetwork", Q_ARG(QVariant, 2), Q_ARG(QVariant, QString())));
        QVERIFY(QMetaObject::invokeMethod(m_backend.get(), "connectNetwork", Q_ARG(QVariant, 0), Q_ARG(QVariant, QString())));
        QCOMPARE(handler->property("calls").toStringList(),
                 (QStringList{u"add:/ap/2:with-password"_s, u"activate:/wifi/1"_s, u"deactivate:/wired/1"_s}));
    }

    void bluetoothListsThePairedDevicesConnectedFirst()
    {
        QTRY_VERIFY_WITH_TIMEOUT(m_backend->property("bluetoothDevices").toList().size() == 2, 2000);
        const QVariantList list = m_backend->property("bluetoothDevices").toList();
        QCOMPARE(list.at(0).toMap().value(u"name"_s).toString(), u"Headphones"_s);
        QVERIFY(list.at(0).toMap().value(u"connected"_s).toBool());
        QCOMPARE(list.at(1).toMap().value(u"name"_s).toString(), u"Mouse"_s);
    }

    void soundListsTheRealOutputs()
    {
        QTRY_VERIFY_WITH_TIMEOUT(m_backend->property("outputs").toList().size() == 2, 2000);
        const QVariantList list = m_backend->property("outputs").toList();
        QCOMPARE(list.at(0).toMap().value(u"name"_s).toString(), u"Speakers"_s);
        QVERIFY(list.at(0).toMap().value(u"active"_s).toBool());
        QCOMPARE(list.at(1).toMap().value(u"name"_s).toString(), u"Headphones"_s);
        QCOMPARE(s_roleWarnings, QStringList());
    }

    void brightnessReachesTheScreen()
    {
        QTRY_VERIFY_WITH_TIMEOUT(m_backend->property("brightnessAvailable").toBool(), 2000);
        QCOMPARE(m_backend->property("brightness").toInt(), 30);
        QObject *screen = child("ScreenBrightnessControl");
        QVERIFY(screen);
        QVERIFY(QMetaObject::invokeMethod(m_backend.get(), "setBrightness", Q_ARG(QVariant, 50)));
        QCOMPARE(screen->property("calls").toStringList(), QStringList{u"eDP-1=500"_s});
        QCOMPARE(s_roleWarnings, QStringList());
    }
};

QTEST_MAIN(TestBackend)
#include "tst_backend.moc"
