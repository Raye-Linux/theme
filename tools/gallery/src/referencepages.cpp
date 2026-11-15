// Reference pages: the colours of the scheme in use and the type ramp.

#include "scheme.h"
#include "widgetpage.h"

#include <KColorScheme>
#include <QFontInfo>
#include <QLabel>
#include <QPainter>

#include <functional>

namespace
{
constexpr int cellWidth = 60;
constexpr int swatchHeight = 32;
constexpr int textHeight = 18;
constexpr int nameWidth = 104;
constexpr int headerHeight = 22;

struct Role {
    const char *name;
    std::function<QColor(const KColorScheme &)> color;
};

const QList<Role> &roles()
{
    using S = KColorScheme;
    static const QList<Role> list = {
        {"Normal", [](const S &s) { return s.background(S::NormalBackground).color(); }},
        {"Altern.", [](const S &s) { return s.background(S::AlternateBackground).color(); }},
        {"Text", [](const S &s) { return s.foreground(S::NormalText).color(); }},
        {"Inactive", [](const S &s) { return s.foreground(S::InactiveText).color(); }},
        {"Active", [](const S &s) { return s.foreground(S::ActiveText).color(); }},
        {"Link", [](const S &s) { return s.foreground(S::LinkText).color(); }},
        {"Negative", [](const S &s) { return s.foreground(S::NegativeText).color(); }},
        {"Neutral", [](const S &s) { return s.foreground(S::NeutralText).color(); }},
        {"Positive", [](const S &s) { return s.foreground(S::PositiveText).color(); }},
        {"Focus", [](const S &s) { return s.decoration(S::FocusColor).color(); }},
        {"Hover", [](const S &s) { return s.decoration(S::HoverColor).color(); }},
    };
    return list;
}

const QList<QPair<const char *, KColorScheme::ColorSet>> &sets()
{
    static const QList<QPair<const char *, KColorScheme::ColorSet>> list = {
        {"View", KColorScheme::View},
        {"Window", KColorScheme::Window},
        {"Button", KColorScheme::Button},
        {"Selection", KColorScheme::Selection},
        {"Tooltip", KColorScheme::Tooltip},
        {"Complementary", KColorScheme::Complementary},
        {"Header", KColorScheme::Header},
    };
    return list;
}

// Every colour set of the scheme in use, each role as a swatch with its
// hex value: repaints itself when Night or Day changes.
class SwatchGrid : public QWidget
{
public:
    explicit SwatchGrid(SchemeManager *schemes)
        : m_schemes(schemes)
    {
        const int rowHeight = swatchHeight + textHeight + 8;
        setMinimumSize(nameWidth + cellWidth * int(roles().size()), headerHeight + rowHeight * int(sets().size()));
        if (m_schemes) {
            connect(m_schemes, &SchemeManager::schemeChanged, this, qOverload<>(&QWidget::update));
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const KSharedConfigPtr config = m_schemes ? m_schemes->config() : KSharedConfigPtr();
        QFont small = font();
        small.setPixelSize(11);
        p.setFont(small);
        const int rowHeight = swatchHeight + textHeight + 8;

        for (int c = 0; c < roles().size(); ++c) {
            p.setPen(palette().color(QPalette::PlaceholderText));
            p.drawText(QRect(nameWidth + c * cellWidth, 0, cellWidth - 6, headerHeight), Qt::AlignLeft | Qt::AlignVCenter,
                       QLatin1String(roles().at(c).name));
        }
        for (int r = 0; r < sets().size(); ++r) {
            const KColorScheme scheme(QPalette::Active, sets().at(r).second, config);
            const int y = headerHeight + r * rowHeight;
            p.setPen(palette().color(QPalette::WindowText));
            p.drawText(QRect(0, y, nameWidth - 8, swatchHeight), Qt::AlignLeft | Qt::AlignVCenter,
                       QLatin1String(sets().at(r).first));
            for (int c = 0; c < roles().size(); ++c) {
                const QColor color = roles().at(c).color(scheme);
                const QRectF swatch(nameWidth + c * cellWidth + 0.5, y + 0.5, cellWidth - 7, swatchHeight - 1);
                p.setPen(QPen(palette().color(QPalette::Mid), 1));
                p.setBrush(color);
                p.drawRoundedRect(swatch, 4, 4);
                p.setPen(palette().color(QPalette::WindowText));
                p.drawText(QRect(nameWidth + c * cellWidth, y + swatchHeight + 2, cellWidth - 6, textHeight),
                           Qt::AlignLeft | Qt::AlignVCenter, color.name());
            }
        }
    }

private:
    SchemeManager *m_schemes = nullptr;
};

struct TypeToken {
    const char *name;
    double px;
    int line;
    QFont::Weight weight;
    const char *family;
};
}

GalleryPage *makeColoursPage(const PageContext &context)
{
    auto *page = new WidgetPage(QStringLiteral("Colour scheme"));
    auto *file = new QLabel(context.schemes && context.schemes->applied()
                                ? context.schemes->fileFor(context.schemes->current())
                                : QStringLiteral("no Raye colour scheme found: the style's own palette"));
    file->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (context.schemes) {
        QObject::connect(context.schemes, &SchemeManager::schemeChanged, file, [file, schemes = context.schemes](Scheme s) {
            file->setText(schemes->fileFor(s));
        });
    }
    page->addWide(QStringLiteral("File"), file);
    page->addWide(QStringLiteral("Colour sets"), new SwatchGrid(context.schemes));
    return page;
}

GalleryPage *makeTypePage(const PageContext &)
{
    // 3.4: Raye's type ramp in pixels
    static const TypeToken tokens[] = {
        {"caption", 12, 16, QFont::Normal, "Inter"},
        {"body", 13.33, 20, QFont::Normal, "Inter"},
        {"bodyStrong", 13.33, 20, QFont::DemiBold, "Inter"},
        {"bodyLarge", 17, 24, QFont::Normal, "Inter"},
        {"subtitle", 19, 26, QFont::DemiBold, "Inter"},
        {"title", 27, 34, QFont::DemiBold, "Inter Display"},
        {"titleLarge", 38, 48, QFont::DemiBold, "Inter Display"},
        {"display", 64, 80, QFont::DemiBold, "Inter Display"},
        {"windowTitle", 12, 16, QFont::Medium, "Inter"},
        {"mono", 13.33, 20, QFont::Normal, "Cascadia Mono"},
    };
    auto *page = new WidgetPage(QStringLiteral("Type ramp"));
    for (const TypeToken &t : tokens) {
        auto *sample = new QLabel(QStringLiteral("Your files are safe 0123"));
        QFont f(QLatin1String(t.family));
        // pixels at 96 dpi, so 13.33 px stays 10 pt like KDE's body font
        f.setPointSizeF(t.px * 72.0 / 96.0);
        f.setWeight(t.weight);
        sample->setFont(f);
        sample->setMinimumHeight(t.line);
        const QString caption =
            QStringLiteral("%1  %2/%3  %4").arg(QLatin1String(t.name)).arg(t.px).arg(t.line).arg(int(t.weight));
        QList<QWidget *> samples{sample};
        const QFontInfo info(f);
        if (info.family() != QLatin1String(t.family)) {
            auto *missing = new QLabel(QStringLiteral("%1 is missing, drawn with %2").arg(QLatin1String(t.family), info.family()));
            missing->setForegroundRole(QPalette::PlaceholderText);
            samples << missing;
        }
        page->addRow(caption, samples);
    }
    return page;
}
