// A gallery page and the list of all pages.
//
// Every page shows a group of controls in each state it can be put in
// without a person at the mouse (rest, pressed, checked, disabled...).
// Hover, keyboard focus and open popups need the real pointer and focus,
// so a page names one sample for each and the capture code sets them:
// the pointer goes over hoverRect(), focusSample() gives keyboard focus,
// openPopups() opens the page's menu, dialog or tooltip.

#pragma once

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

class SchemeManager;

class GalleryPage : public QWidget
{
    Q_OBJECT

public:
    using QWidget::QWidget;

    // where the pointer goes, in global coordinates; empty: nowhere
    virtual QRect hoverRect() const;
    // gives the focus sample keyboard focus, as Tab would
    virtual void focusSample();
    virtual void openPopups()
    {
    }
    virtual void closePopups()
    {
    }
    // what went wrong while building the page (QML errors...)
    virtual QStringList problems() const
    {
        return {};
    }
    // what Qt warned about while the page ran (QML binding loops...)
    virtual QStringList warnings() const
    {
        return {};
    }

    QWidget *hoverSample() const;
    QWidget *focusSampleWidget() const;

protected:
    void setHoverSample(QWidget *widget);
    void setFocusSample(QWidget *widget);

private:
    QPointer<QWidget> m_hover;
    QPointer<QWidget> m_focus;
};

struct PageContext {
    SchemeManager *schemes = nullptr;
    QStringList qmlImportDirs;
};

struct PageInfo {
    QString id;
    QString title;
    QString section;
    // set for a QML page: the file in the program's resources
    QString qmlFile;
    std::function<GalleryPage *(const PageContext &)> make;
};

const QList<PageInfo> &galleryPages();
const PageInfo *findPage(const QString &id);

// builders, one file each
GalleryPage *makeWidgetButtons(const PageContext &);
GalleryPage *makeWidgetText(const PageContext &);
GalleryPage *makeWidgetChoices(const PageContext &);
GalleryPage *makeWidgetRanges(const PageContext &);
GalleryPage *makeWidgetLists(const PageContext &);
GalleryPage *makeWidgetMenus(const PageContext &);
GalleryPage *makeWidgetContainers(const PageContext &);
GalleryPage *makeWidgetDialogs(const PageContext &);
GalleryPage *makeQmlPage(const QString &qmlFile, const PageContext &);
GalleryPage *makeColoursPage(const PageContext &);
GalleryPage *makeTypePage(const PageContext &);
