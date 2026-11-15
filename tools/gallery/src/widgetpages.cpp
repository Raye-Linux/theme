// Qt Widgets pages, first half: buttons, fields, choices, ranges.

#include "widgetpage.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCommandLinkButton>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QIcon>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QTimeEdit>
#include <QToolButton>

namespace
{
QPushButton *button(const QString &text, bool enabled = true)
{
    auto *b = new QPushButton(text);
    b->setEnabled(enabled);
    return b;
}

QMenu *sampleMenu(QWidget *parent)
{
    auto *menu = new QMenu(parent);
    menu->addAction(QIcon::fromTheme(QStringLiteral("document-open")), QStringLiteral("Open"));
    menu->addAction(QIcon::fromTheme(QStringLiteral("document-save")), QStringLiteral("Save"));
    menu->addSeparator();
    menu->addAction(QStringLiteral("Close"));
    return menu;
}

QToolButton *toolButton(const QString &icon, const QString &text, Qt::ToolButtonStyle style)
{
    auto *b = new QToolButton;
    b->setIcon(QIcon::fromTheme(icon));
    b->setText(text);
    b->setToolButtonStyle(style);
    b->setAutoRaise(true);
    return b;
}

QLineEdit *field(const QString &text, const QString &placeholder = {})
{
    auto *f = new QLineEdit(text);
    f->setPlaceholderText(placeholder);
    f->setMinimumWidth(160);
    return f;
}

QCheckBox *check(const QString &text, Qt::CheckState state, bool enabled = true)
{
    auto *c = new QCheckBox(text);
    c->setTristate(state == Qt::PartiallyChecked);
    c->setCheckState(state);
    c->setEnabled(enabled);
    return c;
}

QComboBox *combo(bool editable, bool enabled = true)
{
    auto *c = new QComboBox;
    c->addItems({QStringLiteral("Recommended"), QStringLiteral("Balanced"), QStringLiteral("Best performance")});
    c->setEditable(editable);
    c->setEnabled(enabled);
    return c;
}

QSlider *slider(Qt::Orientation orientation, int value, bool ticks = false, bool enabled = true)
{
    auto *s = new QSlider(orientation);
    s->setRange(0, 100);
    s->setValue(value);
    s->setEnabled(enabled);
    if (ticks) {
        s->setTickPosition(QSlider::TicksBelow);
        s->setTickInterval(10);
    }
    if (orientation == Qt::Horizontal) {
        s->setMinimumWidth(180);
    } else {
        s->setMinimumHeight(120);
    }
    return s;
}

QProgressBar *progress(int value, bool text = false, bool enabled = true)
{
    auto *p = new QProgressBar;
    if (value < 0) {
        // busy: no range
        p->setRange(0, 0);
    } else {
        p->setRange(0, 100);
        p->setValue(value);
    }
    p->setTextVisible(text);
    p->setEnabled(enabled);
    p->setMinimumWidth(160);
    return p;
}
}

GalleryPage *makeWidgetButtons(const PageContext &)
{
    auto *page = new WidgetPage(QStringLiteral("Buttons"));

    auto *hover = button(QStringLiteral("Hover"));
    hover->setToolTip(QStringLiteral("A tooltip shows after a short hover"));
    auto *pressed = button(QStringLiteral("Pressed"));
    pressed->setDown(true);
    auto *focused = button(QStringLiteral("Focused"));
    page->addRow(QStringLiteral("Button"),
                 {button(QStringLiteral("Rest")), hover, pressed, focused, button(QStringLiteral("Disabled"), false)});
    page->setHoverSample(hover);
    page->setFocusSample(focused);

    auto *accent = button(QStringLiteral("Restart now"));
    accent->setDefault(true);
    auto *accentPressed = button(QStringLiteral("Pressed"));
    accentPressed->setDefault(true);
    accentPressed->setDown(true);
    auto *accentOff = button(QStringLiteral("Disabled"), false);
    accentOff->setDefault(true);
    page->addRow(QStringLiteral("Accent (default) button"), {accent, accentPressed, accentOff});

    auto *withIcon = new QPushButton(QIcon::fromTheme(QStringLiteral("document-save")), QStringLiteral("Save"));
    auto *menuButton = button(QStringLiteral("More"));
    menuButton->setMenu(sampleMenu(menuButton));
    page->addRow(QStringLiteral("Icon, menu"), {withIcon, menuButton});

    auto *toggleOff = button(QStringLiteral("Off"));
    toggleOff->setCheckable(true);
    auto *toggleOn = button(QStringLiteral("On"));
    toggleOn->setCheckable(true);
    toggleOn->setChecked(true);
    auto *flat = button(QStringLiteral("Flat"));
    flat->setFlat(true);
    auto *flatOff = button(QStringLiteral("Flat disabled"), false);
    flatOff->setFlat(true);
    page->addRow(QStringLiteral("Toggle, flat"), {toggleOff, toggleOn, flat, flatOff});

    auto *tbChecked = toolButton(QStringLiteral("format-text-bold"), QStringLiteral("Bold"), Qt::ToolButtonIconOnly);
    tbChecked->setCheckable(true);
    tbChecked->setChecked(true);
    auto *tbPressed = toolButton(QStringLiteral("edit-copy"), QStringLiteral("Copy"), Qt::ToolButtonIconOnly);
    tbPressed->setDown(true);
    auto *tbOff = toolButton(QStringLiteral("edit-paste"), QStringLiteral("Paste"), Qt::ToolButtonIconOnly);
    tbOff->setEnabled(false);
    auto *tbMenu = toolButton(QStringLiteral("view-sort"), QStringLiteral("Sort"), Qt::ToolButtonTextBesideIcon);
    tbMenu->setMenu(sampleMenu(tbMenu));
    tbMenu->setPopupMode(QToolButton::MenuButtonPopup);
    auto *tbInstant = toolButton(QStringLiteral("view-list-details"), QStringLiteral("View"), Qt::ToolButtonTextBesideIcon);
    tbInstant->setMenu(sampleMenu(tbInstant));
    tbInstant->setPopupMode(QToolButton::InstantPopup);
    page->addRow(QStringLiteral("Tool buttons"),
                 {toolButton(QStringLiteral("edit-cut"), QStringLiteral("Cut"), Qt::ToolButtonIconOnly), tbChecked,
                  tbPressed, tbOff, tbMenu, tbInstant});

    auto *link = new QCommandLinkButton(QStringLiteral("Restart now"), QStringLiteral("Your files are safe. Raye opens them again."));
    page->addRow(QStringLiteral("Command link"), {link});

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    page->addRow(QStringLiteral("Dialog buttons"), {box});
    return page;
}

GalleryPage *makeWidgetText(const PageContext &)
{
    auto *page = new WidgetPage(QStringLiteral("Text and number fields"));

    auto *focused = field(QStringLiteral("Focused"));
    auto *hover = field(QStringLiteral("Hover"));
    page->addRow(QStringLiteral("Text field"), {field({}, QStringLiteral("Search")), hover, focused});
    page->setFocusSample(focused);
    page->setHoverSample(hover);

    auto *clear = field(QStringLiteral("With a clear button"));
    clear->setClearButtonEnabled(true);
    auto *readOnly = field(QStringLiteral("Read only"));
    readOnly->setReadOnly(true);
    auto *disabled = field(QStringLiteral("Disabled"));
    disabled->setEnabled(false);
    page->addRow(QStringLiteral("More states"), {clear, readOnly, disabled});

    auto *password = field(QStringLiteral("password"));
    password->setEchoMode(QLineEdit::Password);
    page->addRow(QStringLiteral("Password"), {password});

    auto *spin = new QSpinBox;
    spin->setRange(0, 200);
    spin->setValue(42);
    auto *dspin = new QDoubleSpinBox;
    dspin->setValue(1.25);
    dspin->setSuffix(QStringLiteral(" x"));
    auto *spinOff = new QSpinBox;
    spinOff->setValue(7);
    spinOff->setEnabled(false);
    page->addRow(QStringLiteral("Number field"), {spin, dspin, spinOff});

    auto *date = new QDateEdit(QDate(2026, 10, 31));
    date->setCalendarPopup(true);
    page->addRow(QStringLiteral("Date and time"), {date, new QTimeEdit(QTime(9, 41))});

    auto *edit = new QPlainTextEdit(QStringLiteral(
        "A text area with a few lines.\nSelect, type, scroll.\nThe frame and the focus line come from the style.\n"
        "Line four\nLine five\nLine six\nLine seven"));
    edit->setMaximumHeight(110);
    page->addWide(QStringLiteral("Text area"), edit);
    return page;
}

GalleryPage *makeWidgetChoices(const PageContext &)
{
    auto *page = new WidgetPage(QStringLiteral("Check boxes, radios, combo boxes"));

    auto *hover = check(QStringLiteral("Hover"), Qt::Unchecked);
    auto *focused = check(QStringLiteral("Focused"), Qt::Checked);
    page->addRow(QStringLiteral("Check box"),
                 {check(QStringLiteral("Off"), Qt::Unchecked), check(QStringLiteral("On"), Qt::Checked),
                  check(QStringLiteral("Mixed"), Qt::PartiallyChecked), hover, focused});
    page->setHoverSample(hover);
    page->setFocusSample(focused);
    page->addRow(QStringLiteral("Check box, disabled"),
                 {check(QStringLiteral("Off"), Qt::Unchecked, false), check(QStringLiteral("On"), Qt::Checked, false),
                  check(QStringLiteral("Mixed"), Qt::PartiallyChecked, false)});

    auto *group = new QButtonGroup(page);
    auto *off = new QRadioButton(QStringLiteral("Off"));
    auto *on = new QRadioButton(QStringLiteral("On"));
    on->setChecked(true);
    group->addButton(off);
    group->addButton(on);
    auto *offDisabled = new QRadioButton(QStringLiteral("Off, disabled"));
    offDisabled->setAutoExclusive(false);
    offDisabled->setEnabled(false);
    auto *onDisabled = new QRadioButton(QStringLiteral("On, disabled"));
    onDisabled->setAutoExclusive(false);
    onDisabled->setChecked(true);
    onDisabled->setEnabled(false);
    page->addRow(QStringLiteral("Radio button"), {off, on, offDisabled, onDisabled});

    page->addRow(QStringLiteral("Combo box"), {combo(false), combo(true), combo(false, false)});
    return page;
}

GalleryPage *makeWidgetRanges(const PageContext &)
{
    auto *page = new WidgetPage(QStringLiteral("Sliders, progress, scroll bars"));

    auto *hover = slider(Qt::Horizontal, 60);
    auto *focused = slider(Qt::Horizontal, 30);
    page->addRow(QStringLiteral("Slider"), {slider(Qt::Horizontal, 40), hover, focused});
    page->setHoverSample(hover);
    page->setFocusSample(focused);
    page->addRow(QStringLiteral("Ticks, disabled"),
                 {slider(Qt::Horizontal, 50, true), slider(Qt::Horizontal, 50, false, false)});
    page->addRow(QStringLiteral("Vertical"),
                 {slider(Qt::Vertical, 25), slider(Qt::Vertical, 75, true), slider(Qt::Vertical, 50, false, false)});

    page->addRow(QStringLiteral("Progress bar"), {progress(0), progress(30), progress(70), progress(100)});
    page->addRow(QStringLiteral("Busy, text, disabled"), {progress(-1), progress(45, true), progress(60, false, false)});

    auto *hbar = new QScrollBar(Qt::Horizontal);
    hbar->setRange(0, 100);
    hbar->setPageStep(30);
    hbar->setValue(20);
    hbar->setMinimumWidth(240);
    auto *vbar = new QScrollBar(Qt::Vertical);
    vbar->setRange(0, 100);
    vbar->setPageStep(20);
    vbar->setValue(50);
    vbar->setMinimumHeight(120);
    auto *off = new QScrollBar(Qt::Horizontal);
    off->setEnabled(false);
    off->setMinimumWidth(160);
    page->addRow(QStringLiteral("Scroll bar"), {hbar, vbar, off});
    return page;
}
