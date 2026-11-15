// Qt Widgets pages, second half: item views, menus, containers, dialogs.

#include "widgetpage.h"

#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
constexpr int listWidth = 260;
constexpr int listHeight = 200;

QListWidget *list(QAbstractItemView::SelectionMode mode)
{
    auto *l = new QListWidget;
    const QList<QPair<QString, QString>> rows = {
        {QStringLiteral("folder-documents"), QStringLiteral("Documents")},
        {QStringLiteral("folder-download"), QStringLiteral("Downloads")},
        {QStringLiteral("folder-music"), QStringLiteral("Music")},
        {QStringLiteral("folder-pictures"), QStringLiteral("Pictures")},
        {QStringLiteral("folder-videos"), QStringLiteral("Videos")},
        {QStringLiteral("user-trash"), QStringLiteral("Recycle Bin")},
    };
    for (const auto &[icon, text] : rows) {
        l->addItem(new QListWidgetItem(QIcon::fromTheme(icon), text));
    }
    l->setSelectionMode(mode);
    l->setFixedSize(listWidth, listHeight);
    l->item(5)->setFlags(l->item(5)->flags() & ~Qt::ItemIsEnabled);
    return l;
}

class ListsPage : public WidgetPage
{
public:
    ListsPage()
        : WidgetPage(QStringLiteral("Lists, trees, tables"))
    {
        m_list = list(QAbstractItemView::SingleSelection);
        m_list->setCurrentRow(0);
        auto *multi = list(QAbstractItemView::ExtendedSelection);
        multi->item(1)->setSelected(true);
        multi->item(2)->setSelected(true);
        addRow(QStringLiteral("List (single, multiple)"), {m_list, multi});
        setFocusSample(multi);

        auto *tree = new QTreeWidget;
        tree->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Size"), QStringLiteral("Modified")});
        auto *home = new QTreeWidgetItem(tree, {QStringLiteral("Home"), QString(), QStringLiteral("Today")});
        home->setIcon(0, QIcon::fromTheme(QStringLiteral("user-home")));
        for (const auto *name : {"Notes.txt", "Budget.ods", "Photo.jpg"}) {
            auto *item = new QTreeWidgetItem(home, {QLatin1String(name), QStringLiteral("24 KB"), QStringLiteral("Yesterday")});
            item->setIcon(0, QIcon::fromTheme(QStringLiteral("text-plain")));
        }
        auto *other = new QTreeWidgetItem(tree, {QStringLiteral("Projects"), QString(), QStringLiteral("Last week")});
        other->setIcon(0, QIcon::fromTheme(QStringLiteral("folder")));
        new QTreeWidgetItem(other, {QStringLiteral("raye"), QStringLiteral("2 MB"), QStringLiteral("Last week")});
        tree->expandAll();
        tree->setSortingEnabled(true);
        tree->sortByColumn(0, Qt::AscendingOrder);
        tree->setCurrentItem(home->child(1));
        tree->setMinimumHeight(200);
        tree->header()->resizeSection(0, 220);
        addWide(QStringLiteral("Tree with header"), tree);

        auto *table = new QTableWidget(4, 3);
        table->setHorizontalHeaderLabels({QStringLiteral("App"), QStringLiteral("Status"), QStringLiteral("Size")});
        const char *apps[] = {"Files", "Notepad", "Store", "Settings"};
        for (int r = 0; r < 4; ++r) {
            table->setItem(r, 0, new QTableWidgetItem(QLatin1String(apps[r])));
            table->setItem(r, 1, new QTableWidgetItem(r == 2 ? QStringLiteral("Update ready") : QStringLiteral("Up to date")));
            table->setItem(r, 2, new QTableWidgetItem(QStringLiteral("%1 MB").arg(12 + r * 7)));
        }
        table->setAlternatingRowColors(true);
        table->selectRow(1);
        table->setMinimumHeight(170);
        addWide(QStringLiteral("Table"), table);
    }

    // the pointer on the third row, so a row hover shows next to the selection
    QRect hoverRect() const override
    {
        const QRect r = m_list->visualItemRect(m_list->item(2));
        return QRect(m_list->viewport()->mapToGlobal(r.topLeft()), r.size());
    }

private:
    QListWidget *m_list = nullptr;
};

class MenusPage : public WidgetPage
{
public:
    MenusPage()
        : WidgetPage(QStringLiteral("Menus and tool bars"))
    {
        auto *bar = new QMenuBar;
        bar->setNativeMenuBar(false);
        for (const auto *name : {"&File", "&Edit", "&View", "&Help"}) {
            bar->addMenu(QLatin1String(name))->addAction(QStringLiteral("Item"));
        }
        addWide(QStringLiteral("Menu bar"), bar);

        auto *tools = new QToolBar;
        tools->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        tools->addAction(QIcon::fromTheme(QStringLiteral("document-new")), QStringLiteral("New"));
        tools->addAction(QIcon::fromTheme(QStringLiteral("document-open")), QStringLiteral("Open"));
        tools->addSeparator();
        auto *bold = tools->addAction(QIcon::fromTheme(QStringLiteral("format-text-bold")), QStringLiteral("Bold"));
        bold->setCheckable(true);
        bold->setChecked(true);
        auto *off = tools->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), QStringLiteral("Delete"));
        off->setEnabled(false);
        addWide(QStringLiteral("Tool bar"), tools);

        m_anchor = new QPushButton(QStringLiteral("Menu"));
        m_menu = new QMenu(m_anchor);
        m_menu->addSection(QStringLiteral("Clipboard"));
        m_menu->addAction(QIcon::fromTheme(QStringLiteral("edit-cut")), QStringLiteral("Cut"))->setShortcut(QKeySequence::Cut);
        m_hoverAction = m_menu->addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), QStringLiteral("Copy"));
        m_hoverAction->setShortcut(QKeySequence::Copy);
        auto *paste = m_menu->addAction(QIcon::fromTheme(QStringLiteral("edit-paste")), QStringLiteral("Paste"));
        paste->setShortcut(QKeySequence::Paste);
        paste->setEnabled(false);
        m_menu->addSeparator();
        auto *wrap = m_menu->addAction(QStringLiteral("Word wrap"));
        wrap->setCheckable(true);
        wrap->setChecked(true);
        auto *sub = m_menu->addMenu(QIcon::fromTheme(QStringLiteral("view-sort")), QStringLiteral("Sort by"));
        sub->addAction(QStringLiteral("Name"));
        sub->addAction(QStringLiteral("Date"));
        m_menu->addSeparator();
        m_menu->addAction(QIcon::fromTheme(QStringLiteral("document-properties")), QStringLiteral("Properties"));
        m_anchor->setMenu(m_menu);
        addRow(QStringLiteral("Menu (opened)"), {m_anchor});
        setFocusSample(m_anchor);
    }

    void openPopups() override
    {
        m_menu->popup(m_anchor->mapToGlobal(QPoint(0, m_anchor->height())));
    }

    void closePopups() override
    {
        m_menu->hide();
    }

    // the pointer on "Copy" in the open menu
    QRect hoverRect() const override
    {
        if (!m_menu->isVisible()) {
            return {};
        }
        const QRect r = m_menu->actionGeometry(m_hoverAction);
        return QRect(m_menu->mapToGlobal(r.topLeft()), r.size());
    }

private:
    QPushButton *m_anchor = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_hoverAction = nullptr;
};

class ContainersPage : public WidgetPage
{
public:
    ContainersPage()
        : WidgetPage(QStringLiteral("Tabs, frames, group boxes"))
    {
        auto *tabs = new QTabWidget;
        for (const auto *name : {"General", "Sharing", "Security"}) {
            auto *pane = new QLabel(QStringLiteral("Content of the %1 tab").arg(QLatin1String(name)));
            pane->setMargin(16);
            tabs->addTab(pane, QLatin1String(name));
        }
        tabs->setTabEnabled(2, false);
        tabs->setMinimumHeight(110);
        addWide(QStringLiteral("Tabs"), tabs);

        auto *docTabs = new QTabWidget;
        docTabs->setDocumentMode(true);
        docTabs->setTabsClosable(true);
        for (const auto *name : {"Notes.txt", "Budget.ods", "Untitled"}) {
            docTabs->addTab(new QLabel(QStringLiteral("  ") + QLatin1String(name)), QIcon::fromTheme(QStringLiteral("text-plain")),
                            QLatin1String(name));
        }
        docTabs->setCurrentIndex(1);
        docTabs->setMinimumHeight(80);
        addWide(QStringLiteral("Document tabs"), docTabs);

        auto *group = new QGroupBox(QStringLiteral("Group box"));
        auto *gl = new QVBoxLayout(group);
        gl->addWidget(new QLabel(QStringLiteral("Content inside a group box")));
        auto *checkable = new QGroupBox(QStringLiteral("Checkable group"));
        checkable->setCheckable(true);
        auto *cl = new QVBoxLayout(checkable);
        cl->addWidget(new QLabel(QStringLiteral("On or off with its title")));
        addRow(QStringLiteral("Group boxes"), {group, checkable});

        auto frame = [](QFrame::Shape shape, QFrame::Shadow shadow, const QString &text) {
            auto *f = new QFrame;
            f->setFrameStyle(shape | shadow);
            auto *l = new QVBoxLayout(f);
            l->addWidget(new QLabel(text));
            f->setMinimumSize(150, 60);
            return f;
        };
        addRow(QStringLiteral("Frames"),
               {frame(QFrame::StyledPanel, QFrame::Plain, QStringLiteral("Styled panel")),
                frame(QFrame::Box, QFrame::Sunken, QStringLiteral("Box, sunken")),
                frame(QFrame::Panel, QFrame::Raised, QStringLiteral("Panel, raised"))});

        m_scroll = new QScrollArea;
        auto *big = new QLabel(QStringLiteral("A scroll area wider and taller than its frame. "
                                              "Point at the scroll bars to see them grow."));
        big->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        big->setMinimumSize(900, 500);
        big->setMargin(12);
        m_scroll->setWidget(big);
        m_scroll->setFixedSize(420, 160);
        addRow(QStringLiteral("Scroll area"), {m_scroll});
        setFocusSample(tabs->tabBar());
    }

    QRect hoverRect() const override
    {
        QScrollBar *bar = m_scroll->verticalScrollBar();
        return QRect(bar->mapToGlobal(QPoint(0, 0)), bar->size());
    }

private:
    QScrollArea *m_scroll = nullptr;
};

class DialogsPage : public WidgetPage
{
public:
    DialogsPage()
        : WidgetPage(QStringLiteral("Dialogs and tooltips"))
    {
        auto *hover = new QPushButton(QStringLiteral("Point here"));
        hover->setToolTip(QStringLiteral("Tooltips wait a moment, then stay until the pointer moves away."));
        addRow(QStringLiteral("Tooltip"), {hover});
        setHoverSample(hover);

        auto *open = new QPushButton(QStringLiteral("Open dialog"));
        connect(open, &QPushButton::clicked, this, [this] {
            openPopups();
        });
        addRow(QStringLiteral("Message box"), {open});
        setFocusSample(open);

        auto *card = new QFrame;
        card->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
        auto *cl = new QGridLayout(card);
        auto *title = new QLabel(QStringLiteral("Delete 3 items?"));
        QFont f = title->font();
        f.setPixelSize(19);
        f.setWeight(QFont::DemiBold);
        title->setFont(f);
        cl->addWidget(title, 0, 0, 1, 2);
        cl->addWidget(new QLabel(QStringLiteral("They go to the Recycle Bin. You can restore them from there.")), 1, 0, 1, 2);
        auto *yes = new QPushButton(QStringLiteral("Delete"));
        yes->setDefault(true);
        cl->addWidget(yes, 2, 0);
        cl->addWidget(new QPushButton(QStringLiteral("Cancel")), 2, 1);
        card->setMaximumWidth(548);
        addWide(QStringLiteral("Dialog content"), card);
    }

    void openPopups() override
    {
        // tooltips of a window that lost activation to the dialog still show
        window()->setAttribute(Qt::WA_AlwaysShowToolTips);
        if (!m_box) {
            m_box = new QMessageBox(QMessageBox::Question, QStringLiteral("Raye Update"),
                                    QStringLiteral("Restart to finish updating?"), QMessageBox::NoButton, this);
            m_box->setInformativeText(QStringLiteral("Raye installed 12 updates. Your files are safe."));
            auto *restart = m_box->addButton(QStringLiteral("Restart now"), QMessageBox::AcceptRole);
            m_box->addButton(QStringLiteral("Later"), QMessageBox::RejectRole);
            m_box->setDefaultButton(restart);
            m_box->setWindowModality(Qt::NonModal);
        }
        m_box->adjustSize();
        // in the right half, clear of the samples on the left
        const QRect area(mapToGlobal(QPoint(width() / 2, 0)), QSize(width() / 2, height()));
        QRect r(QPoint(0, 0), m_box->sizeHint());
        r.moveCenter(area.center());
        m_box->move(r.topLeft());
        m_box->show();
    }

    void closePopups() override
    {
        if (m_box) {
            m_box->hide();
        }
    }

private:
    QPointer<QMessageBox> m_box;
};
}

GalleryPage *makeWidgetLists(const PageContext &)
{
    return new ListsPage;
}

GalleryPage *makeWidgetMenus(const PageContext &)
{
    return new MenusPage;
}

GalleryPage *makeWidgetContainers(const PageContext &)
{
    return new ContainersPage;
}

GalleryPage *makeWidgetDialogs(const PageContext &)
{
    return new DialogsPage;
}
