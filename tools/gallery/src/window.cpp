#include "window.h"
#include "scheme.h"

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

namespace
{
constexpr int navWidth = 260;
// the gallery's own list: denser than Windows' 36 so every page fits at 100 %
constexpr int navItemHeight = 30;
constexpr int navHeaderHeight = 28;
constexpr int navInset = 8;
constexpr int barMargin = 12;
const double scales[] = {1.0, 1.25, 1.5, 2.0};
}

GalleryWindow::GalleryWindow(const GalleryOptions &options, SchemeManager *schemes, const PageContext &context,
                             QWidget *parent)
    : QWidget(parent)
    , m_options(options)
    , m_schemes(schemes)
    , m_context(context)
{
    setWindowTitle(QStringLiteral("Raye gallery"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    buildBar();
    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    buildNavigation();
    m_stack = new QStackedWidget;
    auto *navPane = new QWidget;
    auto *navLayout = new QVBoxLayout(navPane);
    navLayout->setContentsMargins(navInset, navInset, navInset, navInset);
    navLayout->addWidget(m_nav);
    body->addWidget(navPane);
    body->addWidget(m_stack, 1);
    outer->addLayout(body, 1);

    if (m_schemes) {
        connect(m_schemes, &SchemeManager::schemeChanged, this, [this](Scheme) {
            updateStatus();
        });
    }
    updateStatus();
}

void GalleryWindow::buildBar()
{
    auto *bar = new QWidget;
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(barMargin, barMargin / 2, barMargin, barMargin / 2);

    auto *title = new QLabel(QStringLiteral("Raye gallery"));
    QFont f = title->font();
    f.setWeight(QFont::DemiBold);
    title->setFont(f);
    layout->addWidget(title);
    layout->addSpacing(barMargin);

    m_status = new QLabel;
    m_status->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_status, 1);

    m_scheme = new QComboBox;
    m_scheme->addItem(QStringLiteral("Night"), int(Scheme::Night));
    m_scheme->addItem(QStringLiteral("Day"), int(Scheme::Day));
    m_scheme->setCurrentIndex(m_options.scheme == Scheme::Day ? 1 : 0);
    m_scheme->setEnabled(m_schemes && !m_schemes->colorsDir().isEmpty());
    m_scheme->setAccessibleName(QStringLiteral("Colour scheme"));
    connect(m_scheme, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto scheme = Scheme(m_scheme->itemData(index).toInt());
        QString error;
        if (m_schemes && m_schemes->apply(scheme, &error)) {
            m_options.scheme = scheme;
            QIcon::setThemeName(pickIconTheme(scheme, installedIconThemes()));
            updateStatus();
        } else {
            m_status->setText(error);
        }
    });
    layout->addWidget(m_scheme);

    m_scale = new QComboBox;
    m_scale->setAccessibleName(QStringLiteral("Scale"));
    const double current = qApp->devicePixelRatio();
    for (double s : scales) {
        m_scale->addItem(QStringLiteral("%1 %").arg(qRound(s * 100)), s);
        if (qFuzzyCompare(s, current)) {
            m_scale->setCurrentIndex(m_scale->count() - 1);
        }
    }
    // a scale is fixed when the program starts: changing it starts it again
    connect(m_scale, &QComboBox::activated, this, [this](int index) {
        const double s = m_scale->itemData(index).toDouble();
        if (!qFuzzyCompare(s, qApp->devicePixelRatio())) {
            relaunchAtScale(s);
        }
    });
    layout->addWidget(m_scale);

    static_cast<QVBoxLayout *>(this->layout())->addWidget(bar);
}

void GalleryWindow::buildNavigation()
{
    m_nav = new QListWidget;
    m_nav->setFixedWidth(navWidth - 2 * navInset);
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // a side panel on the window colour, like a navigation pane
    m_nav->viewport()->setBackgroundRole(QPalette::Window);
    m_nav->setAccessibleName(QStringLiteral("Pages"));
    QString section;
    for (const PageInfo &info : galleryPages()) {
        if (info.section != section) {
            section = info.section;
            auto *header = new QListWidgetItem(section);
            header->setFlags(Qt::NoItemFlags);
            QFont f = m_nav->font();
            f.setWeight(QFont::DemiBold);
            header->setFont(f);
            header->setSizeHint(QSize(navWidth - 2 * navInset, navHeaderHeight));
            m_nav->addItem(header);
        }
        auto *item = new QListWidgetItem(info.title);
        item->setData(Qt::UserRole, info.id);
        item->setSizeHint(QSize(navWidth - 2 * navInset, navItemHeight));
        m_nav->addItem(item);
    }
    connect(m_nav, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item) {
            showPage(item->data(Qt::UserRole).toString());
        }
    });
}

bool GalleryWindow::showPage(const QString &id)
{
    const PageInfo *info = findPage(id);
    if (!info) {
        return false;
    }
    if (m_current == id) {
        return true;
    }
    if (GalleryPage *old = currentPage()) {
        old->closePopups();
    }
    GalleryPage *page = m_pages.value(id);
    if (!page) {
        page = info->make(m_context);
        m_pages.insert(id, page);
        m_stack->addWidget(page);
    }
    m_current = id;
    m_stack->setCurrentWidget(page);
    for (int i = 0; i < m_nav->count(); ++i) {
        if (m_nav->item(i)->data(Qt::UserRole).toString() == id) {
            const QSignalBlocker block(m_nav);
            m_nav->setCurrentRow(i);
            break;
        }
    }
    return true;
}

GalleryPage *GalleryWindow::currentPage() const
{
    return m_pages.value(m_current);
}

QString GalleryWindow::currentPageId() const
{
    return m_current;
}

void GalleryWindow::updateStatus()
{
    const QString scheme = m_schemes && m_schemes->applied() ? schemeName(m_schemes->current()) : QStringLiteral("no scheme");
    m_status->setText(QStringLiteral("Style %1 · %2 · icons %3 · %4 %")
                          .arg(QApplication::style()->name(), scheme,
                               QIcon::themeName().isEmpty() ? QStringLiteral("none") : QIcon::themeName())
                          .arg(qRound(qApp->devicePixelRatio() * 100)));
}

void GalleryWindow::relaunchAtScale(double scale)
{
    GalleryOptions next = m_options;
    next.page = m_current;
    // the new process gets --scale, which replaces QT_SCALE_FACTOR from here
    QProcess process;
    process.setProgram(QApplication::applicationFilePath());
    process.setArguments(relaunchArguments(next, scale));
    if (process.startDetached()) {
        QApplication::quit();
    } else {
        m_status->setText(QStringLiteral("could not start the gallery again"));
    }
}
