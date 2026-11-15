#include "gallery.h"
#include "scheme.h"
#include "window.h"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QImage>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

namespace
{
const QString themeDir = QStringLiteral(RAYE_THEME_DIR);

GalleryOptions repoOptions()
{
    GalleryOptions o;
    o.colorsDir = themeDir + QStringLiteral("/colors");
    o.qmlImportDir = themeDir + QStringLiteral("/qml");
    return o;
}

bool isInside(QWidget *child, QWidget *parent)
{
    for (QWidget *w = child; w; w = w->parentWidget()) {
        if (w == parent) {
            return true;
        }
    }
    return false;
}
}

class TestPages : public QObject
{
    Q_OBJECT

private:
    GallerySetup m_setup;

private Q_SLOTS:
    void initTestCase()
    {
        m_setup = setupApplication(repoOptions());
        QVERIFY2(m_setup.error.isEmpty(), qPrintable(m_setup.error));
        QVERIFY(m_setup.schemes->applied());
        QCOMPARE(QApplication::font().family(), QStringLiteral("Inter"));
    }

    void registry()
    {
        static const QRegularExpression idShape(QStringLiteral("^[a-z]-[a-z]+$"));
        QStringList ids;
        for (const PageInfo &page : galleryPages()) {
            QVERIFY2(idShape.match(page.id).hasMatch(), qPrintable(page.id));
            QVERIFY2(!ids.contains(page.id), qPrintable(page.id));
            ids << page.id;
            QVERIFY(!page.title.isEmpty());
            QVERIFY(!page.section.isEmpty());
            QVERIFY(page.make);
            if (!page.qmlFile.isEmpty()) {
                QVERIFY2(QFile::exists(QStringLiteral(":/gallery/qml/") + page.qmlFile), qPrintable(page.qmlFile));
            }
            QCOMPARE(findPage(page.id), &page);
        }
        QVERIFY(!findPage(QStringLiteral("nothing")));
        // both halves of the gallery are there
        QVERIFY(ids.filter(QRegularExpression(QStringLiteral("^w-"))).size() >= 8);
        QVERIFY(ids.filter(QRegularExpression(QStringLiteral("^q-"))).size() >= 8);
    }

    void everyPage_data()
    {
        QTest::addColumn<QString>("id");
        for (const PageInfo &page : galleryPages()) {
            QTest::newRow(qPrintable(page.id)) << page.id;
        }
    }

    void everyPage()
    {
        QFETCH(QString, id);
        GalleryWindow window(repoOptions(), m_setup.schemes, m_setup.context);
        QVERIFY(window.showPage(id));
        window.resize(1280, 800);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(50);

        GalleryPage *page = window.currentPage();
        QVERIFY(page);
        QCOMPARE(window.currentPageId(), id);
        QVERIFY2(page->problems().isEmpty(), qPrintable(page->problems().join(QLatin1Char('\n'))));
        // warnings from org.raye.ui itself belong to that module's tests:
        // shown here, never a failure of the gallery
        QStringList warnings;
        for (const QString &w : page->warnings()) {
            if (id == QLatin1String("q-raye") && w.contains(QLatin1String("/org/raye/ui/"))) {
                qInfo("org.raye.ui: %s", qPrintable(w));
            } else {
                warnings << w;
            }
        }
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(QLatin1Char('\n'))));

        // reference pages, and Raye's pieces while the module is not there
        if (id.startsWith(QLatin1String("r-")) || id == QLatin1String("q-raye")) {
            return;
        }
        // every control page has a hover and a focus sample
        page->openPopups();
        QTest::qWait(50);
        QVERIFY2(page->hoverRect().isValid(), "no hover sample");
        page->focusSample();
        QTest::qWait(20);
        QVERIFY2(isInside(QApplication::focusWidget(), page), "no focus sample");
        page->closePopups();
    }

    void unknownPage()
    {
        GalleryWindow window(repoOptions(), m_setup.schemes, m_setup.context);
        QVERIFY(!window.showPage(QStringLiteral("x-nothing")));
        QVERIFY(!window.currentPage());
    }

    void schemeSwitch()
    {
        GalleryWindow window(repoOptions(), m_setup.schemes, m_setup.context);
        window.showPage(QStringLiteral("w-buttons"));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QComboBox *combo = nullptr;
        for (auto *c : window.findChildren<QComboBox *>()) {
            if (c->accessibleName() == QLatin1String("Colour scheme")) {
                combo = c;
            }
        }
        QVERIFY(combo);
        const QColor night = QApplication::palette().color(QPalette::Window);
        combo->setCurrentIndex(1);
        QCOMPARE(m_setup.schemes->current(), Scheme::Day);
        QVERIFY(QApplication::palette().color(QPalette::Window) != night);
        QVERIFY(QApplication::palette().color(QPalette::Window).lightness() > night.lightness());
        combo->setCurrentIndex(0);
        QCOMPARE(m_setup.schemes->current(), Scheme::Night);
        QCOMPARE(QApplication::palette().color(QPalette::Window), night);
    }

    void shotIsSaved()
    {
        QTemporaryDir tmp;
        const QString file = tmp.filePath(QStringLiteral("shot.png"));
        GalleryWindow window(repoOptions(), m_setup.schemes, m_setup.context);
        window.showPage(QStringLiteral("w-menus"));
        window.setGeometry(window.screen()->geometry());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        int code = -1;
        scheduleShot(&window, file, [&code](int c) {
            code = c;
        });
        QTRY_VERIFY_WITH_TIMEOUT(code != -1, 10000);
        QCOMPARE(code, int(ShotSaved));
        const QImage image(file);
        QVERIFY(!image.isNull());
        QCOMPARE(image.size(), window.screen()->geometry().size() * window.screen()->devicePixelRatio());
        window.currentPage()->closePopups();
    }
};

int main(int argc, char **argv)
{
    prepareEnvironment(GalleryOptions{});
    QApplication app(argc, argv);
    TestPages test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_pages.moc"
