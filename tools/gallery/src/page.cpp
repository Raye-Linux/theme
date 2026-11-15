#include "page.h"

QRect GalleryPage::hoverRect() const
{
    if (!m_hover || !m_hover->isVisible()) {
        return {};
    }
    return QRect(m_hover->mapToGlobal(QPoint(0, 0)), m_hover->size());
}

void GalleryPage::focusSample()
{
    if (m_focus) {
        // what a real Tab press sets: styles draw the focus ring only then
        m_focus->window()->setAttribute(Qt::WA_KeyboardFocusChange);
        m_focus->setFocus(Qt::TabFocusReason);
    }
}

QWidget *GalleryPage::hoverSample() const
{
    return m_hover;
}

QWidget *GalleryPage::focusSampleWidget() const
{
    return m_focus;
}

void GalleryPage::setHoverSample(QWidget *widget)
{
    m_hover = widget;
}

void GalleryPage::setFocusSample(QWidget *widget)
{
    m_focus = widget;
}

namespace
{
std::function<GalleryPage *(const PageContext &)> qml(const QString &file)
{
    return [file](const PageContext &context) {
        return makeQmlPage(file, context);
    };
}
}

const QList<PageInfo> &galleryPages()
{
    const QString widgets = QStringLiteral("Qt Widgets");
    const QString quick = QStringLiteral("Qt Quick");
    const QString reference = QStringLiteral("Reference");
    static const QList<PageInfo> pages = {
        {QStringLiteral("w-buttons"), QStringLiteral("Buttons"), widgets, {}, makeWidgetButtons},
        {QStringLiteral("w-text"), QStringLiteral("Text and number fields"), widgets, {}, makeWidgetText},
        {QStringLiteral("w-choices"), QStringLiteral("Check boxes, radios, combo boxes"), widgets, {}, makeWidgetChoices},
        {QStringLiteral("w-ranges"), QStringLiteral("Sliders, progress, scroll bars"), widgets, {}, makeWidgetRanges},
        {QStringLiteral("w-lists"), QStringLiteral("Lists, trees, tables"), widgets, {}, makeWidgetLists},
        {QStringLiteral("w-menus"), QStringLiteral("Menus and tool bars"), widgets, {}, makeWidgetMenus},
        {QStringLiteral("w-containers"), QStringLiteral("Tabs, frames, group boxes"), widgets, {}, makeWidgetContainers},
        {QStringLiteral("w-dialogs"), QStringLiteral("Dialogs and tooltips"), widgets, {}, makeWidgetDialogs},
        {QStringLiteral("q-buttons"), QStringLiteral("Buttons"), quick, QStringLiteral("QButtons.qml"),
         qml(QStringLiteral("QButtons.qml"))},
        {QStringLiteral("q-text"), QStringLiteral("Text and number fields"), quick, QStringLiteral("QText.qml"),
         qml(QStringLiteral("QText.qml"))},
        {QStringLiteral("q-choices"), QStringLiteral("Check boxes, radios, switches"), quick, QStringLiteral("QChoices.qml"),
         qml(QStringLiteral("QChoices.qml"))},
        {QStringLiteral("q-ranges"), QStringLiteral("Sliders and progress"), quick, QStringLiteral("QRanges.qml"),
         qml(QStringLiteral("QRanges.qml"))},
        {QStringLiteral("q-lists"), QStringLiteral("Lists, tabs, scroll bars"), quick, QStringLiteral("QLists.qml"),
         qml(QStringLiteral("QLists.qml"))},
        {QStringLiteral("q-menus"), QStringLiteral("Menus and tool bars"), quick, QStringLiteral("QMenus.qml"),
         qml(QStringLiteral("QMenus.qml"))},
        {QStringLiteral("q-dialogs"), QStringLiteral("Dialogs and tooltips"), quick, QStringLiteral("QDialogs.qml"),
         qml(QStringLiteral("QDialogs.qml"))},
        {QStringLiteral("q-raye"), QStringLiteral("Raye QML pieces"), quick, QStringLiteral("QRaye.qml"),
         qml(QStringLiteral("QRaye.qml"))},
        {QStringLiteral("r-colours"), QStringLiteral("Colour scheme"), reference, {}, makeColoursPage},
        {QStringLiteral("r-type"), QStringLiteral("Type ramp"), reference, {}, makeTypePage},
    };
    return pages;
}

const PageInfo *findPage(const QString &id)
{
    for (const PageInfo &page : galleryPages()) {
        if (page.id == id) {
            return &page;
        }
    }
    return nullptr;
}
