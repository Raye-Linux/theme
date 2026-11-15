/*
 * SPDX-FileCopyrightText: 2026 techoraye
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * The built plugins load and answer to the Raye names only, so they
 * sit next to Breeze without replacing it.
 */

#include <KPluginFactory>
#include <KPluginMetaData>

#include <QApplication>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QPluginLoader>
#include <QPushButton>
#include <QStyle>
#include <QStyleFactory>
#include <QStylePlugin>
#include <QTest>

#include <memory>

class TestRayePlugins : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stylePluginAnswersToRaye()
    {
        QPluginLoader loader(QStringLiteral(STYLE_PLUGIN));
        const QJsonArray keys = loader.metaData().value(QStringLiteral("MetaData")).toObject().value(QStringLiteral("Keys")).toArray();
        QCOMPARE(keys, QJsonArray({QStringLiteral("Raye")}));

        auto plugin = qobject_cast<QStylePlugin *>(loader.instance());
        QVERIFY2(plugin, qPrintable(loader.errorString()));

        std::unique_ptr<QStyle> style(plugin->create(QStringLiteral("Raye")));
        QVERIFY(style);
        // the whole tree was renamed, classes included
        QCOMPARE(QString::fromLatin1(style->metaObject()->className()), QStringLiteral("Raye::Style"));
        // same key in any case, like QStyleFactory looks it up
        std::unique_ptr<QStyle> lower(plugin->create(QStringLiteral("raye")));
        QVERIFY(lower);

        // never answers for Breeze: that one stays the fallback
        std::unique_ptr<QStyle> breeze(plugin->create(QStringLiteral("Breeze")));
        QVERIFY(!breeze);
    }

    // what QT_STYLE_OVERRIDE=Raye and the look and feel package go through
    void styleFactoryFindsRaye()
    {
        QVERIFY2(QStyleFactory::keys().contains(QStringLiteral("Raye")),
                 qPrintable(QStyleFactory::keys().join(QLatin1Char(' '))));
        std::unique_ptr<QStyle> style(QStyleFactory::create(QStringLiteral("Raye")));
        QVERIFY(style);
        QCOMPARE(QString::fromLatin1(style->metaObject()->className()), QStringLiteral("Raye::Style"));
    }

    void stylePaintsWidgets()
    {
        QStyle *style = QStyleFactory::create(QStringLiteral("Raye"));
        QVERIFY(style);
        QApplication::setStyle(style); // the application owns it from here

        QWidget window;
        auto button = new QPushButton(QStringLiteral("Restart now"), &window);
        auto combo = new QComboBox(&window);
        combo->addItem(QStringLiteral("Raye"));
        combo->move(0, 40);
        window.resize(200, 100);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(QString::fromLatin1(button->style()->metaObject()->className()), QStringLiteral("Raye::Style"));
        const QImage image = window.grab().toImage();
        QVERIFY(!image.isNull());
        QVERIFY(button->sizeHint().height() > 0);
    }

    void decorationPlugin()
    {
        const KPluginMetaData meta(QStringLiteral(DECORATION_PLUGIN));
        QVERIFY(meta.isValid());
        QCOMPARE(meta.pluginId(), QStringLiteral("org.raye.decoration"));
        QCOMPARE(meta.name(), QStringLiteral("Raye"));
        QCOMPARE(meta.value(QStringLiteral("X-KDE-ConfigModule")), QStringLiteral("kcm_rayedecoration"));

        const auto factory = KPluginFactory::loadFactory(meta);
        QVERIFY2(factory, qPrintable(factory.errorString));
    }

    void configModules()
    {
        const KPluginMetaData decorationKcm(QStringLiteral(DECORATION_KCM));
        QVERIFY(decorationKcm.isValid());
        QCOMPARE(decorationKcm.pluginId(), QStringLiteral("kcm_rayedecoration"));
        const auto decorationFactory = KPluginFactory::loadFactory(decorationKcm);
        QVERIFY2(decorationFactory, qPrintable(decorationFactory.errorString));

        const KPluginMetaData styleKcm(QStringLiteral(STYLE_KCM));
        QVERIFY(styleKcm.isValid());
        QCOMPARE(styleKcm.pluginId(), QStringLiteral("rayestyleconfig"));
        const auto styleFactory = KPluginFactory::loadFactory(styleKcm);
        QVERIFY2(styleFactory, qPrintable(styleFactory.errorString));
    }
};

QTEST_MAIN(TestRayePlugins)
#include "tst_rayeplugins.moc"
