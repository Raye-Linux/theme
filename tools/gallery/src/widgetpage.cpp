#include "widgetpage.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace
{
// 3.2/3.4: page margins 24 (s7), rows 12 apart (s4), samples 8 apart (s3)
constexpr int pageMargin = 24;
constexpr int rowGap = 12;
constexpr int sampleGap = 8;
constexpr int captionWidth = 168;
// a caption lines up with the first line of controls (32 px high, 1.9)
constexpr int captionHeight = 32;
constexpr int subtitlePx = 19;
}

QLabel *captionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setMinimumWidth(captionWidth);
    label->setMinimumHeight(captionHeight);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

WidgetPage::WidgetPage(const QString &title, QWidget *parent)
    : GalleryPage(parent)
{
    setObjectName(title);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *heading = new QLabel(title);
    QFont f = heading->font();
    f.setPixelSize(subtitlePx);
    f.setWeight(QFont::DemiBold);
    heading->setFont(f);
    heading->setContentsMargins(pageMargin, pageMargin, pageMargin, rowGap);
    outer->addWidget(heading);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    content->setAutoFillBackground(false);
    scroll->viewport()->setAutoFillBackground(false);
    m_grid = new QGridLayout(content);
    m_grid->setContentsMargins(pageMargin, 0, pageMargin, pageMargin);
    m_grid->setHorizontalSpacing(sampleGap * 2);
    m_grid->setVerticalSpacing(rowGap);
    m_grid->setColumnStretch(1, 1);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
}

void WidgetPage::addRow(const QString &caption, const QList<QWidget *> &samples)
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(sampleGap);
    for (QWidget *sample : samples) {
        layout->addWidget(sample);
    }
    layout->addStretch(1);
    m_grid->addWidget(captionLabel(caption), m_rows, 0, Qt::AlignTop);
    m_grid->addWidget(row, m_rows, 1);
    m_grid->setRowStretch(m_rows, 0);
    ++m_rows;
    // keeps the rows at the top when the page is taller than its content
    m_grid->setRowStretch(m_rows, 1);
}

void WidgetPage::addWide(const QString &caption, QWidget *sample)
{
    m_grid->addWidget(captionLabel(caption), m_rows, 0, Qt::AlignTop);
    m_grid->addWidget(sample, m_rows, 1);
    m_grid->setRowStretch(m_rows, 0);
    ++m_rows;
    m_grid->setRowStretch(m_rows, 1);
}
