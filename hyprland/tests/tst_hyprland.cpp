// The Hyprland link against a stand-in for Hyprland's two sockets: what it
// asks, what it makes of the answers, and the events that make it ask again.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hyprland.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class FakeHyprland : public QObject
{
    Q_OBJECT
public:
    explicit FakeHyprland(const QString &dir)
    {
        requests.listen(dir + QStringLiteral("/.socket.sock"));
        events.listen(dir + QStringLiteral("/.socket2.sock"));
        connect(&requests, &QLocalServer::newConnection, this, [this] {
            QLocalSocket *s = requests.nextPendingConnection();
            connect(s, &QLocalSocket::readyRead, s, [this, s] {
                const QByteArray q = s->readAll();
                asked << q;
                s->write(answers.value(q, QByteArray("ok")));
                s->flush();
                s->disconnectFromServer();
            });
        });
        connect(&events, &QLocalServer::newConnection, this, [this] {
            listener = events.nextPendingConnection();
        });
    }
    void send(const QByteArray &line)
    {
        listener->write(line + '\n');
        listener->flush();
    }

    QLocalServer requests;
    QLocalServer events;
    QLocalSocket *listener = nullptr;
    QHash<QByteArray, QByteArray> answers;
    QList<QByteArray> asked;
};

class TestHyprland : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        qputenv("RAYE_HYPR_SOCKET_DIR", m_dir.path().toUtf8());
        m_fake = std::make_unique<FakeHyprland>(m_dir.path());
        m_fake->answers.insert("j/clients",
                               R"([{"address":"0xa","class":"firefox","title":"Web","workspace":{"id":2,"name":"2"},"mapped":true}])");
        m_fake->answers.insert("j/activewindow", R"({"address":"0xa"})");
        m_fake->answers.insert("j/monitors",
                               R"([{"name":"DP-1","focused":false,"activeWorkspace":{"id":1}},{"name":"HDMI-A-1","focused":true,"activeWorkspace":{"id":2}}])");
    }
    void cleanup()
    {
        m_fake.reset();
    }

    void readsClientsWhenConnected()
    {
        Hyprland h;
        QSignalSpy spy(&h, &Hyprland::clientsChanged);
        QVERIFY(spy.wait(2000));
        QVERIFY(h.available());
        QCOMPARE(h.clients().size(), 1);
        QCOMPARE(h.clients().first().toMap().value(QStringLiteral("class")).toString(), QStringLiteral("firefox"));
        QCOMPARE(h.activeAddress(), QStringLiteral("0xa"));
        QCOMPARE(h.activeWorkspace(), 2);
    }

    void windowEventReadsAgain()
    {
        Hyprland h;
        QSignalSpy spy(&h, &Hyprland::clientsChanged);
        QVERIFY(spy.wait(2000));
        QSignalSpy events(&h, &Hyprland::event);
        m_fake->answers.insert("j/clients", "[]");
        m_fake->send("openwindow>>b,1,kitty,Term");
        QVERIFY(spy.wait(2000));
        QCOMPARE(h.clients().size(), 0);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().at(0).toString(), QStringLiteral("openwindow"));
        QCOMPARE(events.first().at(1).toString(), QStringLiteral("b,1,kitty,Term"));
    }

    void otherEventsDoNotReadAgain()
    {
        Hyprland h;
        QSignalSpy spy(&h, &Hyprland::clientsChanged);
        QVERIFY(spy.wait(2000));
        m_fake->asked.clear();
        m_fake->send("activelayout>>kbd,French");
        QVERIFY(!spy.wait(300));
        QVERIFY(m_fake->asked.isEmpty());
    }

    void eventSplitOverTwoReads()
    {
        Hyprland h;
        QSignalSpy spy(&h, &Hyprland::clientsChanged);
        QVERIFY(spy.wait(2000));
        QSignalSpy events(&h, &Hyprland::event);
        m_fake->listener->write("closewin");
        m_fake->listener->flush();
        QTest::qWait(50);
        m_fake->send("dow>>a");
        QVERIFY(events.wait(2000));
        QCOMPARE(events.first().at(0).toString(), QStringLiteral("closewindow"));
    }

    void requestIsSentAsIs()
    {
        Hyprland h;
        QSignalSpy spy(&h, &Hyprland::clientsChanged);
        QVERIFY(spy.wait(2000));
        h.request(QStringLiteral("[[BATCH]]dispatch a;dispatch b"));
        QTRY_VERIFY(m_fake->asked.contains("[[BATCH]]dispatch a;dispatch b"));
    }

    void focusedMonitor()
    {
        Hyprland h;
        QSignalSpy spy(&h, &Hyprland::monitorsChanged);
        QVERIFY(spy.wait(2000));
        QCOMPARE(h.focusedMonitor().value(QStringLiteral("name")).toString(), QStringLiteral("HDMI-A-1"));
    }

    void noHyprland()
    {
        QTemporaryDir empty;
        qputenv("RAYE_HYPR_SOCKET_DIR", empty.path().toUtf8());
        Hyprland h;
        QTest::qWait(200);
        QVERIFY(!h.available());
        QVERIFY(h.activeWindow().isEmpty());
    }

private:
    QTemporaryDir m_dir;
    std::unique_ptr<FakeHyprland> m_fake;
};

QTEST_GUILESS_MAIN(TestHyprland)
#include "tst_hyprland.moc"
