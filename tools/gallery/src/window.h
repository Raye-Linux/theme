// The gallery window: pages on the left, the open page on the right, and
// a bar with the Night/Day switch, the scale and what draws the controls.

#pragma once

#include "options.h"
#include "page.h"

#include <QHash>
#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QStackedWidget;
class SchemeManager;

class GalleryWindow : public QWidget
{
    Q_OBJECT

public:
    GalleryWindow(const GalleryOptions &options, SchemeManager *schemes, const PageContext &context,
                  QWidget *parent = nullptr);

    bool showPage(const QString &id);
    GalleryPage *currentPage() const;
    QString currentPageId() const;

private:
    void buildBar();
    void buildNavigation();
    void updateStatus();
    void relaunchAtScale(double scale);

    GalleryOptions m_options;
    SchemeManager *m_schemes = nullptr;
    PageContext m_context;
    QListWidget *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;
    QComboBox *m_scheme = nullptr;
    QComboBox *m_scale = nullptr;
    QLabel *m_status = nullptr;
    QHash<QString, GalleryPage *> m_pages;
    QString m_current;
};
