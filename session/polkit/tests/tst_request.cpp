#include "request.h"

#include <QPointer>
#include <QSignalSpy>
#include <QTest>

// Stands for PAM: remembers what it was told, and the test plays PAM's part.
class FakeSession : public AuthSession
{
    Q_OBJECT
public:
    explicit FakeSession(const QString &user, QObject *parent)
        : AuthSession(parent)
        , user(user)
    {
    }

    void start() override { started = true; }
    void respond(const QString &answer) override { answers << answer; }
    void cancel() override
    {
        cancelled = true;
        Q_EMIT finished(false);
    }

    QString user;
    bool started = false;
    bool cancelled = false;
    QStringList answers;
};

class TestRequest : public QObject
{
    Q_OBJECT

    QList<QPointer<FakeSession>> sessions;

    Request::SessionFactory factory()
    {
        return [this](const QString &user, QObject *parent) {
            auto *s = new FakeSession(user, parent);
            sessions << s;
            return s;
        };
    }

private Q_SLOTS:
    void init() { sessions.clear(); }

    void nothingBeforeYes()
    {
        Request r(QStringLiteral("alex"), factory());
        QCOMPARE(r.state(), Request::Waiting);
        QVERIFY(sessions.isEmpty());
    }

    void rightPassword()
    {
        Request r(QStringLiteral("alex"), factory());
        QSignalSpy done(&r, &Request::finished);

        r.accept(QStringLiteral("secret"));
        QCOMPARE(sessions.size(), 1);
        FakeSession *s = sessions.first();
        QVERIFY(s->started);
        QCOMPARE(s->user, QStringLiteral("alex"));
        QVERIFY(r.busy());
        QVERIFY(s->answers.isEmpty());

        Q_EMIT s->question(QStringLiteral("Password: "), false);
        QCOMPARE(s->answers, QStringList{QStringLiteral("secret")});

        Q_EMIT s->finished(true);
        QCOMPARE(r.state(), Request::Done);
        QCOMPARE(done.size(), 1);
        QCOMPARE(done.first().first().toBool(), true);
        QVERIFY(r.authorized());
    }

    void noPasswordNeeded()
    {
        // an account PAM lets through without a question: still needs Yes
        Request r(QStringLiteral("alex"), factory());
        QSignalSpy done(&r, &Request::finished);
        r.accept(QString());
        Q_EMIT sessions.first()->finished(true);
        QCOMPARE(done.size(), 1);
        QVERIFY(r.authorized());
    }

    void wrongPasswordThenRight()
    {
        Request r(QStringLiteral("alex"), factory());
        QSignalSpy done(&r, &Request::finished);

        r.accept(QStringLiteral("nope"));
        Q_EMIT sessions.at(0)->question(QStringLiteral("Password: "), false);
        Q_EMIT sessions.at(0)->error(QStringLiteral("Authentication failure"));
        Q_EMIT sessions.at(0)->finished(false);

        QCOMPARE(r.state(), Request::Waiting);
        QVERIFY(r.wrongPassword());
        QVERIFY(r.message().isEmpty());
        QCOMPARE(r.failures(), 1);
        QCOMPARE(done.size(), 0);

        r.accept(QStringLiteral("secret"));
        QCOMPARE(sessions.size(), 2);
        QVERIFY(!r.wrongPassword());
        Q_EMIT sessions.at(1)->question(QStringLiteral("Password: "), false);
        QCOMPARE(sessions.at(1)->answers, QStringList{QStringLiteral("secret")});
        Q_EMIT sessions.at(1)->finished(true);
        QCOMPARE(done.size(), 1);
        QVERIFY(r.authorized());
    }

    void threeWrongPasswords()
    {
        Request r(QStringLiteral("alex"), factory());
        QSignalSpy done(&r, &Request::finished);
        for (int i = 0; i < Request::maxFailures; ++i) {
            QCOMPARE(r.state(), Request::Waiting);
            r.accept(QStringLiteral("nope"));
            Q_EMIT sessions.last()->question(QStringLiteral("Password: "), false);
            Q_EMIT sessions.last()->finished(false);
        }
        QCOMPARE(r.state(), Request::Done);
        QCOMPARE(done.size(), 1);
        QCOMPARE(done.first().first().toBool(), false);
    }

    void noWhileChecking()
    {
        Request r(QStringLiteral("alex"), factory());
        QSignalSpy done(&r, &Request::finished);
        r.accept(QStringLiteral("secret"));
        QPointer<FakeSession> s = sessions.first();
        r.reject();
        QVERIFY(s->cancelled);
        QCOMPARE(done.size(), 1);
        QCOMPARE(done.first().first().toBool(), false);
        // a late answer from PAM changes nothing
        if (s)
            Q_EMIT s->finished(true);
        QCOMPARE(done.size(), 1);
        QVERIFY(!r.authorized());
    }

    void noBeforeYes()
    {
        Request r(QStringLiteral("alex"), factory());
        QSignalSpy done(&r, &Request::finished);
        r.reject();
        r.reject();
        QCOMPARE(done.size(), 1);
        QVERIFY(sessions.isEmpty());
    }

    void polkitCancels()
    {
        Request r(QStringLiteral("alex"), factory());
        QSignalSpy done(&r, &Request::finished);
        r.accept(QStringLiteral("secret"));
        r.cancelFromPolkit();
        QVERIFY(sessions.first()->cancelled);
        QCOMPARE(done.size(), 1);
        r.accept(QStringLiteral("again"));
        QCOMPARE(sessions.size(), 1);
    }

    void otherQuestion()
    {
        // a PAM module that asks something visible (a code), after the password
        Request r(QStringLiteral("alex"), factory());
        r.accept(QStringLiteral("secret"));
        FakeSession *s = sessions.first();
        Q_EMIT s->question(QStringLiteral("Password: "), false);
        Q_EMIT s->question(QStringLiteral("Verification code: "), true);
        QCOMPARE(r.state(), Request::Asking);
        QCOMPARE(r.question(), QStringLiteral("Verification code: "));

        r.accept(QStringLiteral("123456"));
        QCOMPARE(s->answers, (QStringList{QStringLiteral("secret"), QStringLiteral("123456")}));
        QVERIFY(r.busy());
        QVERIFY(r.question().isEmpty());
    }

    void secondPasswordQuestion()
    {
        // the typed password is given once; a second question waits for the person
        Request r(QStringLiteral("alex"), factory());
        r.accept(QStringLiteral("secret"));
        FakeSession *s = sessions.first();
        Q_EMIT s->question(QStringLiteral("Password: "), false);
        Q_EMIT s->question(QStringLiteral("Password: "), false);
        QCOMPARE(s->answers.size(), 1);
        QCOMPARE(r.state(), Request::Asking);
    }

    void infoMessage()
    {
        Request r(QStringLiteral("alex"), factory());
        QSignalSpy changed(&r, &Request::messageChanged);
        r.accept(QStringLiteral("x"));
        Q_EMIT sessions.first()->info(QStringLiteral("Place your finger on the reader"));
        QCOMPARE(r.message(), QStringLiteral("Place your finger on the reader"));
        QVERIFY(changed.size() >= 1);
    }

    void otherAdministrator()
    {
        Request r(QStringLiteral("alex"), factory());
        r.accept(QStringLiteral("secret"));
        QPointer<FakeSession> first = sessions.first();
        r.setUser(QStringLiteral("camille"));
        QVERIFY(first.isNull() || first->cancelled);
        QCOMPARE(r.state(), Request::Waiting);
        r.accept(QStringLiteral("hers"));
        QCOMPARE(sessions.last()->user, QStringLiteral("camille"));
    }

    void idleCloses()
    {
        QCOMPARE(Request::idleTimeoutMs, 120000);
        Request r(QStringLiteral("alex"), factory());
        QSignalSpy done(&r, &Request::finished);
        r.setIdleInterval(100);
        QTest::qWait(60);
        r.touch();
        QTest::qWait(60);
        QCOMPARE(done.size(), 0);
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(!r.authorized());
    }
};

QTEST_GUILESS_MAIN(TestRequest)
#include "tst_request.moc"
