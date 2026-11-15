/*
 * SPDX-FileCopyrightText: 2026 techoraye
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// The style reads its colours from the palette the KDE colour scheme
// writes. These checks feed it the Raye Night and Raye Day palettes and
// compare what it derives with the token table (theming spec 3.1).

#include "rayetokens.h"

#include <QPalette>
#include <QTest>

#include <cmath>

using namespace Raye;

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

QPalette dayPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(0xF6, 0xF3, 0xFB));
    p.setColor(QPalette::WindowText, QColor(0x1A, 0x10, 0x28));
    p.setColor(QPalette::Base, QColor(0xFC, 0xFB, 0xFE));
    p.setColor(QPalette::AlternateBase, QColor(0xFF, 0xFF, 0xFF));
    p.setColor(QPalette::Text, QColor(0x1A, 0x10, 0x28));
    p.setColor(QPalette::Button, QColor(0xFD, 0xFC, 0xFE));
    p.setColor(QPalette::ButtonText, QColor(0x1A, 0x10, 0x28));
    p.setColor(QPalette::Highlight, QColor(0x7B, 0x3C, 0xFF));
    p.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
    p.setColor(QPalette::Link, QColor(0x6A, 0x2F, 0xE6));
    p.setColor(QPalette::PlaceholderText, QColor(0x57, 0x4A, 0x6D));
    return p;
}

// a solid colour within `tolerance` per channel of the table value
bool near(const QColor &got, const char *want, int tolerance)
{
    const QColor w(QString::fromLatin1(want));
    return qAbs(got.red() - w.red()) <= tolerance && qAbs(got.green() - w.green()) <= tolerance && qAbs(got.blue() - w.blue()) <= tolerance;
}

// WCAG 2 contrast, worked out here rather than read from the style, so a
// mistake in the style's own arithmetic cannot make its tests agree with it
qreal ratio(const QColor &a, const QColor &b)
{
    auto relative = [](const QColor &c) {
        auto channel = [](qreal v) {
            return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
    };
    const qreal la = relative(a);
    const qreal lb = relative(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

// what a translucent colour looks like once drawn over `base`
QColor over(const QColor &base, const QColor &top)
{
    const qreal a = top.alphaF();
    return QColor::fromRgbF(base.redF() + (top.redF() - base.redF()) * a,
                            base.greenF() + (top.greenF() - base.greenF()) * a,
                            base.blueF() + (top.blueF() - base.blueF()) * a);
}

#define NEAR(got, want, tol)                                                                                                                                   \
    QVERIFY2(near(got, want, tol), qPrintable(QStringLiteral("%1 is not near %2").arg((got).name(), QStringLiteral(want))))
}

class TestRayeTokens : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mode()
    {
        QVERIFY(Tokens::isDark(nightPalette()));
        QVERIFY(!Tokens::isDark(dayPalette()));
    }

    void nightControls()
    {
        const QPalette p = nightPalette();
        NEAR(Tokens::controlFill(p), "#261F38", 0);
        NEAR(Tokens::controlHover(p), "#2D2541", 3);
        NEAR(Tokens::controlPressed(p), "#211A31", 3);
        NEAR(Tokens::controlDisabled(p), "#1D1729", 3);
        NEAR(Tokens::strokeStrong(p), "#8D7FA8", 4);
        NEAR(Tokens::inputActive(p), "#0F0B19", 2);
        // the table gives the stroke as white at 7 %; its solid value is only an approximation
        QCOMPARE(qRound(Tokens::strokeControl(p).alphaF() * 100), 7);
        QCOMPARE(Tokens::strokeControl(p).rgb(), QColor(Qt::white).rgb());
        QCOMPARE(qRound(Tokens::subtleHover(p).alphaF() * 100), 6);
        QCOMPARE(qRound(Tokens::subtleSelected(p).alphaF() * 100), 18);
    }

    void dayControls()
    {
        const QPalette p = dayPalette();
        NEAR(Tokens::controlFill(p), "#FDFCFE", 0);
        NEAR(Tokens::controlHover(p), "#F8F5FC", 4);
        NEAR(Tokens::controlPressed(p), "#F3EFF9", 6);
        NEAR(Tokens::strokeStrong(p), "#8A7CA3", 7);
        NEAR(Tokens::inputActive(p), "#FFFFFF", 0);
        NEAR(over(p.color(QPalette::Window), Tokens::strokeBottom(p)), "#D2C8E0", 8);
        QCOMPARE(qRound(Tokens::subtleSelected(p).alphaF() * 100), 10);
    }

    void text()
    {
        NEAR(Tokens::textSecondary(nightPalette()), "#B9A8D6", 0);
        NEAR(Tokens::textTertiary(nightPalette()), "#8C7DA6", 4);
        NEAR(Tokens::textDisabled(nightPalette()), "#5F5476", 4);
        NEAR(Tokens::textDisabled(dayPalette()), "#ADA2BE", 6);
        NEAR(Tokens::textSecondary(dayPalette()), "#574A6D", 6);
        NEAR(Tokens::textTertiary(dayPalette()), "#7A6D8F", 6);
    }

    void defaultAccentKeepsTableShades()
    {
        // the table's value, not the plain 12 % mix: #8B52FF left white
        // text at 4.42:1 on a hovered accent button, #8950FF gives 4.51:1
        NEAR(Tokens::accentHover(nightPalette()), "#8950FF", 0);
        NEAR(Tokens::accentPressed(nightPalette()), "#6A2FE6", 0);
        NEAR(Tokens::accentHover(dayPalette()), "#6C2FF0", 0);
        NEAR(Tokens::accentPressed(dayPalette()), "#5F27D6", 0);
    }

    void userAccentFollowsTheRule()
    {
        QPalette p = nightPalette();
        p.setColor(QPalette::Highlight, QColor(0x20, 0x80, 0x40));
        // This used to expect #3B8F57, a plain 12 % toward white, and
        // #3B8F57 leaves the scheme's own HighlightedText at 4.00:1 --
        // less than the 4.97:1 the accent itself gives. The amount is
        // walked back until the text holds; here it goes to zero, so the
        // hover follows the pressed shade at half its change instead,
        // which is what keeps a hovered state visible at all.
        NEAR(Tokens::accentHover(p), "#1E783C", 1);
        NEAR(Tokens::accentPressed(p), "#1C7138", 1);
        p = dayPalette();
        p.setColor(QPalette::Highlight, QColor(0x20, 0x80, 0x40));
        // day: hover 12 % toward black, pressed 24 % toward black
        NEAR(Tokens::accentHover(p), "#1C7138", 1);
        NEAR(Tokens::accentPressed(p), "#186131", 1);
    }

    //* the ratio a fraction cannot promise: text on a hovered accent fill
    void accentStatesKeepTheirText()
    {
        // #CC33CC is the hard case: NEITHER white nor the dark text
        // reaches 4.5:1 on it -- the ceiling is 4.28:1 -- so the floor is
        // what the accent itself gives, and the states may not lose it.
        // A plain 24 % toward black gave #9B279B, which left the dark
        // text at 2.75:1; the rule turns the other way and gives 5.86:1.
        QPalette p = dayPalette();
        p.setColor(QPalette::Highlight, QColor(0xCC, 0x33, 0xCC));
        p.setColor(QPalette::HighlightedText, QColor(0x1A, 0x10, 0x28));
        NEAR(Tokens::accentPressed(p), "#D864D8", 2);
        NEAR(Tokens::accentHover(p), "#D24BD2", 2);

        // and over a sweep of accents, in both modes, the text the scheme
        // writes never reads worse on a state than on the accent itself
        const QColor accents[] = {QColor(0x7B, 0x3C, 0xFF), QColor(0x00, 0x78, 0xD4),
                                  QColor(0x10, 0x7C, 0x10), QColor(0xC5, 0x0F, 0x1F),
                                  QColor(0xFF, 0xB9, 0x00), QColor(0xE5, 0xA0, 0x00),
                                  QColor(0xE3, 0x00, 0x8C), QColor(0x80, 0x80, 0x80),
                                  QColor(0x7A, 0x7A, 0x7A), QColor(0x00, 0xFF, 0xFF),
                                  QColor(0xFF, 0xFF, 0x00), QColor(0x00, 0x00, 0x00),
                                  QColor(0xFF, 0xFF, 0xFF)};
        // The text is the one accent_rule.on_accent would have chosen,
        // because that is the one the scheme writes as HighlightedText.
        // Feeding the other one instead asks the style to hold a ratio for
        // a pairing no scheme produces -- and for the default accent,
        // whose shades are the table's fixed ones, it is unanswerable.
        const QColor light(Qt::white);
        const QColor dark(0x1A, 0x10, 0x28);
        for (const QPalette &base : {nightPalette(), dayPalette()}) {
            for (const QColor &accent : accents) {
                const QColor text = ratio(light, accent) >= 4.5 ? light
                    : ratio(dark, accent) >= 4.5                ? dark
                    : ratio(light, accent) >= ratio(dark, accent) ? light
                                                                  : dark;
                QPalette q = base;
                q.setColor(QPalette::Highlight, accent);
                q.setColor(QPalette::HighlightedText, text);
                const qreal floor = qMin(4.5, ratio(text, accent));
                for (const QColor &state : {Tokens::accentHover(q), Tokens::accentPressed(q)}) {
                    QVERIFY2(ratio(text, state) >= floor - 0.01,
                             qPrintable(QStringLiteral("%1 on %2 (accent %3) is %4:1, needs %5:1")
                                            .arg(text.name(), state.name(), accent.name())
                                            .arg(ratio(text, state), 0, 'f', 2)
                                            .arg(floor, 0, 'f', 2)));
                }
            }
        }
    }

    void focusRing()
    {
        QCOMPARE(Tokens::focusOuter(nightPalette()), QColor(0xF3, 0xEA, 0xFF));
        QCOMPARE(Tokens::focusOuter(dayPalette()), QColor(0x1A, 0x10, 0x28));
        QCOMPARE(qRound(Tokens::focusInner(nightPalette()).alphaF() * 100), 70);
        QCOMPARE(Tokens::focusInner(nightPalette()).rgb(), QColor(Qt::black).rgb());
        QCOMPARE(Tokens::focusInner(dayPalette()).rgb(), QColor(Qt::white).rgb());
        // the glow is a night thing only
        QVERIFY(Tokens::glow(nightPalette()).alpha() > 0);
        QCOMPARE(Tokens::glow(dayPalette()).alpha(), 0);
    }

    void disabledAccentFollowsTheRule()
    {
        // whatever the scheme writes as the disabled highlight
        QPalette p = nightPalette();
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x14, 0x10, 0x20));
        NEAR(Tokens::accent(p, QPalette::Disabled), "#3A2A5E", 8);
        NEAR(Tokens::accentDisabled(dayPalette()), "#C9B8F2", 8);
        NEAR(Tokens::accent(p), "#7B3CFF", 0);
        NEAR(Tokens::accent(p, QPalette::Inactive), "#7B3CFF", 0);
    }

    void radii()
    {
        QCOMPARE(Tokens::ControlRadius, 4);
        QCOMPARE(Tokens::OverlayRadius, 8);
        QCOMPARE(Tokens::FocusRadius, 7);
    }
};

QTEST_GUILESS_MAIN(TestRayeTokens)

#include "tst_rayetokens.moc"
