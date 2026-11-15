/*
 * SPDX-FileCopyrightText: 2026 techoraye
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

// Raye colour roles, worked out from the palette the KDE colour scheme
// (Raye Night or Raye Day) writes, so the style repaints with the scheme
// and the user's accent. Names and values follow the token table of the
// theming spec (3.1); translucent roles are returned with their alpha
// and are meant to be drawn over whatever is under them.

#include <QColor>
#include <QPalette>

namespace Raye
{
namespace Tokens
{
//* corner radii at 100 % (theming spec 3.3)
constexpr int ControlRadius = 4;
constexpr int OverlayRadius = 8;
constexpr int SmallRadius = 2;
//* the focus ring sits 3 px outside the control
constexpr int FocusMargin = 3;
constexpr int FocusRadius = ControlRadius + FocusMargin;

//* true for a dark scheme (Raye Night), false for a light one (Raye Day)
bool isDark(const QPalette &palette);

//* `a` moved toward `b` by `amount` (0..1), like `b` drawn at that alpha
QColor mix(const QColor &a, const QColor &b, qreal amount);

//* `color` with its alpha set to `alpha`
QColor withAlpha(QColor color, qreal alpha);

//* the colour translucent strokes and fills are made of: white on night, the text colour on day
QColor ink(const QPalette &palette);

// control fills (button, combo box, text field)
QColor controlFill(const QPalette &, QPalette::ColorGroup = QPalette::Active);
QColor controlHover(const QPalette &);
QColor controlPressed(const QPalette &);
QColor controlDisabled(const QPalette &);
QColor inputActive(const QPalette &);

// translucent fills of lists, menus and tool buttons
QColor subtleHover(const QPalette &);
QColor subtlePressed(const QPalette &);
QColor subtleSelected(const QPalette &);

// strokes
QColor strokeControl(const QPalette &);
QColor strokeBottom(const QPalette &);
QColor strokeStrong(const QPalette &, QPalette::ColorGroup = QPalette::Active);
QColor strokeCard(const QPalette &);
QColor strokeSurface(const QPalette &);
QColor divider(const QPalette &);

// surfaces
QColor bgTertiary(const QPalette &);
QColor card(const QPalette &);

// text
QColor textPrimary(const QPalette &, QPalette::ColorGroup = QPalette::Active);
QColor textSecondary(const QPalette &);
QColor textTertiary(const QPalette &);
QColor textDisabled(const QPalette &);

// accent
QColor accent(const QPalette &, QPalette::ColorGroup = QPalette::Active);
QColor accentHover(const QPalette &);
QColor accentPressed(const QPalette &);
//* the accent of a disabled control: the accent at 35 % over the window, a little greyed
QColor accentDisabled(const QPalette &);
QColor accentText(const QPalette &);
QColor onAccent(const QPalette &);

// focus
QColor focusOuter(const QPalette &);
QColor focusInner(const QPalette &);
//* faint magenta glow outside the focus ring; transparent on day
QColor glow(const QPalette &);

//* colour of shadows
QColor shadow(const QPalette &);
}
}
