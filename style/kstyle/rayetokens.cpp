/*
 * SPDX-FileCopyrightText: 2026 techoraye
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rayetokens.h"

#include <cmath>

namespace Raye
{
namespace Tokens
{
namespace
{
// The accent Raye ships with, and the shades accorded by eye for it --
// the values of the token table, which the colour schemes also carry.
// nightDefaultHover was 0x8B52FF, which is the plain 12 % toward white
// and NOT what the table says: the table deepens it to 0x8950FF exactly
// so that white text keeps 4.5:1 (0x8B52FF leaves 4.42:1). The schemes
// and Tokens.qml both carry 0x8950FF; this third copy had drifted, and it
// is the DEFAULT accent, so it was every user who never picked one.
// tokens/test_tokens.py ties these four constants to the table now.
const QColor defaultAccent(0x7B, 0x3C, 0xFF);
const QColor nightDefaultHover(0x89, 0x50, 0xFF);
const QColor nightDefaultPressed(0x6A, 0x2F, 0xE6);
const QColor dayDefaultHover(0x6C, 0x2F, 0xF0);
const QColor dayDefaultPressed(0x5F, 0x27, 0xD6);

const QColor glowColor(0xC0, 0x40, 0xFF);

QColor window(const QPalette &palette)
{
    return palette.color(QPalette::Active, QPalette::Window);
}

QColor windowText(const QPalette &palette)
{
    return palette.color(QPalette::Active, QPalette::WindowText);
}

bool sameRgb(const QColor &a, const QColor &b)
{
    return a.rgb() == b.rgb();
}

// --- WCAG 2 contrast, so a hovered accent keeps its text ---------------
//
// The hover and the pressed shade used to be a plain 12 % / 24 % mix
// toward white or black. A FRACTION guarantees no ratio: it landed well
// on the violet it was tuned for and nowhere else. Measured over 150
// accents in both modes, 103 state shades left the scheme's own
// HighlightedText below what the accent itself gives -- 2.75:1 where the
// accent gave 4.28:1 -- and an accent button draws that text on the fill
// (rayehelper.cpp: drawAccentButton, the check glyph).
//
// The amount is now walked back toward the accent until the text holds,
// exactly as accent_rule.on_accent in tokens/raye-tokens.json says and as
// tokens/generate.py (on_accent_text) and Tokens.qml (deriveAccent) do.
// The text itself comes from the palette rather than being chosen here:
// under a Raye scheme that IS the derived text.onAccent, and under any
// other scheme it is the ratio that scheme asked for.

qreal linearChannel(qreal v)
{
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

qreal luminance(const QColor &c)
{
    return 0.2126 * linearChannel(c.redF()) + 0.7152 * linearChannel(c.greenF())
        + 0.0722 * linearChannel(c.blueF());
}

qreal contrastRatio(const QColor &a, const QColor &b)
{
    const qreal la = luminance(a);
    const qreal lb = luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

//* accent_rule.on_accent: the ratio text keeps, and the step it backs off by
constexpr qreal OnAccentMinContrast = 4.5;
constexpr qreal BackOffStep = 0.005;

//* `amount` reduced until `text` holds `floor` on the accent mixed that far
qreal backOff(const QColor &base, const QColor &text, const QColor &toward, qreal amount, qreal floor)
{
    while (amount > 0 && contrastRatio(text, mix(base, toward, amount)) < floor) {
        // rounded like the Python and QML rules, so the three agree step for step
        amount = qMax(0.0, std::round((amount - BackOffStep) * 1e6) / 1e6);
    }
    return amount;
}

struct AccentStates {
    QColor hover;
    QColor pressed;
};

// Hover and pressed together, because keeping the text readable couples
// them: a pressed shade left with less than half its change goes the
// other way, and a hover left so follows pressed at half its change, so
// every state still shows.
AccentStates accentStates(const QPalette &palette, const QColor &base)
{
    const QColor text = palette.color(QPalette::Active, QPalette::HighlightedText);
    const bool dark = isDark(palette);
    // an accent no text holds 4.5:1 on: keep at least what the accent gives
    const qreal floor = qMin(OnAccentMinContrast, contrastRatio(text, base));

    const qreal wantHover = 0.12;
    const qreal wantPressed = dark ? 0.12 : 0.24;

    QColor pressedToward(Qt::black);
    qreal pressedBy = backOff(base, text, pressedToward, wantPressed, floor);
    if (pressedBy < wantPressed / 2) {
        const qreal flipped = backOff(base, text, Qt::white, wantPressed, floor);
        if (flipped >= wantPressed / 2) {
            pressedToward = QColor(Qt::white);
            pressedBy = flipped;
        }
    }

    // night: lighter on hover; day: darker, like the token rule
    QColor hoverToward(dark ? Qt::white : Qt::black);
    qreal hoverBy = backOff(base, text, hoverToward, wantHover, floor);
    if (hoverBy < wantHover / 2 && !sameRgb(pressedToward, hoverToward) && pressedBy > 0) {
        hoverToward = pressedToward;
        hoverBy = pressedBy / 2;
    } else if (sameRgb(pressedToward, hoverToward)) {
        // one way for both: hover sits halfway to pressed at most
        hoverBy = qMin(hoverBy, pressedBy / 2);
    }

    return {mix(base, hoverToward, hoverBy), mix(base, pressedToward, pressedBy)};
}
}

bool isDark(const QPalette &palette)
{
    // the window colour decides; text is the opposite of it in every scheme
    return qGray(window(palette).rgb()) < 128;
}

QColor mix(const QColor &a, const QColor &b, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * amount,
                            a.greenF() + (b.greenF() - a.greenF()) * amount,
                            a.blueF() + (b.blueF() - a.blueF()) * amount,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * amount)
        .toRgb();
}

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(qBound(0.0, alpha, 1.0));
    return color;
}

QColor ink(const QPalette &palette)
{
    return isDark(palette) ? QColor(Qt::white) : windowText(palette);
}

QColor controlFill(const QPalette &palette, QPalette::ColorGroup group)
{
    return palette.color(group, QPalette::Button);
}

QColor controlHover(const QPalette &palette)
{
    const QColor fill = controlFill(palette);
    return isDark(palette) ? mix(fill, Qt::white, 0.03) : mix(fill, ink(palette), 0.02);
}

QColor controlPressed(const QPalette &palette)
{
    const QColor fill = controlFill(palette);
    return isDark(palette) ? mix(fill, Qt::black, 0.14) : mix(fill, ink(palette), 0.04);
}

QColor controlDisabled(const QPalette &palette)
{
    return mix(controlFill(palette), window(palette), 0.5);
}

QColor inputActive(const QPalette &palette)
{
    return isDark(palette) ? mix(window(palette), Qt::black, 0.2) : QColor(Qt::white);
}

QColor subtleHover(const QPalette &palette)
{
    return withAlpha(ink(palette), isDark(palette) ? 0.06 : 0.04);
}

QColor subtlePressed(const QPalette &palette)
{
    return withAlpha(ink(palette), isDark(palette) ? 0.04 : 0.025);
}

QColor subtleSelected(const QPalette &palette)
{
    return withAlpha(accent(palette), isDark(palette) ? 0.18 : 0.10);
}

QColor strokeControl(const QPalette &palette)
{
    return withAlpha(ink(palette), isDark(palette) ? 0.07 : 0.08);
}

QColor strokeBottom(const QPalette &palette)
{
    return withAlpha(ink(palette), isDark(palette) ? 0.04 : 0.16);
}

QColor strokeStrong(const QPalette &palette, QPalette::ColorGroup group)
{
    const QColor strong = mix(window(palette), textSecondary(palette), isDark(palette) ? 0.74 : 0.66);
    return group == QPalette::Disabled ? textDisabled(palette) : strong;
}

QColor strokeCard(const QPalette &palette)
{
    return isDark(palette) ? withAlpha(Qt::black, 0.2) : withAlpha(ink(palette), 0.07);
}

QColor strokeSurface(const QPalette &palette)
{
    return withAlpha(ink(palette), isDark(palette) ? 0.09 : 0.10);
}

QColor divider(const QPalette &palette)
{
    return withAlpha(ink(palette), 0.08);
}

QColor bgTertiary(const QPalette &palette)
{
    return mix(window(palette), Qt::white, isDark(palette) ? 0.03 : 0.6);
}

QColor card(const QPalette &palette)
{
    return palette.color(QPalette::Active, QPalette::AlternateBase);
}

QColor textPrimary(const QPalette &palette, QPalette::ColorGroup group)
{
    return group == QPalette::Disabled ? textDisabled(palette) : palette.color(group, QPalette::WindowText);
}

QColor textSecondary(const QPalette &palette)
{
    // KColorScheme writes the scheme's inactive text (text.secondary,
    // violet tinted) as the placeholder colour; the other dim text
    // shades are taken from it so they keep the tint
    return palette.color(QPalette::Active, QPalette::PlaceholderText);
}

QColor textTertiary(const QPalette &palette)
{
    return mix(textSecondary(palette), window(palette), isDark(palette) ? 0.27 : 0.22);
}

QColor textDisabled(const QPalette &palette)
{
    return mix(window(palette), textSecondary(palette), 0.46);
}

QColor accent(const QPalette &palette, QPalette::ColorGroup group)
{
    // the disabled highlight KColorScheme writes is nearly the window
    // colour; a disabled accent follows the token rule instead
    if (group == QPalette::Disabled) {
        return accentDisabled(palette);
    }
    return palette.color(group, QPalette::Highlight);
}

QColor accentDisabled(const QPalette &palette)
{
    const QColor over = mix(window(palette), accent(palette), 0.35);
    const int grey = qRound(0.299 * over.red() + 0.587 * over.green() + 0.114 * over.blue());
    return mix(over, QColor(grey, grey, grey), 0.3);
}

QColor accentHover(const QPalette &palette)
{
    const QColor base = accent(palette);
    if (sameRgb(base, defaultAccent)) {
        return isDark(palette) ? nightDefaultHover : dayDefaultHover;
    }
    return accentStates(palette, base).hover;
}

QColor accentPressed(const QPalette &palette)
{
    const QColor base = accent(palette);
    if (sameRgb(base, defaultAccent)) {
        return isDark(palette) ? nightDefaultPressed : dayDefaultPressed;
    }
    return accentStates(palette, base).pressed;
}

QColor accentText(const QPalette &palette)
{
    return palette.color(QPalette::Active, QPalette::Link);
}

QColor onAccent(const QPalette &palette)
{
    return palette.color(QPalette::Active, QPalette::HighlightedText);
}

QColor focusOuter(const QPalette &palette)
{
    return windowText(palette);
}

QColor focusInner(const QPalette &palette)
{
    return withAlpha(isDark(palette) ? Qt::black : Qt::white, 0.7);
}

QColor glow(const QPalette &palette)
{
    return isDark(palette) ? withAlpha(glowColor, 0.35) : withAlpha(glowColor, 0.0);
}

QColor shadow(const QPalette &palette)
{
    return isDark(palette) ? QColor(Qt::black) : windowText(palette);
}
}
}
