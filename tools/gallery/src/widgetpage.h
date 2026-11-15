// Layout shared by the Qt Widgets pages: a title, then rows of samples
// with a caption on the left, in a scroll area.

#pragma once

#include "page.h"

#include <QList>

class QGridLayout;
class QLabel;

class WidgetPage : public GalleryPage
{
    Q_OBJECT

public:
    explicit WidgetPage(const QString &title, QWidget *parent = nullptr);

    // one row: the caption, then the samples side by side
    void addRow(const QString &caption, const QList<QWidget *> &samples);
    // one row whose sample takes the whole width
    void addWide(const QString &caption, QWidget *sample);

    using GalleryPage::setFocusSample;
    using GalleryPage::setHoverSample;

private:
    QGridLayout *m_grid = nullptr;
    int m_rows = 0;
};

QLabel *captionLabel(const QString &text);
