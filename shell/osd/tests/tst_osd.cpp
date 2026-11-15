// The Raye on-screen display.
//
// interface: the files still fit the Plasma installed at build time. Our
// qmldir is the stock one without its "prefer" line, our Osd and OsdItem
// declare every property the stock ones do, and plasmashell still sets
// the properties we answer to. A Plasma release that renames one fails
// the package build instead of breaking the volume keys.
//
// The rest runs Osd.qml from a copy of the module (our files, the stock
// plugin) the way plasmashell drives it, with a stand-in for the player
// list: the pill with a value, the pill with words, the media card, the
// fade, where the window goes, and French.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace Qt::StringLiterals;

static const QString s_module = u"org/kde/plasma/workspace/osd"_s;

// A player for the media card: what it reads, and counters for its buttons.
// (QML cannot declare the capitalised MPRIS method names.)
class MockPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString track MEMBER track NOTIFY changed)
    Q_PROPERTY(QString artist MEMBER artist NOTIFY changed)
    Q_PROPERTY(QString identity MEMBER identity NOTIFY changed)
    Q_PROPERTY(QString iconName MEMBER iconName NOTIFY changed)
    Q_PROPERTY(QUrl artUrl MEMBER artUrl NOTIFY changed)
    Q_PROPERTY(int playbackStatus MEMBER playbackStatus NOTIFY changed)
    Q_PROPERTY(bool canGoNext MEMBER canGoNext NOTIFY changed)
    Q_PROPERTY(bool canGoPrevious MEMBER canGoPrevious NOTIFY changed)
    Q_PROPERTY(bool canPlay MEMBER canPlay NOTIFY changed)
    Q_PROPERTY(bool canPause MEMBER canPause NOTIFY changed)

public:
    QString track;
    QString artist = QStringLiteral("Northern Lights");
    QString identity = QStringLiteral("Music");
    QString iconName = QStringLiteral("multimedia-audio-player");
    QUrl artUrl;
    int playbackStatus = 2; // Playing
    bool canGoNext = true;
    bool canGoPrevious = true;
    bool canPlay = true;
    bool canPause = true;
    int previousCalls = 0;
    int playPauseCalls = 0;
    int nextCalls = 0;

    void setStatus(int status)
    {
        playbackStatus = status;
        Q_EMIT changed();
    }

    Q_INVOKABLE void Previous() { ++previousCalls; }
    Q_INVOKABLE void PlayPause()
    {
        ++playPauseCalls;
        setStatus(playbackStatus == 2 ? 3 : 2);
    }
    Q_INVOKABLE void Next() { ++nextCalls; }

Q_SIGNALS:
    void changed();
};

class TestOsd : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_stockDir;
    QStringList m_warnings;
    static inline TestOsd *s_self = nullptr;

    static void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
    {
        if (type != QtDebugMsg && type != QtInfoMsg && (msg.contains(u"Osd.qml"_s) || msg.contains(u"OsdItem.qml"_s)))
            s_self->m_warnings << msg;
    }

    static QString read(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(f.readAll());
    }

    // the stock file: moved aside to <file>.plasma once raye-shell is installed
    QString stock(const QString &name) const
    {
        const QString moved = m_stockDir + u'/' + name + u".plasma"_s;
        if (QFileInfo::exists(moved))
            return moved;
        const QFileInfo plain(m_stockDir + u'/' + name);
        if (plain.isSymLink() && plain.symLinkTarget().contains(u"/raye/"_s))
            return {};
        return plain.exists() ? plain.filePath() : QString();
    }

    // "property <type> <name>" and "property alias <name>" of a QML file
    static QMap<QString, QString> properties(const QString &qml)
    {
        static const QRegularExpression re(u"^\\s*(?:readonly\\s+)?property\\s+([\\w.]+)\\s+(\\w+)"_s,
                                           QRegularExpression::MultilineOption);
        QMap<QString, QString> out;
        for (auto it = re.globalMatch(qml); it.hasNext();) {
            const auto m = it.next();
            out.insert(m.captured(2), m.captured(1));
        }
        return out;
    }

    // the qmldir lines that matter, in order
    static QStringList qmldirLines(const QString &text)
    {
        QStringList out;
        for (const QString &line : text.split(u'\n')) {
            const QString l = line.simplified();
            if (l.isEmpty() || l.startsWith(u'#') || l.startsWith(u"prefer "_s))
                continue;
            out << l;
        }
        return out;
    }

    struct Run {
        std::unique_ptr<QQmlEngine> engine;
        std::unique_ptr<MockPlayer> player;
        std::unique_ptr<QQuickWindow> osd;
        QQuickItem *item = nullptr;
        QObject *mpris = nullptr;

        template<typename T = QQuickItem>
        T *find(const QString &name) const
        {
            return item ? item->findChild<T *>(name) : nullptr;
        }
    };

    std::unique_ptr<Run> start()
    {
        auto run = std::make_unique<Run>();
        run->engine = std::make_unique<QQmlEngine>();
        // our copy of the module first, then the stand-ins
        run->engine->addImportPath(m_dir.path());
        run->engine->addImportPath(QStringLiteral(RAYE_OSD_TESTS "/mock"));
        run->engine->addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        run->engine->addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        KLocalization::setupLocalizedContext(run->engine.get());

        QQmlComponent c(run->engine.get());
        c.loadFromModule("org.kde.plasma.workspace.osd", "Osd");
        std::unique_ptr<QObject> o(c.create());
        if (!o) {
            qWarning() << c.errors();
            return nullptr;
        }
        run->osd.reset(qobject_cast<QQuickWindow *>(o.release()));
        if (!run->osd)
            return nullptr;
        run->item = run->osd->property("mainItem").value<QQuickItem *>();
        for (QObject *child : run->item->children()) {
            if (child->property("currentPlayer").isValid() || QByteArray(child->metaObject()->className()).startsWith("Mpris2Model"))
                run->mpris = child;
        }
        return run;
    }

    static void addPlayer(Run *run, const QString &track, int status = 2)
    {
        run->player = std::make_unique<MockPlayer>();
        run->player->track = track;
        run->player->playbackStatus = status;
        QVERIFY(run->mpris);
        run->mpris->setProperty("currentPlayer", QVariant::fromValue<QObject *>(run->player.get()));
    }

    // what plasmashell does at every OSD (shell/osd.cpp, showOsd)
    static void show(QQuickWindow *osd, const QString &icon, const QVariant &value, bool progress, int max = 100, const QString &more = {})
    {
        osd->setProperty("icon", icon);
        osd->setProperty("osdValue", value);
        osd->setProperty("osdMaxValue", max);
        osd->setProperty("osdAdditionalText", more);
        osd->setProperty("showingProgress", progress);
        osd->setProperty("animateOpacity", false);
        osd->setProperty("opacity", 1.0);
        osd->setProperty("visible", true);
        osd->setProperty("animateOpacity", true);
        osd->setProperty("opacity", 0.0);
    }

    // and when its timer runs out (hideOsd)
    static void hide(QQuickWindow *osd)
    {
        osd->setProperty("visible", false);
        osd->setProperty("osdValue", 0);
    }

private Q_SLOTS:
    void initTestCase()
    {
        s_self = this;
        qInstallMessageHandler(collect);
        QVERIFY(m_dir.isValid());
        m_stockDir = QLibraryInfo::path(QLibraryInfo::QmlImportsPath) + u'/' + s_module;
        QVERIFY2(QFileInfo::exists(m_stockDir + u"/libplasmashell_osd.so"_s), qPrintable(m_stockDir));

        // the module as plasmashell will see it: our three files, the stock plugin
        const QString dir = m_dir.path() + u'/' + s_module;
        QVERIFY(QDir().mkpath(dir));
        for (const QString &f : {u"qmldir"_s, u"Osd.qml"_s, u"OsdItem.qml"_s})
            QVERIFY(QFile::copy(QStringLiteral(RAYE_OSD_DIR) + u'/' + f, dir + u'/' + f));
        for (const QString &f : {u"libplasmashell_osd.so"_s, u"plasmashell_osd.qmltypes"_s})
            QVERIFY(QFile::link(m_stockDir + u'/' + f, dir + u'/' + f));

        // French from the po file
        const QString mo = m_dir.path() + u"/locale/fr/LC_MESSAGES"_s;
        QVERIFY(QDir().mkpath(mo));
        QCOMPARE(QProcess::execute(u"msgfmt"_s, {u"-o"_s, mo + u"/raye_shell_osd.mo"_s,
                                                 QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_osd.po")}), 0);
        KLocalizedString::addDomainLocaleDir("raye_shell_osd", m_dir.path() + u"/locale"_s);
    }

    void init()
    {
        m_warnings.clear();
    }

    void cleanup()
    {
        QVERIFY2(m_warnings.isEmpty(), qPrintable(m_warnings.join(u'\n')));
    }

    // --- the contract with the installed Plasma ------------------------------

    void qmldirMatchesStock()
    {
        const QString path = stock(u"qmldir"_s);
        QVERIFY2(!path.isEmpty(), "no stock qmldir of org.kde.plasma.workspace.osd");
        const QString text = read(path);
        // the stock module really prefers its compiled-in files: that is why we divert
        QVERIFY(text.contains(u"\nprefer :/qt/qml/org/kde/plasma/workspace/osd/"_s));
        QCOMPARE(qmldirLines(read(QStringLiteral(RAYE_OSD_DIR "/qmldir"))), qmldirLines(text));
        QVERIFY(!read(QStringLiteral(RAYE_OSD_DIR "/qmldir")).contains(u"prefer"_s));
    }

    void osdProperties()
    {
        const QString path = stock(u"Osd.qml"_s);
        QVERIFY2(!path.isEmpty(), "no stock Osd.qml");
        const auto theirs = properties(read(path));
        const auto ours = properties(read(QStringLiteral(RAYE_OSD_DIR "/Osd.qml")));
        QVERIFY(theirs.size() >= 6);
        for (auto it = theirs.cbegin(); it != theirs.cend(); ++it) {
            QVERIFY2(ours.contains(it.key()), qPrintable(u"Osd.qml lacks "_s + it.key()));
            QCOMPARE(ours.value(it.key()), it.value());
        }
    }

    void osdItemProperties()
    {
        const QString path = stock(u"OsdItem.qml"_s);
        QVERIFY2(!path.isEmpty(), "no stock OsdItem.qml");
        const auto theirs = properties(read(path));
        const auto ours = properties(read(QStringLiteral(RAYE_OSD_DIR "/OsdItem.qml")));
        for (auto it = theirs.cbegin(); it != theirs.cend(); ++it) {
            QVERIFY2(ours.contains(it.key()), qPrintable(u"OsdItem.qml lacks "_s + it.key()));
            QCOMPARE(ours.value(it.key()), it.value());
        }
    }

    void pluginStillThere()
    {
        const QString types = read(m_stockDir + u"/plasmashell_osd.qmltypes"_s);
        QVERIFY(types.contains(u"\"org.kde.plasma.workspace.osd/OsdWindow 254.0\""_s));
    }

    void plasmashellSetsWhatWeRead()
    {
        const QString exe = QStandardPaths::findExecutable(u"plasmashell"_s);
        QVERIFY2(!exe.isEmpty(), "plasmashell not installed");
        QFile f(exe);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray bin = f.readAll();
        for (const char *name : {"org.kde.plasma.workspace.osd", "osdValue", "osdMaxValue", "osdAdditionalText",
                                 "showingProgress", "icon", "timeout", "visible", "animateOpacity", "opacity"}) {
            QVERIFY2(bin.contains(QByteArray(QByteArray(name) + char(0))), name);
        }
    }

    // --- the display itself ---------------------------------------------------

    void volumePill()
    {
        auto run = start();
        QVERIFY(run);
        QVERIFY(run->osd->property("timeout").toInt() == 1834);
        show(run->osd.get(), u"audio-volume-medium"_s, 40, true);

        auto *pill = run->find(u"osdPill"_s);
        QVERIFY(pill);
        QCOMPARE(pill->width(), 192.0);
        QCOMPARE(pill->height(), 48.0);
        QCOMPARE(run->find(u"osdValueText"_s)->property("text").toString(), u"40"_s);
        QVERIFY(run->find(u"osdTrack"_s)->isVisible());
        QVERIFY(!run->find(u"osdLabel"_s)->isVisible());
        auto *track = run->find(u"osdTrack"_s);
        auto *fill = run->find(u"osdFill"_s);
        QTRY_COMPARE(fill->width(), qRound(track->width() * 0.4) * 1.0);
        // no player: no card, clicks go through
        QVERIFY(!run->find(u"osdMediaCard"_s)->isVisible());
        QVERIFY(run->osd->property("outputOnly").toBool());

        // held key: new value, same window
        show(run->osd.get(), u"audio-volume-high"_s, 85, true);
        QCOMPARE(run->find(u"osdValueText"_s)->property("text").toString(), u"85"_s);
        QTRY_COMPARE(fill->width(), qRound(track->width() * 0.85) * 1.0);
    }

    static QVariantList plays(Run *run)
    {
        const auto all = run->item->findChildren<QObject *>();
        for (QObject *o : all) {
            if (QByteArray(o->metaObject()->className()).startsWith("VolumeFeedback"))
                return o->property("plays").toList();
        }
        return {u"no feedback"_s};
    }

    void volumeKeysSound()
    {
        auto run = start();
        QVERIFY(run);
        QObject *chime = run->item->findChild<QObject *>(u"volumeChime"_s);
        QVERIFY(chime);
        // one press: one sound at once, on the output in use
        show(run->osd.get(), u"audio-volume-medium"_s, 40, true);
        QCOMPARE(plays(run.get()), QVariantList({7}));

        // a held key repeats every 40 ms: quiet until it lets go, then once
        QTest::qWait(chime->property("quietMs").toInt() - 50);
        for (int v = 42; v <= 60; v += 2) {
            show(run->osd.get(), u"audio-volume-high"_s, v, true);
            QTest::qWait(40);
        }
        QCOMPARE(plays(run.get()).size(), 1);
        QTRY_COMPARE(plays(run.get()).size(), 2);
        QTest::qWait(chime->property("quietMs").toInt() + 50);

        // brightness, the mute key and a level of 0 make no sound
        show(run->osd.get(), u"video-display-brightness"_s, 50, true);
        show(run->osd.get(), u"audio-volume-muted"_s, 0, true);
        QTest::qWait(chime->property("settleMs").toInt() + 100);
        QCOMPARE(plays(run.get()).size(), 2);
    }

    void volumeKeyShowsTwice()
    {
        auto run = start();
        QVERIFY(run);
        QObject *chime = run->item->findChild<QObject *>(u"volumeChime"_s);
        QVERIFY(chime);
        // plasmashell gets volumeChanged twice for one press, same value
        show(run->osd.get(), u"audio-volume-medium"_s, 70, true);
        show(run->osd.get(), u"audio-volume-medium"_s, 70, true);
        QTest::qWait(chime->property("settleMs").toInt() + 100);
        QCOMPARE(plays(run.get()).size(), 1);
    }

    void overMaximum()
    {
        auto run = start();
        QVERIFY(run);
        show(run->osd.get(), u"audio-volume-high"_s, 150, true, 150);
        auto *track = run->find(u"osdTrack"_s);
        QTRY_COMPARE(run->find(u"osdFill"_s)->width(), track->width());
        QCOMPARE(run->find(u"osdValueText"_s)->property("text").toString(), u"150"_s);
        // the number keeps room for three digits: the pill does not grow
        QCOMPARE(run->find(u"osdPill"_s)->width(), 192.0);
    }

    void brightnessWithDisplayName()
    {
        auto run = start();
        QVERIFY(run);
        show(run->osd.get(), u"video-display-brightness"_s, 70, true, 100, u"Built-in display"_s);
        auto *more = run->find(u"osdAdditionalText"_s);
        QVERIFY(more->isVisible());
        QCOMPARE(more->property("text").toString(), u"Built-in display"_s);
        QVERIFY(run->find(u"osdPill"_s)->width() > 192);
    }

    void messagePill()
    {
        auto run = start();
        QVERIFY(run);
        show(run->osd.get(), u"input-caps-on"_s, u"Caps Lock On"_s, false);
        auto *label = run->find(u"osdLabel"_s);
        QVERIFY(label->isVisible());
        QCOMPARE(label->property("text").toString(), u"Caps Lock On"_s);
        QVERIFY(!run->find(u"osdTrack"_s)->isVisible());
        QVERIFY(!run->find(u"osdValueText"_s)->isVisible());
        QCOMPARE(run->find(u"osdPill"_s)->width(), 192.0);

        // long words: wider, up to a limit
        show(run->osd.get(), u"preferences-desktop-keyboard"_s,
             u"English (United States, international with dead keys)"_s, false);
        const qreal w = run->find(u"osdPill"_s)->width();
        QVERIFY(w > 192);
        QVERIFY(w <= 400);
    }

    void mediaCard()
    {
        auto run = start();
        QVERIFY(run);
        addPlayer(run.get(), u"Aurora over the lake"_s);
        show(run->osd.get(), u"audio-volume-medium"_s, 30, true);
        auto *card = run->find(u"osdMediaCard"_s);
        QVERIFY(card->isVisible());
        QCOMPARE(card->width(), 360.0);
        QCOMPARE(run->find(u"osdMediaTitle"_s)->property("text").toString(), u"Aurora over the lake"_s);
        QCOMPARE(run->find(u"osdMediaArtist"_s)->property("text").toString(), u"Northern Lights"_s);
        // the card above the pill, the buttons take clicks
        QVERIFY(card->y() + card->height() <= run->find(u"osdPill"_s)->y());
        QVERIFY(!run->osd->property("outputOnly").toBool());

        auto *play = run->find(u"osdPlayPause"_s);
        QCOMPARE(play->property("label").toString(), u"Pause"_s);
        QMetaObject::invokeMethod(play, "clicked");
        QCOMPARE(run->player->playPauseCalls, 1);
        QCOMPARE(play->property("label").toString(), u"Play"_s);
        QMetaObject::invokeMethod(run->find(u"osdNext"_s), "clicked");
        QMetaObject::invokeMethod(run->find(u"osdPrevious"_s), "clicked");
        QCOMPARE(run->player->nextCalls, 1);
        QCOMPARE(run->player->previousCalls, 1);

        // not for brightness, not for a stopped player
        show(run->osd.get(), u"video-display-brightness"_s, 50, true);
        QVERIFY(!card->isVisible());
        QVERIFY(run->osd->property("outputOnly").toBool());
        run->player->setStatus(1);
        show(run->osd.get(), u"audio-volume-medium"_s, 30, true);
        QVERIFY(!card->isVisible());
    }

    void placement()
    {
        auto run = start();
        QVERIFY(run);
        show(run->osd.get(), u"audio-volume-medium"_s, 40, true);
        QTRY_COMPARE(run->find(u"osdContent"_s)->opacity(), 1.0);
        const QRect screen = run->osd->screen()->geometry();
        auto *pill = run->find(u"osdPill"_s);
        const QPointF p = pill->mapToItem(nullptr, QPointF(0, 0));
        const qreal pillX = run->osd->x() + p.x();
        const qreal pillBottom = run->osd->y() + p.y() + pill->height();
        QCOMPARE(qRound(pillX + pill->width() / 2), screen.x() + screen.width() / 2);
        // 24 px above the 48 px taskbar
        QCOMPARE(qRound(pillBottom), screen.y() + screen.height() - 48 - 24);

        // the card grows the window upwards: the pill stays where it was
        addPlayer(run.get(), u"Song"_s);
        QCoreApplication::processEvents();
        const QPointF q = pill->mapToItem(nullptr, QPointF(0, 0));
        QCOMPARE(qRound(run->osd->y() + q.y() + pill->height()), qRound(pillBottom));

        // another screen in use (Osd.qml sets it from KWin at each show):
        // the display goes there, centred above its taskbar
        QObject *item = run->find(u"osdContent"_s);
        while (item && !item->property("area").isValid())
            item = item->parent();
        QVERIFY(item);
        item->setProperty("area", QRectF(1280, 0, 1920, 1080));
        QCoreApplication::processEvents();
        const QPointF r = pill->mapToItem(nullptr, QPointF(0, 0));
        QCOMPARE(qRound(run->osd->x() + r.x() + pill->width() / 2), 1280 + 1920 / 2);
        QCOMPARE(qRound(run->osd->y() + r.y() + pill->height()), 1080 - 48 - 24);
    }

    void fadesInAndOut()
    {
        auto run = start();
        QVERIFY(run);
        auto *content = run->find(u"osdContent"_s);
        QCOMPARE(content->opacity(), 0.0);
        show(run->osd.get(), u"audio-volume-medium"_s, 40, true);
        QTRY_COMPARE_WITH_TIMEOUT(content->opacity(), 1.0, 1000);
        QTest::qWait(1200);
        // shown again before the end: stays
        show(run->osd.get(), u"audio-volume-medium"_s, 45, true);
        QTest::qWait(500);
        QCOMPARE(content->opacity(), 1.0);
        // gone shortly before plasmashell hides the window
        QTRY_COMPARE_WITH_TIMEOUT(content->opacity(), 0.0, 2500);
        QVERIFY(run->osd->isVisible());
        hide(run->osd.get());
        QVERIFY(!run->osd->isVisible());
        // next time it starts from nothing and comes in again
        show(run->osd.get(), u"audio-volume-medium"_s, 50, true);
        QTRY_COMPARE_WITH_TIMEOUT(content->opacity(), 1.0, 1000);
    }

    void french()
    {
        // run as its own test with LANGUAGE=fr (ctest "osd-fr")
        if (qgetenv("LANGUAGE") != "fr")
            QSKIP("French runs in osd-fr");
        auto run = start();
        QVERIFY(run);
        addPlayer(run.get(), u"Chanson"_s);
        show(run->osd.get(), u"audio-volume-medium"_s, 30, true);
        QCOMPARE(run->find(u"osdPrevious"_s)->property("label").toString(), u"Piste précédente"_s);
        QCOMPARE(run->find(u"osdNext"_s)->property("label").toString(), u"Piste suivante"_s);
        QCOMPARE(run->find(u"osdPlayPause"_s)->property("label").toString(), u"Pause"_s);
        QMetaObject::invokeMethod(run->find(u"osdPlayPause"_s), "clicked");
        QCOMPARE(run->find(u"osdPlayPause"_s)->property("label").toString(), u"Lire"_s);

        // every string of the files has its French
        const QString po = read(QStringLiteral(RAYE_SHELL_PO_DIR "/fr/raye_shell_osd.po"));
        static const QRegularExpression call(u"i18nd\\(\"raye_shell_osd\", \"([^\"]+)\"\\)"_s);
        int count = 0;
        for (const QString &file : {u"Osd.qml"_s, u"OsdItem.qml"_s}) {
            const QString source = read(QStringLiteral(RAYE_OSD_DIR) + u'/' + file);
            QVERIFY(!source.contains(u"i18n(\""_s));
            for (auto it = call.globalMatch(source); it.hasNext(); ++count) {
                const QString id = it.next().captured(1);
                const QRegularExpression entry(u"msgid \"%1\"\\nmsgstr \"([^\"]+)\""_s.arg(QRegularExpression::escape(id)));
                QVERIFY2(entry.match(po).hasMatch(), qPrintable(id));
            }
        }
        QCOMPARE(count, 4);
    }
};

QTEST_MAIN(TestOsd)
#include "tst_osd.moc"
