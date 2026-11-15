/*
 * SPDX-FileCopyrightText: 2026 techoraye
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The Raye style draws Windows 11 sizes (theming spec 1.9, 3.3 and 3.12):
// 32 px controls and menu items, 20 px check boxes, thin scroll bars that
// float over the content, 3 px progress bars, tooltips after 500 ms.
// Sizes are asked from the style; a few pictures are checked pixel by pixel.

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QAbstractItemView>
#include <QListWidget>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOptionMenuItem>
#include <QTest>

namespace
{
QPalette nightPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(0x13, 0x0E, 0x1F));
    p.setColor(QPalette::WindowText, QColor(0xF3, 0xEA, 0xFF));
    p.setColor(QPalette::Base, QColor(0x1B, 0x15, 0x29));
    p.setColor(QPalette::AlternateBase, QColor(0x22, 0x1B, 0x33));
    p.setColor(QPalette::Text, QColor(0xF3, 0xEA, 0xFF));
    p.setColor(QPalette::Button, QColor(0x26, 0x1F, 0x38));
    p.setColor(QPalette::ButtonText, QColor(0xF3, 0xEA, 0xFF));
    p.setColor(QPalette::Highlight, QColor(0x7B, 0x3C, 0xFF));
    p.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
    p.setColor(QPalette::Link, QColor(0xB4, 0x8C, 0xFF));
    p.setColor(QPalette::PlaceholderText, QColor(0xB9, 0xA8, 0xD6));
    return p;
}

// number of rows of `image` that hold a pixel visibly different from `background`
int paintedRows(const QImage &image, const QColor &background)
{
    int rows = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c(image.pixelColor(x, y));
            if (qAbs(c.red() - background.red()) + qAbs(c.green() - background.green()) + qAbs(c.blue() - background.blue()) > 24) {
                ++rows;
                break;
            }
        }
    }
    return rows;
}
}

class TestRayeStyle : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStyle *style = QStyleFactory::create(QStringLiteral("Raye"));
        QVERIFY(style);
        QApplication::setStyle(style);
        QApplication::setPalette(nightPalette());
    }

    void controlsAre32PixelsTall()
    {
        QPushButton button(QStringLiteral("Restart now"));
        QCOMPARE(button.sizeHint().height(), 32);
        QLineEdit edit;
        QCOMPARE(edit.sizeHint().height(), 32);
        QComboBox combo;
        combo.addItem(QStringLiteral("Recommended"));
        QCOMPARE(combo.sizeHint().height(), 32);
        // text has 11 px of padding left and right, and buttons are at least 64 wide
        QPushButton ok(QStringLiteral("OK"));
        QVERIFY(ok.sizeHint().width() >= 64);
    }

    void checkBoxIs20()
    {
        QCheckBox box(QStringLiteral("On"));
        QCOMPARE(box.style()->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, &box), 20);
        QCOMPARE(box.style()->pixelMetric(QStyle::PM_ExclusiveIndicatorHeight, nullptr, &box), 20);
        QCOMPARE(box.style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, &box), 8);
    }

    void menuItemsAre32()
    {
        QMenu menu;
        QStyleOptionMenuItem option;
        option.initFrom(&menu);
        option.menuItemType = QStyleOptionMenuItem::Normal;
        option.text = QStringLiteral("Copy\tCtrl+C");
        option.maxIconWidth = 16;
        const QSize size(menu.style()->sizeFromContents(QStyle::CT_MenuItem, &option, QSize(), &menu));
        QCOMPARE(size.height(), 32);
        // 4 px of padding around the items
        QCOMPARE(menu.style()->pixelMetric(QStyle::PM_MenuHMargin, nullptr, &menu), 4);
        QCOMPARE(menu.style()->pixelMetric(QStyle::PM_MenuVMargin, nullptr, &menu), 4);

        // a plain separator: a 1 px line with 4 px above and below
        option.menuItemType = QStyleOptionMenuItem::Separator;
        option.text.clear();
        QCOMPARE(menu.style()->sizeFromContents(QStyle::CT_MenuItem, &option, QSize(), &menu).height(), 9);
    }

    void listRowsAre32()
    {
        QListWidget list;
        list.addItem(QStringLiteral("Display"));
        list.show();
        QVERIFY(QTest::qWaitForWindowExposed(&list));
        QCOMPARE(list.visualItemRect(list.item(0)).height(), 32);
    }

    void scrollBarsAreThinAndFloat()
    {
        QScrollBar bar(Qt::Horizontal);
        QCOMPARE(bar.style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, &bar), 12);
        QVERIFY(bar.style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, &bar));
        // Qt Quick scroll bars are left alone
        QVERIFY(!bar.style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, nullptr));
    }

    void scrollBarAtRestIsAThinLine()
    {
        QWidget window;
        window.setAutoFillBackground(true);
        auto bar = new QScrollBar(Qt::Horizontal, &window);
        bar->setRange(0, 100);
        bar->setPageStep(30);
        bar->setGeometry(0, 0, 200, 12);
        window.resize(200, 12);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const QImage image(bar->grab().toImage());
        // only the 2 px resting line is painted, no separator, no track
        const int rows(paintedRows(image, nightPalette().color(QPalette::Window)));
        QVERIFY2(rows >= 1 && rows <= 3, qPrintable(QString::number(rows)));
        // and it sits 2 px from the outer (bottom) edge
        QVERIFY(paintedRows(image.copy(0, 0, 200, 7), nightPalette().color(QPalette::Window)) == 0);
    }

    void progressBarIs3()
    {
        QProgressBar bar;
        bar.setTextVisible(false);
        bar.setRange(0, 100);
        bar.setValue(50);
        bar.resize(200, 24);
        QStyleOptionProgressBar option;
        option.initFrom(&bar);
        option.state |= QStyle::State_Horizontal;
        option.minimum = 0;
        option.maximum = 100;
        option.progress = 50;
        option.textVisible = false;
        // the bar is 3 px, centred in whatever height the widget gets
        QCOMPARE(bar.style()->subElementRect(QStyle::SE_ProgressBarGroove, &option, &bar).height(), 3);
    }

    void tooltipTiming()
    {
        QWidget w;
        QCOMPARE(w.style()->styleHint(QStyle::SH_ToolTip_WakeUpDelay, nullptr, &w), 500);
        QCOMPARE(w.style()->styleHint(QStyle::SH_ToolTip_FallAsleepDelay, nullptr, &w), 100);
    }

    void comboListOpensOverTheField()
    {
        QComboBox combo;
        combo.addItem(QStringLiteral("Raye"));
        QStyleOptionComboBox option;
        option.initFrom(&combo);
        option.editable = false;
        QVERIFY(combo.style()->styleHint(QStyle::SH_ComboBox_Popup, &option, &combo));
        option.editable = true;
        QVERIFY(!combo.style()->styleHint(QStyle::SH_ComboBox_Popup, &option, &combo));
    }

    void comboListItemsAre32()
    {
        QComboBox combo;
        combo.addItems({QStringLiteral("Recommended"), QStringLiteral("Other"), QStringLiteral("Off")});
        combo.show();
        QVERIFY(QTest::qWaitForWindowExposed(&combo));
        combo.showPopup();
        QTRY_VERIFY(combo.view()->isVisible());
        // menu items, not menu items plus the list margins
        QCOMPARE(combo.view()->visualRect(combo.model()->index(0, 0)).height(), 32);
        combo.hidePopup();
    }
};

QTEST_MAIN(TestRayeStyle)

#include "tst_rayestyle.moc"
