// The Project panel on a stand-in for the screens: four rows, the one in
// use marked, keys and clicks reach the screens, and a single screen makes
// the rows do nothing.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KLocalizedQmlContext>

#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

using namespace Qt::StringLiterals;

class TestProjectPanel : public QObject
{
    Q_OBJECT

private:
    struct View {
        std::unique_ptr<QQmlEngine> engine;
        std::unique_ptr<QObject> fixture;
        QQuickWindow *window = nullptr;
        QQuickItem *panel = nullptr;
        QObject *controller = nullptr;
    };

    std::unique_ptr<View> view()
    {
        auto v = std::make_unique<View>();
        v->engine = std::make_unique<QQmlEngine>();
        v->engine->addImportPath(QStringLiteral(RAYE_THEME_QML_DIR));
        v->engine->addImportPath(QStringLiteral(RAYE_SHELL_QML_DIR));
        KLocalization::setupLocalizedContext(v->engine.get());
        QQmlComponent c(v->engine.get(), QUrl::fromLocalFile(QStringLiteral(RAYE_PROJECT_TESTS "/fixture.qml")));
        QObject *o = c.create(v->engine->rootContext());
        if (!o) {
            qWarning() << c.errors();
            return nullptr;
        }
        v->fixture.reset(o);
        v->window = qobject_cast<QQuickWindow *>(o);
        v->panel = o->property("panel").value<QQuickItem *>();
        v->controller = o->property("controller").value<QObject *>();
        if (!v->window || !v->panel || !v->controller)
            return nullptr;
        v->window->show();
        v->panel->forceActiveFocus();
        if (!QTest::qWaitForWindowExposed(v->window))
            return nullptr;
        return v;
    }

    static QQuickItem *item(QQuickItem *from, const QString &name)
    {
        if (from->objectName() == name)
            return from;
        const auto children = from->childItems();
        for (QQuickItem *child : children) {
            if (QQuickItem *found = item(child, name))
                return found;
        }
        return nullptr;
    }

private Q_SLOTS:
    void theFourChoicesInWindowsOrder()
    {
        auto v = view();
        QVERIFY(v);
        QCOMPARE(v->panel->property("modes").toStringList(),
                 QStringList({u"pc-only"_s, u"duplicate"_s, u"extend"_s, u"second-only"_s}));
        const QList<QPair<QString, QString>> rows = {
            {u"row-pc-only"_s, u"PC screen only"_s},
            {u"row-duplicate"_s, u"Duplicate"_s},
            {u"row-extend"_s, u"Extend"_s},
            {u"row-second-only"_s, u"Second screen only"_s},
        };
        for (const auto &r : rows) {
            QQuickItem *row = item(v->panel, r.first);
            QVERIFY2(row, qPrintable(r.first));
            QCOMPARE(row->property("title").toString(), r.second);
        }
    }

    void theChoiceInUseIsMarkedAndTheKeysStartOnIt()
    {
        auto v = view();
        QVERIFY(v);
        QVERIFY(item(v->panel, u"row-extend"_s)->property("inUse").toBool());
        QVERIFY(!item(v->panel, u"row-duplicate"_s)->property("inUse").toBool());
        QCOMPARE(v->panel->property("focusRow").toInt(), 2);
    }

    void theKeysMoveAndEnterPicks()
    {
        auto v = view();
        QVERIFY(v);
        QTest::keyClick(v->window, Qt::Key_Up);
        QCOMPARE(v->panel->property("focusRow").toInt(), 1);
        QTest::keyClick(v->window, Qt::Key_Return);
        QCOMPARE(v->controller->property("calls").toStringList(), QStringList{u"duplicate"_s});
        // the choice went through: the panel asks to leave
        QCOMPARE(v->fixture->property("closes").toInt(), 1);
    }

    void movingPastTheEndComesBackAround()
    {
        auto v = view();
        QVERIFY(v);
        QTest::keyClick(v->window, Qt::Key_Down);
        QTest::keyClick(v->window, Qt::Key_Down);
        QCOMPARE(v->panel->property("focusRow").toInt(), 0);
    }

    void aClickPicks()
    {
        auto v = view();
        QVERIFY(v);
        QQuickItem *row = item(v->panel, u"row-second-only"_s);
        QVERIFY(row);
        const QPointF p = row->mapToScene(QPointF(row->width() / 2, row->height() / 2));
        QTest::mouseClick(v->window, Qt::LeftButton, Qt::NoModifier, p.toPoint());
        QCOMPARE(v->controller->property("calls").toStringList(), QStringList{u"second-only"_s});
    }

    void winPAgainMovesOnWithoutChangingAnything()
    {
        auto v = view();
        QVERIFY(v);
        QMetaObject::invokeMethod(v->controller, "activated");
        QCOMPARE(v->panel->property("focusRow").toInt(), 3);
        QVERIFY(v->controller->property("calls").toStringList().isEmpty());
    }

    void escLeavesWithoutChangingAnything()
    {
        auto v = view();
        QVERIFY(v);
        QTest::keyClick(v->window, Qt::Key_Escape);
        QCOMPARE(v->fixture->property("closes").toInt(), 1);
        QVERIFY(v->controller->property("calls").toStringList().isEmpty());
    }

    void itLeavesWhenTheScreenThatAskedForItGoesAway()
    {
        auto v = view();
        QVERIFY(v);
        QMetaObject::invokeMethod(v->controller, "dismissed");
        QCOMPARE(v->fixture->property("closes").toInt(), 1);
    }

    void oneScreenRowsDoNothingAndSayWhy()
    {
        auto v = view();
        QVERIFY(v);
        v->controller->setProperty("canChoose", false);
        v->controller->setProperty("currentMode", u"pc-only"_s);
        QTest::keyClick(v->window, Qt::Key_Return);
        QVERIFY(v->controller->property("calls").toStringList().isEmpty());
        QVERIFY(!item(v->panel, u"row-extend"_s)->isEnabled());
        QTRY_VERIFY(item(v->panel, u"oneScreenHint"_s)->isVisible());
    }

    void theLinkOpensDisplaySettingsAndLeaves()
    {
        auto v = view();
        QVERIFY(v);
        QQuickItem *link = item(v->panel, u"displaySettings"_s);
        QVERIFY(link);
        QMetaObject::invokeMethod(link, "clicked");
        QCOMPARE(v->fixture->property("settingsOpened").toInt(), 1);
        QCOMPARE(v->fixture->property("closes").toInt(), 1);
    }

    // the panel took the place of a stock one: none of its wording is left
    void itSpeaksInItsOwnWords()
    {
        for (const QString &f : {u"ProjectPanel.qml"_s, u"Main.qml"_s}) {
            QFile file(QStringLiteral(RAYE_PROJECT_QML "/") + f);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QString code = QString::fromUtf8(file.readAll());
            QVERIFY(!code.contains(u"Only use external"_s));
            QVERIFY(!code.contains(u"KDE"_s));
        }
    }
};

QTEST_MAIN(TestProjectPanel)
#include "tst_projectpanel.moc"
