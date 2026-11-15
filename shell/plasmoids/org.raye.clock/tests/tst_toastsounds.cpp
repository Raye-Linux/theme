// Notification sounds under Do not disturb (toasts/ToastSounds.qml), on a
// stand-in for the sound server's saved streams (mock/): the notification
// stream shows up only after Do not disturb is already on, and must be
// muted then all the same.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QQmlComponent>
#include <QQmlEngine>
#include <QTest>

using namespace Qt::StringLiterals;

class TestToastSounds : public QObject
{
    Q_OBJECT

    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QObject> m_settings;
    std::unique_ptr<QObject> m_sounds;

    // the notification settings, as far as the sounds use them
    std::unique_ptr<QObject> settings(bool soundsInhibited)
    {
        QQmlComponent c(m_engine.get());
        c.setData("import QtQml\n"
                  "QtObject { property bool notificationSoundsInhibited; property int saves: 0; function save() { ++saves } }",
                  QUrl());
        std::unique_ptr<QObject> o(c.create());
        if (o)
            o->setProperty("notificationSoundsInhibited", soundsInhibited);
        return o;
    }

    // set up the way Toasts.qml does it: inhibited first, then the settings
    void start(bool inhibited, bool soundsInhibited, bool streamMuted = false)
    {
        m_engine = std::make_unique<QQmlEngine>();
        m_engine->addImportPath(QStringLiteral(RAYE_CLOCK_TESTS "/mock"));
        m_settings = settings(soundsInhibited);
        QVERIFY(m_settings);
        QQmlComponent c(m_engine.get(), QUrl::fromLocalFile(QStringLiteral(RAYE_CLOCK_DIR "/contents/ui/toasts/ToastSounds.qml")));
        m_sounds.reset(c.create());
        QVERIFY2(m_sounds, qPrintable(c.errorString()));
        // how the stream will come: it is not there yet
        auto *instantiator = m_sounds->property("instantiator").value<QObject *>();
        QVERIFY(instantiator);
        auto *streams = instantiator->property("model").value<QObject *>();
        QVERIFY(streams);
        streams->setProperty("eventMuted", streamMuted);
        m_sounds->setProperty("inhibited", inhibited);
        m_sounds->setProperty("settings", QVariant::fromValue(m_settings.get()));
    }

    QObject *stream() const
    {
        return m_sounds->property("stream").value<QObject *>();
    }

private Q_SLOTS:
    void cleanup()
    {
        m_sounds.reset();
        m_settings.reset();
        m_engine.reset();
    }

    void aStreamThatComesLateIsMutedUnderDoNotDisturb()
    {
        start(true, false);
        QVERIFY(!stream());
        QTRY_VERIFY_WITH_TIMEOUT(stream(), 2000);
        QVERIFY(stream()->property("muted").toBool());
        QVERIFY(m_settings->property("notificationSoundsInhibited").toBool());
        QVERIFY(m_settings->property("saves").toInt() > 0);
    }

    // muted by us before a restart: given back when Do not disturb is off
    void aStreamThatComesLateIsGivenItsSoundBack()
    {
        start(false, true, true);
        QTRY_VERIFY_WITH_TIMEOUT(stream(), 2000);
        QVERIFY(!stream()->property("muted").toBool());
        QVERIFY(!m_settings->property("notificationSoundsInhibited").toBool());
    }

    void turningDoNotDisturbOffUnmutesIt()
    {
        start(true, false);
        QTRY_VERIFY_WITH_TIMEOUT(stream() && stream()->property("muted").toBool(), 2000);
        m_sounds->setProperty("inhibited", false);
        QVERIFY(!stream()->property("muted").toBool());
        QVERIFY(!m_settings->property("notificationSoundsInhibited").toBool());
    }
};

QTEST_GUILESS_MAIN(TestToastSounds)
#include "tst_toastsounds.moc"
