// The right-click menu's lists of a taskbar app: the name the activity
// manager knows the app by, and which recent files are shown.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appjumplist.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TestAppJumpList : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void storageIds()
    {
        QCOMPARE(JumpList::storageId(u"org.kde.kate.desktop"_s), u"org.kde.kate"_s);
        QCOMPARE(JumpList::storageId(u"firefox.desktop"_s), u"firefox"_s);
        QCOMPARE(JumpList::storageId(u"firefox"_s), QString());
        QCOMPARE(JumpList::storageId(u"../evil.desktop"_s), QString());
        QCOMPARE(JumpList::storageId(u"/usr/share/applications/x.desktop"_s), QString());
        QCOMPARE(JumpList::storageId(QString()), QString());
    }

    void recentFiles()
    {
        const QUrl a = QUrl::fromLocalFile(u"/home/u/a.txt"_s);
        const QUrl b = QUrl::fromLocalFile(u"/home/u/b.txt"_s);
        const QUrl gone = QUrl::fromLocalFile(u"/home/u/gone.txt"_s);
        const QUrl web(u"https://example.org/x"_s);
        const auto exists = [&](const QUrl &u) { return u != gone; };
        // newest first, each once, only local files that are still there
        QCOMPARE(JumpList::pickRecent({a, web, gone, a, b}, 10, exists), (QList<QUrl>{a, b}));
        QCOMPARE(JumpList::pickRecent({a, b}, 1, exists), (QList<QUrl>{a}));
        QCOMPARE(JumpList::pickRecent({a, b}, 0, exists), (QList<QUrl>{}));
        QCOMPARE(JumpList::pickRecent({}, 5, exists), (QList<QUrl>{}));
    }

    void unknownApp()
    {
        AppJumpList list;
        QSignalSpy changed(&list, &AppJumpList::changed);
        list.setDesktopId(u"no.such.app.raye-test.desktop"_s);
        QCOMPARE(changed.count(), 1);
        QVERIFY(!list.valid());
        QVERIFY(list.actions().isEmpty());
        QVERIFY(list.recent().isEmpty());
        QVERIFY(!list.launch());
        QVERIFY(!list.runAction(0));
        QVERIFY(!list.openRecent(QUrl::fromLocalFile(u"/tmp/x"_s)));
        // a path is never taken for an app
        list.setDesktopId(u"../../bin/sh.desktop"_s);
        QVERIFY(!list.valid());
    }
};

QTEST_GUILESS_MAIN(TestAppJumpList)
#include "tst_appjumplist.moc"
