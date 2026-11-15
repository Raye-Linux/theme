/*
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rayehelper.h"

#include "raye.h"
#include "rayepropertynames.h"
#include "rayetokens.h"

#include <KColorScheme>
#include <KColorUtils>
#include <KIconLoader>
#include <KWindowSystem>
#include <qobject.h>
#if __has_include(<KX11Extras>)
#include <KX11Extras>
#endif

#include <QApplication>
#include <QDockWidget>
#include <QFileInfo>
#include <QMainWindow>
#include <QMdiArea>
#include <QMenuBar>
#include <QPainter>
#include <QStyleOption>
#include <QWindow>

#include <QDialog>
#include <algorithm>

namespace Raye
{
//* contrast for arrow and treeline rendering
static const qreal arrowShade = 0.15;

static const qreal highlightBackgroundAlpha = 0.33;

static const auto radioCheckSunkenDarkeningFactor = 110;

PaletteChangedEventFilter::PaletteChangedEventFilter(Helper *helper)
    : QObject(helper)
    , _helper(helper)
{
}

bool PaletteChangedEventFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::ApplicationPaletteChange || watched != qApp) {
        return QObject::eventFilter(watched, event);
    }
    if (!qApp->property("KDE_COLOR_SCHEME_PATH").isValid()) {
        return QObject::eventFilter(watched, event);
    }
    const auto path = qApp->property("KDE_COLOR_SCHEME_PATH").toString();
    if (!path.isEmpty()) {
        KConfig config(path, KConfig::SimpleConfig);
        KConfigGroup group(config.group(QStringLiteral("WM")));
        const QPalette palette(QApplication::palette());
        _helper->_activeTitleBarColor = group.readEntry("activeBackground", palette.color(QPalette::Active, QPalette::Highlight));
        _helper->_activeTitleBarTextColor = group.readEntry("activeForeground", palette.color(QPalette::Active, QPalette::HighlightedText));
        _helper->_inactiveTitleBarColor = group.readEntry("inactiveBackground", palette.color(QPalette::Disabled, QPalette::Highlight));
        _helper->_inactiveTitleBarTextColor = group.readEntry("inactiveForeground", palette.color(QPalette::Disabled, QPalette::HighlightedText));
    }
    return QObject::eventFilter(watched, event);
}

//____________________________________________________________________
Helper::Helper(KSharedConfig::Ptr config)
    : QObject()
    , _config(std::move(config))
    , _kwinConfig(KSharedConfig::openConfig("kwinrc"))
    , _decorationConfig(new InternalSettings())
    , _eventFilter(new PaletteChangedEventFilter(this))
{
}

//____________________________________________________________________
KSharedConfig::Ptr Helper::config() const
{
    return _config;
}

//____________________________________________________________________
QSharedPointer<InternalSettings> Helper::decorationConfig() const
{
    return _decorationConfig;
}

//____________________________________________________________________
void Helper::loadConfig()
{
    _viewFocusBrush = KStatefulBrush(KColorScheme::View, KColorScheme::FocusColor);
    _viewHoverBrush = KStatefulBrush(KColorScheme::View, KColorScheme::HoverColor);
    _buttonFocusBrush = KStatefulBrush(KColorScheme::Button, KColorScheme::FocusColor);
    _buttonHoverBrush = KStatefulBrush(KColorScheme::Button, KColorScheme::HoverColor);
    _viewNegativeTextBrush = KStatefulBrush(KColorScheme::View, KColorScheme::NegativeText);
    _viewNeutralTextBrush = KStatefulBrush(KColorScheme::View, KColorScheme::NeutralText);

    const QPalette palette(QApplication::palette());
    _config->reparseConfiguration();
    _kwinConfig->reparseConfiguration();
    _cachedAutoValid = false;
    _decorationConfig->load();

    KConfigGroup globalGroup(_config->group(QStringLiteral("WM")));
    _activeTitleBarColor = globalGroup.readEntry("activeBackground", palette.color(QPalette::Active, QPalette::Highlight));
    _activeTitleBarTextColor = globalGroup.readEntry("activeForeground", palette.color(QPalette::Active, QPalette::HighlightedText));
    _inactiveTitleBarColor = globalGroup.readEntry("inactiveBackground", palette.color(QPalette::Disabled, QPalette::Highlight));
    _inactiveTitleBarTextColor = globalGroup.readEntry("inactiveForeground", palette.color(QPalette::Disabled, QPalette::HighlightedText));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    _contrastBias = KColorScheme::frameContrast();
#else
    _contrastBias = 0.2;
#endif

    if (const QString colorSchemePath = qApp->property("KDE_COLOR_SCHEME_PATH").toString(); !colorSchemePath.isEmpty()) {
        KConfig config(colorSchemePath, KConfig::SimpleConfig);
        KConfigGroup appGroup(config.group(QStringLiteral("WM")));
        _activeTitleBarColor = appGroup.readEntry("activeBackground", _activeTitleBarColor);
        _activeTitleBarTextColor = appGroup.readEntry("activeForeground", _activeTitleBarTextColor);
        _inactiveTitleBarColor = appGroup.readEntry("inactiveBackground", _inactiveTitleBarColor);
        _inactiveTitleBarTextColor = appGroup.readEntry("inactiveForeground", _inactiveTitleBarTextColor);
    }
}

void Helper::installEventFilter(QApplication *app) const
{
    if (app) {
        app->installEventFilter(_eventFilter);
    }
}

void Helper::removeEventFilter(QApplication *app) const
{
    if (app) {
        app->removeEventFilter(_eventFilter);
    }
}

//* a rect of the given size centred in rect, on whole pixels
static QRectF centerRect(const QRectF &rect, qreal width, qreal height)
{
    return QRectF(qRound(rect.center().x() - width / 2), qRound(rect.center().y() - height / 2), width, height);
}

QColor transparentize(const QColor &color, qreal amount)
{
    auto clone = color;
    clone.setAlphaF(amount);
    return clone;
}

//____________________________________________________________________
QColor Helper::frameOutlineColor(const QPalette &palette, bool mouseOver, bool hasFocus, qreal opacity, AnimationMode mode) const
{
    QColor outline(KColorUtils::mix(palette.color(QPalette::Window), palette.color(QPalette::WindowText), frameIntensityBias()));

    // focus takes precedence over hover
    if (mode == AnimationFocus) {
        const QColor focus(focusColor(palette));
        const QColor hover(hoverColor(palette));

        if (mouseOver) {
            outline = KColorUtils::mix(hover, focus, opacity);
        } else {
            outline = KColorUtils::mix(outline, focus, opacity);
        }

    } else if (hasFocus) {
        outline = focusColor(palette);

    } else if (mode == AnimationHover) {
        const QColor hover(hoverColor(palette));
        outline = KColorUtils::mix(outline, hover, opacity);

    } else if (mouseOver) {
        outline = hoverColor(palette);
    }

    return outline;
}

//____________________________________________________________________
QColor Helper::focusOutlineColor(const QPalette &palette) const
{
    return KColorUtils::mix(focusColor(palette), palette.color(QPalette::WindowText), 0.15);
}

//____________________________________________________________________
QColor Helper::hoverOutlineColor(const QPalette &palette) const
{
    return KColorUtils::mix(hoverColor(palette), palette.color(QPalette::WindowText), 0.15);
}

//____________________________________________________________________
QColor Helper::buttonFocusOutlineColor(const QPalette &palette) const
{
    return KColorUtils::mix(buttonFocusColor(palette), palette.color(QPalette::ButtonText), 0.15);
}

//____________________________________________________________________
QColor Helper::buttonHoverOutlineColor(const QPalette &palette) const
{
    return KColorUtils::mix(buttonHoverColor(palette), palette.color(QPalette::ButtonText), 0.15);
}

//____________________________________________________________________
QColor Helper::sidePanelOutlineColor(const QPalette &palette, bool hasFocus, qreal opacity, AnimationMode mode) const
{
    QColor outline(palette.color(QPalette::Inactive, QPalette::Highlight));
    const QColor &focus = palette.color(QPalette::Active, QPalette::Highlight);

    if (mode == AnimationFocus) {
        outline = KColorUtils::mix(outline, focus, opacity);

    } else if (hasFocus) {
        outline = focus;
    }

    return outline;
}

//____________________________________________________________________
QColor Helper::frameBackgroundColor(const QPalette &palette, QPalette::ColorGroup group) const
{
    return KColorUtils::mix(palette.color(group, QPalette::Window), palette.color(group, QPalette::Base), 0.3);
}

//____________________________________________________________________
QColor Helper::arrowColor(const QPalette &palette, QPalette::ColorGroup group, QPalette::ColorRole role) const
{
    switch (role) {
    case QPalette::Text:
        return KColorUtils::mix(palette.color(group, QPalette::Text), palette.color(group, QPalette::Base), arrowShade);
    case QPalette::WindowText:
        return KColorUtils::mix(palette.color(group, QPalette::WindowText), palette.color(group, QPalette::Window), arrowShade);
    case QPalette::ButtonText:
        return KColorUtils::mix(palette.color(group, QPalette::ButtonText), palette.color(group, QPalette::Button), arrowShade);
    default:
        return palette.color(group, role);
    }
}

//____________________________________________________________________
QColor Helper::arrowColor(const QPalette &palette, bool mouseOver, bool hasFocus, qreal opacity, AnimationMode mode) const
{
    QColor outline(arrowColor(palette, QPalette::WindowText));
    if (mode == AnimationHover) {
        const QColor focus(focusColor(palette));
        const QColor hover(hoverColor(palette));
        if (hasFocus) {
            outline = KColorUtils::mix(focus, hover, opacity);
        } else {
            outline = KColorUtils::mix(outline, hover, opacity);
        }

    } else if (mouseOver) {
        outline = hoverColor(palette);

    } else if (mode == AnimationFocus) {
        const QColor focus(focusColor(palette));
        outline = KColorUtils::mix(outline, focus, opacity);

    } else if (hasFocus) {
        outline = focusColor(palette);
    }

    return outline;
}

//____________________________________________________________________
QColor Helper::sliderOutlineColor(const QPalette &palette, bool mouseOver, bool hasFocus, qreal opacity, AnimationMode mode) const
{
    QColor outline(KColorUtils::mix(palette.color(QPalette::Button), palette.color(QPalette::ButtonText), frameIntensityBias()));

    // hover takes precedence over focus
    if (mode == AnimationHover) {
        const QColor hover(hoverColor(palette));
        const QColor focus(focusColor(palette));
        if (hasFocus) {
            outline = KColorUtils::mix(focus, hover, opacity);
        } else {
            outline = KColorUtils::mix(outline, hover, opacity);
        }

    } else if (mouseOver) {
        outline = hoverColor(palette);

    } else if (mode == AnimationFocus) {
        const QColor focus(focusColor(palette));
        outline = KColorUtils::mix(outline, focus, opacity);

    } else if (hasFocus) {
        outline = focusColor(palette);
    }

    return outline;
}

//____________________________________________________________________
QColor Helper::scrollBarHandleColor(const QPalette &palette, bool mouseOver, bool hasFocus, qreal opacity, AnimationMode mode) const
{
    QColor color(alphaColor(palette.color(QPalette::WindowText), 0.5));

    // hover takes precedence over focus
    if (mode == AnimationHover) {
        const QColor hover(hoverColor(palette));
        const QColor focus(focusColor(palette));
        if (hasFocus) {
            color = KColorUtils::mix(focus, hover, opacity);
        } else {
            color = KColorUtils::mix(color, hover, opacity);
        }

    } else if (mouseOver) {
        color = hoverColor(palette);

    } else if (mode == AnimationFocus) {
        const QColor focus(focusColor(palette));
        color = KColorUtils::mix(color, focus, opacity);

    } else if (hasFocus) {
        color = focusColor(palette);
    }

    return color;
}

//______________________________________________________________________________
QColor Helper::checkBoxIndicatorColor(const QPalette &palette, bool mouseOver, bool active, qreal opacity, AnimationMode mode) const
{
    QColor color(KColorUtils::mix(palette.color(QPalette::Window), palette.color(QPalette::WindowText), 0.6));
    if (mode == AnimationHover) {
        const QColor focus(focusColor(palette));
        const QColor hover(hoverColor(palette));
        if (active) {
            color = KColorUtils::mix(focus, hover, opacity);
        } else {
            color = KColorUtils::mix(color, hover, opacity);
        }

    } else if (mouseOver) {
        color = hoverColor(palette);

    } else if (active) {
        color = focusColor(palette);
    }

    return color;
}

//______________________________________________________________________________
QColor Helper::separatorColor(const QPalette &palette) const
{
    return KColorUtils::mix(palette.color(QPalette::Window), palette.color(QPalette::WindowText), frameIntensityBias());
}

//______________________________________________________________________________
QPalette Helper::disabledPalette(const QPalette &source, qreal ratio) const
{
    QPalette copy(source);

    const QList<QPalette::ColorRole> roles =
        {QPalette::Window, QPalette::Highlight, QPalette::WindowText, QPalette::ButtonText, QPalette::Text, QPalette::Button};
    for (const QPalette::ColorRole &role : roles) {
        copy.setColor(role, KColorUtils::mix(source.color(QPalette::Active, role), source.color(QPalette::Disabled, role), 1.0 - ratio));
    }

    return copy;
}

//____________________________________________________________________
QColor Helper::alphaColor(QColor color, qreal alpha) const
{
    if (alpha >= 0 && alpha < 1.0) {
        color.setAlphaF(alpha * color.alphaF());
    }
    return color;
}

//______________________________________________________________________________
void Helper::renderDebugFrame(QPainter *painter, const QRectF &rect) const
{
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(Qt::red);
    painter->drawRect(strokedRect(rect));
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderFocusRect(QPainter *painter, const QRectF &rect, const QColor &color, const QColor &outline, Sides sides) const
{
    if (!color.isValid()) {
        return;
    }

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing);
    painter->setBrush(color);

    if (!(outline.isValid() && sides)) {
        painter->setPen(Qt::NoPen);
        painter->drawRect(rect);

    } else {
        painter->setClipRect(rect);

        QRectF copy(strokedRect(rect));

        const qreal radius(frameRadius(PenWidth::Frame));
        if (!(sides & SideTop)) {
            copy.adjust(0, -radius, 0, 0);
        }
        if (!(sides & SideBottom)) {
            copy.adjust(0, 0, 0, radius);
        }
        if (!(sides & SideLeft)) {
            copy.adjust(-radius, 0, 0, 0);
        }
        if (!(sides & SideRight)) {
            copy.adjust(0, 0, radius, 0);
        }

        painter->setPen(outline);
        // painter->setBrush( Qt::NoBrush );
        painter->drawRoundedRect(copy, radius, radius);
    }

    painter->restore();
}

//______________________________________________________________________________
void Helper::renderFocusLine(QPainter *painter, const QRectF &rect, const QColor &color) const
{
    if (!color.isValid()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(color);

    painter->translate(0, 2);
    painter->drawLine(rect.bottomLeft(), rect.bottomRight());
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderFocusRing(QPainter *painter, const QRectF &rect, const QPalette &palette, qreal radius) const
{
    // Windows draws the ring 3 px outside the control; a widget cannot paint
    // outside itself, so the ring follows the edge of the widget instead,
    // with the same 2 px + 1 px structure and the same radius rule
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);

    const QRectF outer(rect.adjusted(1, 1, -1, -1));
    painter->setPen(QPen(Tokens::focusOuter(palette), 2));
    painter->drawRoundedRect(outer, radius, radius);

    const QRectF inner(rect.adjusted(2.5, 2.5, -2.5, -2.5));
    const qreal innerRadius(qMax<qreal>(radius - 2, 0));
    painter->setPen(QPen(Tokens::focusInner(palette), 1));
    painter->drawRoundedRect(inner, innerRadius, innerRadius);
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderLineEditFrame(QPainter *painter, const QRectF &rect, const QPalette &palette, bool enabled, bool mouseOver, bool hasFocus, bool neutral)
    const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal radius(Tokens::ControlRadius);
    const QRectF frameRect(strokedRect(rect));

    QColor fill;
    if (!enabled) {
        fill = Tokens::controlDisabled(palette);
    } else if (hasFocus) {
        fill = Tokens::inputActive(palette);
    } else if (mouseOver) {
        fill = Tokens::controlHover(palette);
    } else {
        fill = Tokens::controlFill(palette);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(fill);
    painter->drawRoundedRect(frameRect, radius, radius);

    // 1 px stroke, a little stronger on the bottom row ("elevation border")
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(neutral ? neutralText(palette) : Tokens::strokeControl(palette), PenWidth::Frame));
    painter->drawRoundedRect(frameRect, radius, radius);

    QPainterPath clip;
    clip.addRoundedRect(rect, radius, radius);
    painter->setClipPath(clip);
    if (!neutral) {
        painter->setPen(QPen(Tokens::strokeBottom(palette), PenWidth::Frame));
        painter->drawLine(QPointF(rect.left(), rect.bottom() - 0.5), QPointF(rect.right(), rect.bottom() - 0.5));
    }

    // focused: 2 px accent line inside the bottom edge
    if (hasFocus && enabled) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(neutral ? neutralText(palette) : Tokens::accent(palette));
        painter->drawRect(QRectF(rect.left(), rect.bottom() - 2, rect.width(), 2));
    }
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderSelectionPill(QPainter *painter, const QRectF &itemRect, const QPalette &palette, bool reverseLayout) const
{
    const qreal height(qMin<qreal>(Metrics::ItemView_PillHeight, itemRect.height() - 8));
    if (height <= 0) {
        return;
    }
    const qreal width(Metrics::ItemView_PillWidth);
    const qreal x(reverseLayout ? itemRect.right() - width : itemRect.left());
    const QRectF pill(x, itemRect.center().y() - height / 2, width, height);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(Tokens::accent(palette));
    painter->drawRoundedRect(pill, width / 2, width / 2);
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderMenuCheckGlyph(QPainter *painter, const QRectF &rect, const QColor &color, bool exclusive) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QPointF c(rect.center());
    if (exclusive) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawEllipse(c, 3, 3);
    } else {
        QPainterPath path;
        path.moveTo(c + QPointF(-5, 0.5));
        path.lineTo(c + QPointF(-1.5, 4));
        path.lineTo(c + QPointF(5.5, -3.5));
        QPen pen(color, 1.5);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    }
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderFrameWithSides(QPainter *painter, const QRectF &rect, const QColor &color, Qt::Edges edges, const QColor &outline) const
{
    painter->save();

    painter->setRenderHint(QPainter::Antialiasing);

    QRectF frameRect(rect);

    // set brush
    painter->setBrush(color);
    painter->setPen(Qt::NoPen);

    // render
    painter->drawRect(frameRect);

    // set brush again
    painter->setBrush(Qt::NoBrush);
    painter->setPen(outline);

    // manually apply the effects of StrokedRect here but only to the edges with a frame
    if (edges & Qt::LeftEdge) {
        frameRect.adjust(0.5, 0.0, 0.0, 0.0);
    }
    if (edges & Qt::RightEdge) {
        frameRect.adjust(0.0, 0, -0.5, 0.0);
    }
    if (edges & Qt::TopEdge) {
        frameRect.adjust(0.0, 0.5, 0.0, 0.0);
    }
    if (edges & Qt::BottomEdge) {
        frameRect.adjust(0.0, 0.0, 0.0, -0.5);
    }

    // draw lines
    if (edges & Qt::LeftEdge) {
        painter->drawLine(QLineF(frameRect.topLeft(), frameRect.bottomLeft()));
    }
    if (edges & Qt::RightEdge) {
        painter->drawLine(QLineF(frameRect.topRight(), frameRect.bottomRight()));
    }
    if (edges & Qt::TopEdge) {
        painter->drawLine(QLineF(frameRect.topLeft(), frameRect.topRight()));
    }
    if (edges & Qt::BottomEdge) {
        painter->drawLine(QLineF(frameRect.bottomLeft(), frameRect.bottomRight()));
    }

    painter->restore();
}

//______________________________________________________________________________
void Helper::renderFrame(QPainter *painter, const QRectF &rect, const QColor &color, const QColor &outline) const
{
    painter->setRenderHint(QPainter::Antialiasing);

    QRectF frameRect(rect);
    qreal radius(frameRadius(PenWidth::NoPen));

    // set pen
    if (outline.isValid()) {
        painter->setPen(outline);
        frameRect = strokedRect(frameRect);
        radius = frameRadiusForNewPenWidth(radius, PenWidth::Frame);

    } else {
        painter->setPen(Qt::NoPen);
    }

    // set brush
    if (color.isValid()) {
        painter->setBrush(color);
    } else {
        painter->setBrush(Qt::NoBrush);
    }

    // render
    painter->drawRoundedRect(frameRect, radius, radius);
}

//______________________________________________________________________________
void Helper::renderSidePanelFrame(QPainter *painter, const QRectF &rect, const QColor &outline, Side side) const
{
    // check color
    if (!outline.isValid()) {
        return;
    }

    // adjust rect
    QRectF frameRect(strokedRect(rect));

    // setup painter
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(outline);

    // render
    switch (side) {
    default:
    case SideLeft:
        painter->drawLine(frameRect.topRight(), frameRect.bottomRight());
        break;

    case SideTop:
        painter->drawLine(frameRect.topLeft(), frameRect.topRight());
        break;

    case SideRight:
        painter->drawLine(frameRect.topLeft(), frameRect.bottomLeft());
        break;

    case SideBottom:
        painter->drawLine(frameRect.bottomLeft(), frameRect.bottomRight());
        break;

    case AllSides: {
        const qreal radius(frameRadius(PenWidth::Frame));
        painter->drawRoundedRect(frameRect, radius, radius);
        break;
    }
    }
}

//______________________________________________________________________________
void Helper::renderMenuFrame(QPainter *painter, const QRectF &rect, const QColor &color, const QColor &outline, bool roundCorners, Qt::Edges seamlessEdges)
    const
{
    painter->save();

    // set brush
    if (color.isValid()) {
        painter->setBrush(color);
    } else {
        painter->setBrush(Qt::NoBrush);
    }

    // We simulate being able to independently adjust corner radii by
    // setting a clip region and then extending the rectangle beyond it.
    if (seamlessEdges != Qt::Edges()) {
        painter->setClipRect(rect);
    }

    if (roundCorners) {
        painter->setRenderHint(QPainter::Antialiasing);
        QRectF frameRect(rect);

        // menus and popups have Windows' 8 px corners
        qreal radius(Metrics::Menu_Radius);

        frameRect.adjust( //
            seamlessEdges.testFlag(Qt::LeftEdge) ? -radius : 0,
            seamlessEdges.testFlag(Qt::TopEdge) ? -radius : 0,
            seamlessEdges.testFlag(Qt::RightEdge) ? radius : 0,
            seamlessEdges.testFlag(Qt::BottomEdge) ? radius : 0);

        // set pen
        if (outline.isValid()) {
            painter->setPen(outline);
            frameRect = strokedRect(frameRect);
            radius = frameRadiusForNewPenWidth(radius, PenWidth::Frame);

        } else {
            painter->setPen(Qt::NoPen);
        }

        // render
        painter->drawRoundedRect(frameRect, radius, radius);

    } else {
        painter->setRenderHint(QPainter::Antialiasing, false);
        QRectF frameRect(rect);

        frameRect.adjust( //
            seamlessEdges.testFlag(Qt::LeftEdge) ? 1 : 0,
            seamlessEdges.testFlag(Qt::TopEdge) ? 1 : 0,
            seamlessEdges.testFlag(Qt::RightEdge) ? -1 : 0,
            seamlessEdges.testFlag(Qt::BottomEdge) ? -1 : 0);

        if (outline.isValid()) {
            painter->setPen(outline);
            frameRect.adjust(0, 0, -1, -1);

        } else {
            painter->setPen(Qt::NoPen);
        }

        painter->drawRect(frameRect);
    }

    painter->restore();
}

QRegion Helper::menuFrameRegion(const QMenu *widget)
{
    if (!widget) {
        return {};
    }

    const bool hasAlpha(hasAlphaChannel(widget));
    const auto seamlessEdges = menuSeamlessEdges(widget);
    const auto roundCorners = hasAlpha;

    if (roundCorners) {
        QRectF frameRect(widget->rect());

        qreal radius(Metrics::Menu_Radius);

        frameRect.adjust( //
            seamlessEdges.testFlag(Qt::LeftEdge) ? -radius : 0,
            seamlessEdges.testFlag(Qt::TopEdge) ? -radius : 0,
            seamlessEdges.testFlag(Qt::RightEdge) ? radius : 0,
            seamlessEdges.testFlag(Qt::BottomEdge) ? radius : 0);

        // outline is always valid/drawn
        frameRect = strokedRect(frameRect);
        radius = frameRadiusForNewPenWidth(radius, PenWidth::Frame);

        QPainterPath path;
        path.addRoundedRect(frameRect, radius, radius);
        return QRegion(path.toFillPolygon().toPolygon()).intersected(widget->rect());
    }

    return QRegion(widget->rect());
}

//______________________________________________________________________________
void Helper::renderAcrylicFrame(QPainter *painter, const QRectF &rect, const QPalette &palette, qreal opacity, bool roundCorners, Qt::Edges seamlessEdges) const
{
    painter->save();
    // the tint replaces whatever is in the window buffer, so the blur
    // behind shows through it; the outline is then drawn over it
    if (opacity < 1.0 && roundCorners) {
        painter->setCompositionMode(QPainter::CompositionMode_Source);
    }
    renderMenuFrame(painter, rect, Tokens::withAlpha(Tokens::bgTertiary(palette), roundCorners ? opacity : 1.0), QColor(), roundCorners, seamlessEdges);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    renderMenuFrame(painter, rect, QColor(), Tokens::strokeSurface(palette), roundCorners, seamlessEdges);
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderButtonFrame(QPainter *painter,
                               const QRectF &rect,
                               const QPalette &palette,
                               const QHash<QByteArray, bool> &stateProperties,
                               qreal bgAnimation,
                               qreal penAnimation) const
{
    Q_UNUSED(penAnimation)

    const bool enabled = stateProperties.value("enabled", true);
    const bool visualFocus = stateProperties.value("visualFocus");
    const bool hovered = stateProperties.value("hovered");
    const bool down = stateProperties.value("down");
    const bool checked = stateProperties.value("checked");
    const bool flat = stateProperties.value("flat");
    const bool defaultButton = stateProperties.value("defaultButton");
    const bool hasNeutralHighlight = stateProperties.value("hasNeutralHighlight");
    const bool roundButton = stateProperties.value("roundButton");

    // flat buttons draw nothing at rest
    if (flat && !(hovered || down || checked || visualFocus) && bgAnimation == AnimationData::OpacityInvalid) {
        return;
    }

    // the button fills its whole rect: 32 px tall on screen, like Windows'
    const QRectF frameRect(strokedRect(rect));
    const qreal roundRadius = std::max(rect.width(), rect.height()) / 2;
    const qreal radius = roundButton ? roundRadius : qreal(Tokens::ControlRadius);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Windows' primary button, and a toggled button, are filled with the accent
    const bool accentFill(!flat && (defaultButton || checked));

    QColor fill;
    QColor stroke;
    QColor bottom;
    if (flat) {
        // tool-button look: translucent fills over whatever is behind
        if (!enabled) {
            fill = checked ? Tokens::subtlePressed(palette) : QColor(Qt::transparent);
        } else if (down) {
            fill = Tokens::subtlePressed(palette);
        } else if (checked) {
            fill = hovered ? Tokens::mix(Tokens::subtleSelected(palette), Tokens::subtleHover(palette), 0.5) : Tokens::subtleSelected(palette);
        } else if (hovered) {
            fill = Tokens::subtleHover(palette);
        } else {
            fill = Qt::transparent;
        }
        // a hover that is still fading in or out
        if (bgAnimation != AnimationData::OpacityInvalid && enabled && !down && !checked) {
            fill = Tokens::withAlpha(Tokens::subtleHover(palette), Tokens::subtleHover(palette).alphaF() * bgAnimation);
        }
    } else if (accentFill) {
        if (!enabled) {
            fill = Tokens::accent(palette, QPalette::Disabled);
        } else if (down) {
            fill = Tokens::accentPressed(palette);
        } else if (hovered) {
            fill = Tokens::accentHover(palette);
        } else {
            fill = Tokens::accent(palette);
        }
    } else {
        if (!enabled) {
            fill = Tokens::controlDisabled(palette);
        } else if (down) {
            fill = Tokens::controlPressed(palette);
        } else if (hovered) {
            fill = Tokens::controlHover(palette);
        } else {
            fill = Tokens::controlFill(palette);
        }
        stroke = hasNeutralHighlight ? neutralText(palette) : Tokens::strokeControl(palette);
        bottom = (hasNeutralHighlight || down) ? QColor() : Tokens::strokeBottom(palette);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(fill);
    painter->drawRoundedRect(frameRect, radius, radius);

    QPainterPath clip;
    clip.addRoundedRect(frameRect.adjusted(-0.5, -0.5, 0.5, 0.5), radius, radius);

    if (stroke.isValid()) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(stroke, PenWidth::Frame));
        painter->drawRoundedRect(frameRect, radius, radius);
    }

    if (bottom.isValid() && !roundButton) {
        // the bottom pixel row of the stroke is a little stronger
        painter->save();
        painter->setClipPath(clip);
        painter->setPen(QPen(bottom, PenWidth::Frame));
        painter->drawLine(QPointF(frameRect.left(), frameRect.bottom()), QPointF(frameRect.right(), frameRect.bottom()));
        painter->restore();
    }

    if (accentFill && enabled && !down) {
        // the Raye "lit edge": 1 px highlight inside the top of an accent button
        painter->save();
        painter->setClipPath(clip);
        painter->setPen(QPen(QColor(255, 255, 255, 31), PenWidth::Frame));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(frameRect.adjusted(0.5, 0.5, -0.5, 0.5), radius, radius);
        painter->restore();
    }

    painter->restore();

    if (visualFocus && enabled) {
        renderFocusRing(painter, rect, palette, radius);
    }
}

//______________________________________________________________________________
void Helper::renderToolBoxFrame(QPainter *painter, const QRectF &rect, int tabWidth, const QColor &outline) const
{
    if (!outline.isValid()) {
        return;
    }

    // round radius
    const qreal radius(frameRadius(PenWidth::Frame));
    const QSizeF cornerSize(2 * radius, 2 * radius);

    // if rect - tabwidth is even, need to increase tabWidth by 1 unit
    // for anti aliasing
    if (!((rect.toRect().width() - tabWidth) % 2)) {
        ++tabWidth;
    }

    // adjust rect for antialiasing
    QRectF baseRect(strokedRect(rect));

    // create path
    QPainterPath path;
    path.moveTo(0, baseRect.height() - 1);
    path.lineTo((baseRect.width() - tabWidth) / 2 - radius, baseRect.height() - 1);
    path.arcTo(QRectF(QPointF((baseRect.width() - tabWidth) / 2 - 2 * radius, baseRect.height() - 1 - 2 * radius), cornerSize), 270, 90);
    path.lineTo((baseRect.width() - tabWidth) / 2, radius);
    path.arcTo(QRectF(QPointF((baseRect.width() - tabWidth) / 2, 0), cornerSize), 180, -90);
    path.lineTo((baseRect.width() + tabWidth) / 2 - 1 - radius, 0);
    path.arcTo(QRectF(QPointF((baseRect.width() + tabWidth) / 2 - 1 - 2 * radius, 0), cornerSize), 90, -90);
    path.lineTo((baseRect.width() + tabWidth) / 2 - 1, baseRect.height() - 1 - radius);
    path.arcTo(QRectF(QPointF((baseRect.width() + tabWidth) / 2 - 1, baseRect.height() - 1 - 2 * radius), cornerSize), 180, 90);
    path.lineTo(baseRect.width() - 1, baseRect.height() - 1);

    // render
    painter->setRenderHints(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(outline);
    painter->translate(baseRect.topLeft());
    painter->drawPath(path);
}

//______________________________________________________________________________
void Helper::renderTabWidgetFrame(QPainter *painter, const QRectF &rect, const QColor &color, const QColor &outline, Corners corners) const
{
    painter->setRenderHint(QPainter::Antialiasing);

    QRectF frameRect(rect.adjusted(1, 1, -1, -1));
    qreal radius(frameRadius(PenWidth::NoPen));

    // set pen
    if (outline.isValid()) {
        painter->setPen(outline);
        frameRect = strokedRect(frameRect);
        radius = frameRadiusForNewPenWidth(radius, PenWidth::Frame);

    } else {
        painter->setPen(Qt::NoPen);
    }

    // set brush
    if (color.isValid()) {
        painter->setBrush(color);
    } else {
        painter->setBrush(Qt::NoBrush);
    }

    // render
    QPainterPath path(roundedPath(frameRect, corners, radius));
    painter->drawPath(path);
}

//______________________________________________________________________________
void Helper::renderSelection(QPainter *painter, const QRectF &rect, const QColor &color) const
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawRect(rect);
}

//______________________________________________________________________________
void Helper::renderSeparator(QPainter *painter, const QRectF &rect, const QColor &color, bool vertical) const
{
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(color);

    if (vertical) {
        painter->translate(rect.width() / 2, 0);
        painter->drawLine(rect.topLeft(), rect.bottomLeft());

    } else {
        painter->translate(0, rect.height() / 2);
        painter->drawLine(rect.topLeft(), rect.topRight());
    }
}

//______________________________________________________________________________
void Helper::renderCheckBoxBackground(QPainter *painter,
                                      const QRectF &rect,
                                      const QPalette &palette,
                                      CheckBoxState state,
                                      bool neutalHighlight,
                                      bool sunken,
                                      qreal animation,
                                      bool mouseOver,
                                      bool enabled) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // a 20 px box with 4 px corners
    const QRectF boxRect(centerRect(rect, Metrics::CheckBox_Size, Metrics::CheckBox_Size));
    const QRectF frameRect(strokedRect(boxRect));
    const qreal radius(Metrics::CheckBox_Radius - 0.5);

    // unchecked: a faint fill with a strong outline
    QColor fill;
    if (!enabled) {
        fill = Tokens::controlDisabled(palette);
    } else if (sunken) {
        fill = Tokens::controlPressed(palette);
    } else if (mouseOver) {
        fill = Tokens::controlHover(palette);
    } else {
        fill = Tokens::controlPressed(palette);
    }
    const QColor outline(neutalHighlight ? neutralText(palette) : Tokens::strokeStrong(palette, enabled ? QPalette::Active : QPalette::Disabled));

    painter->setPen(QPen(outline, PenWidth::Frame));
    painter->setBrush(fill);
    painter->drawRoundedRect(frameRect, radius, radius);

    if (state != CheckOff) {
        // checked or partly checked: filled with the accent
        QColor accentColor;
        if (neutalHighlight) {
            accentColor = neutralText(palette);
        } else if (!enabled) {
            accentColor = Tokens::accent(palette, QPalette::Disabled);
        } else if (sunken) {
            accentColor = Tokens::accentPressed(palette);
        } else if (mouseOver) {
            accentColor = Tokens::accentHover(palette);
        } else {
            accentColor = Tokens::accent(palette);
        }
        if (state == CheckAnimated && animation != AnimationData::OpacityInvalid) {
            painter->setOpacity(animation);
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(accentColor);
        painter->drawRoundedRect(boxRect, Metrics::CheckBox_Radius, Metrics::CheckBox_Radius);
    }
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderCheckBox(QPainter *painter,
                            const QRectF &rect,
                            const QPalette &palette,
                            bool mouseOver,
                            CheckBoxState state,
                            CheckBoxState target,
                            bool neutalHighlight,
                            bool sunken,
                            qreal animation,
                            qreal hoverAnimation) const
{
    Q_UNUSED(mouseOver)
    Q_UNUSED(neutalHighlight)
    Q_UNUSED(sunken)
    Q_UNUSED(hoverAnimation)

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // the 12 px glyph, in the text colour on the accent
    const QRectF boxRect(centerRect(rect, Metrics::CheckBox_Size, Metrics::CheckBox_Size));
    const QPointF c(boxRect.center());

    QPainterPath check;
    check.moveTo(c + QPointF(-4.5, 0.25));
    check.lineTo(c + QPointF(-1.5, 3.25));
    check.lineTo(c + QPointF(4.5, -3.25));

    QPainterPath dash;
    dash.moveTo(c + QPointF(-4, 0));
    dash.lineTo(c + QPointF(4, 0));

    QPen pen(Tokens::onAccent(palette), 1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setBrush(Qt::NoBrush);

    const CheckBoxState shown(state == CheckAnimated ? target : state);
    const QPainterPath &path(shown == CheckPartial ? dash : check);
    if (shown == CheckOn || shown == CheckPartial) {
        if (state == CheckAnimated && animation != AnimationData::OpacityInvalid) {
            // the check draws its stroke, like Windows'
            pen.setDashPattern({path.length() * animation / pen.widthF(), path.length() / pen.widthF()});
        }
        painter->setPen(pen);
        painter->drawPath(path);
    }
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderRadioButtonBackground(QPainter *painter,
                                         const QRectF &rect,
                                         const QPalette &palette,
                                         RadioButtonState state,
                                         bool neutalHighlight,
                                         bool sunken,
                                         qreal animation,
                                         bool mouseOver,
                                         bool enabled) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF circleRect(centerRect(rect, Metrics::CheckBox_Size, Metrics::CheckBox_Size));
    const QRectF frameRect(strokedRect(circleRect));

    QColor fill;
    if (!enabled) {
        fill = Tokens::controlDisabled(palette);
    } else if (sunken) {
        fill = Tokens::controlPressed(palette);
    } else if (mouseOver) {
        fill = Tokens::controlHover(palette);
    } else {
        fill = Tokens::controlPressed(palette);
    }
    const QColor outline(neutalHighlight ? neutralText(palette) : Tokens::strokeStrong(palette, enabled ? QPalette::Active : QPalette::Disabled));
    painter->setPen(QPen(outline, PenWidth::Frame));
    painter->setBrush(fill);
    painter->drawEllipse(frameRect);

    if (state != RadioOff) {
        QColor accentColor;
        if (neutalHighlight) {
            accentColor = neutralText(palette);
        } else if (!enabled) {
            accentColor = Tokens::accent(palette, QPalette::Disabled);
        } else if (mouseOver || sunken) {
            accentColor = Tokens::accentHover(palette);
        } else {
            accentColor = Tokens::accent(palette);
        }
        if (state == RadioAnimated && animation != AnimationData::OpacityInvalid) {
            painter->setOpacity(animation);
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(accentColor);
        painter->drawEllipse(circleRect);
    }
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderRadioButton(QPainter *painter,
                               const QRectF &rect,
                               const QPalette &palette,
                               bool mouseOver,
                               RadioButtonState state,
                               bool neutralHighlight,
                               bool sunken,
                               qreal animation,
                               qreal animationHover) const
{
    Q_UNUSED(neutralHighlight)
    Q_UNUSED(animationHover)

    if (state == RadioOff) {
        return;
    }

    // Windows' inverted dot: 12 px in the control colour on the accent,
    // 14 under the pointer and 10 while pressed
    qreal diameter(sunken ? 10 : (mouseOver ? 14 : 12));
    if (state == RadioAnimated && animation != AnimationData::OpacityInvalid) {
        diameter = diameter * animation;
    }

    const QRectF circleRect(centerRect(rect, Metrics::CheckBox_Size, Metrics::CheckBox_Size));
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(Tokens::isDark(palette) ? Tokens::controlPressed(palette) : QColor(Qt::white));
    painter->drawEllipse(circleRect.center(), diameter / 2, diameter / 2);
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderSliderGroove(QPainter *painter, const QRectF &rect, const QColor &fg, const QColor &bg) const
{
    Q_UNUSED(bg)

    // a 4 px track with round ends; the caller passes the rail colour for
    // the whole track, then the accent for the part up to the thumb
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const qreal radius(0.5 * qMin(rect.width(), rect.height()));
    if (fg.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(fg);
        painter->drawRoundedRect(rect, radius, radius);
    }
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderSliderThumb(QPainter *painter, const QRectF &rect, const QPalette &palette, bool enabled, bool mouseOver, bool sunken) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF outer(centerRect(rect, Metrics::Slider_ControlThickness, Metrics::Slider_ControlThickness));
    painter->setPen(QPen(Tokens::strokeControl(palette), PenWidth::Frame));
    painter->setBrush(Tokens::controlFill(palette));
    painter->drawEllipse(strokedRect(outer));

    const qreal diameter(sunken ? 10 : (mouseOver ? 14 : 12));
    painter->setPen(Qt::NoPen);
    QColor inner;
    if (!enabled) {
        inner = Tokens::textDisabled(palette);
    } else if (sunken) {
        inner = Tokens::accentPressed(palette);
    } else if (mouseOver) {
        inner = Tokens::accentHover(palette);
    } else {
        inner = Tokens::accent(palette);
    }
    painter->setBrush(inner);
    painter->drawEllipse(outer.center(), diameter / 2, diameter / 2);
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderDialGroove(QPainter *painter, const QRectF &rect, const QColor &fg, const QColor &bg, qreal first, qreal last) const
{
    // setup painter
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF baseRect(rect);

    // content
    if (fg.isValid()) {
        const qreal penWidth(Metrics::Slider_GrooveThickness);
        const QRectF grooveRect(rect.adjusted(penWidth / 2, penWidth / 2, -penWidth / 2, -penWidth / 2));

        // setup angles
        const int angleStart(first * 180 * 16 / M_PI);
        const int angleSpan((last - first) * 180 * 16 / M_PI);
        const QPen bgPen(fg, penWidth, Qt::SolidLine, Qt::RoundCap);
        const QPen fgPen(transparentize(KColorUtils::overlayColors(bg, alphaColor(fg, 0.5)), frameIntensityBias()), penWidth - 2, Qt::SolidLine, Qt::RoundCap);

        // setup pen
        if (angleSpan != 0) {
            painter->setPen(bgPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawArc(grooveRect, angleStart, angleSpan);
            painter->setPen(fgPen);
            painter->drawArc(grooveRect, angleStart, angleSpan);
        }
    }
}

//______________________________________________________________________________
void Helper::initSliderStyleOption(const QSlider *slider, QStyleOptionSlider *option) const
{
    option->initFrom(slider);
    option->subControls = QStyle::SC_None;
    option->activeSubControls = QStyle::SC_None;
    option->orientation = slider->orientation();
    option->maximum = slider->maximum();
    option->minimum = slider->minimum();
    option->tickPosition = slider->tickPosition();
    option->tickInterval = slider->tickInterval();
    option->upsideDown = (slider->orientation() == Qt::Horizontal) //
        ? (slider->invertedAppearance() != (option->direction == Qt::RightToLeft))
        : (!slider->invertedAppearance());
    option->direction = Qt::LeftToRight; // we use the upsideDown option instead
    option->sliderPosition = slider->sliderPosition();
    option->sliderValue = slider->value();
    option->singleStep = slider->singleStep();
    option->pageStep = slider->pageStep();
    if (slider->orientation() == Qt::Horizontal) {
        option->state |= QStyle::State_Horizontal;
    }
    // Can't fetch activeSubControls, because it's private API
}

//______________________________________________________________________________
QRectF Helper::pathForSliderHandleFocusFrame(QPainterPath &focusFramePath, const QRectF &rect, int hmargin, int vmargin) const
{
    // Mimics path and adjustments of renderSliderThumb
    QRectF frameRect(rect);
    frameRect.translate(hmargin, vmargin);
    frameRect = strokedRect(frameRect);
    focusFramePath.addEllipse(frameRect);
    frameRect.adjust(-hmargin, -vmargin, hmargin, vmargin);
    focusFramePath.addEllipse(frameRect);
    return frameRect;
}

//______________________________________________________________________________
void Helper::renderSliderHandle(QPainter *painter, const QRectF &rect, const QColor &color, const QColor &outline, const QColor &shadow, bool sunken) const
{
    // setup painter
    painter->setRenderHint(QPainter::Antialiasing, true);

    // copy rect
    QRectF frameRect(rect);
    frameRect.adjust(1, 1, -1, -1);

    // shadow
    if (!sunken) {
        renderEllipseShadow(painter, frameRect, shadow);
    }

    // set pen
    if (outline.isValid()) {
        painter->setPen(QPen(outline, PenWidth::Frame));
        frameRect = strokedRect(frameRect);

    } else {
        painter->setPen(Qt::NoPen);
    }

    // set brush
    if (color.isValid()) {
        painter->setBrush(color);
    } else {
        painter->setBrush(Qt::NoBrush);
    }

    // render
    painter->drawEllipse(frameRect);
}

//______________________________________________________________________________
void Helper::renderProgressBarGroove(QPainter *painter, const QRectF &rect, const QColor &fg, const QColor &bg) const
{
    Q_UNUSED(bg)

    // Windows' progress bar: a 3 px accent bar, drawn here, on a 1 px rail
    // drawn by the caller with this same call and a thin rect
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const qreal radius(0.5 * qMin(rect.width(), rect.height()));
    if (fg.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(fg);
        painter->drawRoundedRect(rect, radius, radius);
    }
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderProgressBarBusyContents(QPainter *painter,
                                           const QRectF &rect,
                                           const QColor &first,
                                           const QColor &second,
                                           bool horizontal,
                                           bool reverse,
                                           int progress) const
{
    // an accent segment, fading into the Raye glow, slides along the bar
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setClipRect(rect);

    const qreal length(horizontal ? rect.width() : rect.height());
    const qreal segment(qMax<qreal>(length * 0.4, 8));
    // `progress` counts busy steps; one loop covers the bar and the segment
    const qreal travel(length + segment);
    const int steps(qMax(1, int(travel)));
    qreal offset(qreal(progress % steps) / steps * travel - segment);
    if (reverse) {
        offset = length - offset - segment;
    }

    QRectF segRect;
    QPointF start;
    QPointF stop;
    if (horizontal) {
        segRect = QRectF(rect.left() + offset, rect.top(), segment, rect.height());
        start = segRect.topLeft();
        stop = segRect.topRight();
    } else {
        // vertical bars fill from the bottom
        segRect = QRectF(rect.left(), rect.bottom() - offset - segment, rect.width(), segment);
        start = segRect.bottomLeft();
        stop = segRect.topLeft();
    }
    QLinearGradient gradient(start, stop);
    gradient.setColorAt(0, first);
    gradient.setColorAt(1, second.isValid() ? second : first);

    const qreal radius(0.5 * qMin(rect.width(), rect.height()));
    painter->setPen(Qt::NoPen);
    painter->setBrush(gradient);
    painter->drawRoundedRect(segRect, radius, radius);
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderScrollBarHandle(QPainter *painter, const QRectF &rect, const QColor &fg, const QColor &bg) const
{
    // setup painter
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF baseRect(rect);
    const qreal radius(0.5 * std::min({baseRect.width(), baseRect.height(), (qreal)Metrics::ScrollBar_SliderWidth}));

    painter->setPen(Qt::NoPen);
    painter->setPen(QPen(transparentize(fg, frameIntensityBias()), 1.001));
    painter->setBrush(KColorUtils::overlayColors(bg, alphaColor(fg, 0.5)));
    painter->drawRoundedRect(strokedRect(baseRect), radius, radius);
}

//______________________________________________________________________________
void Helper::renderScrollBarBorder(QPainter *painter, const QRectF &rect, const QColor &color) const
{
    // content
    if (color.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawRect(rect);
    }
}

void Helper::renderStaticTabBarTab(QPainter *painter,
                                   const QRectF &rect,
                                   const QPalette &palette,
                                   const QHash<QByteArray, bool> &stateProperties,
                                   Corners corners,
                                   qreal animation) const
{
    Q_UNUSED(corners)

    // navigation tabs that cannot move or close (pages of a settings
    // window): Windows' SelectorBar, a tint on hover and a 3x16 accent pill
    // under (or beside) the selected one
    const bool enabled = stateProperties.value("enabled", true);
    const bool hovered = stateProperties.value("hovered");
    const bool selected = stateProperties.value("selected");
    const bool south = stateProperties.value("south");
    const bool west = stateProperties.value("west");
    const bool east = stateProperties.value("east");
    const bool vertical(west || east);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);

    const QRectF tintRect(rect.adjusted(2, 2, -2, -2));
    QColor tint;
    if (enabled && hovered) {
        tint = Tokens::subtleHover(palette);
    }
    if (animation != AnimationData::OpacityInvalid && enabled) {
        tint = Tokens::withAlpha(Tokens::subtleHover(palette), Tokens::subtleHover(palette).alphaF() * animation);
    }
    if (tint.isValid()) {
        painter->setBrush(tint);
        painter->drawRoundedRect(tintRect, Tokens::ControlRadius, Tokens::ControlRadius);
    }

    if (selected) {
        const qreal length(Metrics::ItemView_PillHeight);
        const qreal thickness(Metrics::ItemView_PillWidth);
        QRectF pill;
        if (vertical) {
            pill = QRectF(east ? tintRect.right() - thickness : tintRect.left(), rect.center().y() - length / 2, thickness, length);
        } else {
            pill = QRectF(rect.center().x() - length / 2, south ? tintRect.top() : tintRect.bottom() - thickness, length, thickness);
        }
        painter->setBrush(enabled ? Tokens::accent(palette) : Tokens::accent(palette, QPalette::Disabled));
        painter->drawRoundedRect(pill, thickness / 2, thickness / 2);
    }
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderTabBarTab(QPainter *painter,
                             const QRectF &rect,
                             const QPalette &palette,
                             const QHash<QByteArray, bool> &stateProperties,
                             Corners corners,
                             qreal animation) const
{
    // WinUI TabView: the selected tab sits on the content colour with 8 px
    // corners on the bar side and merges into the page; the others are
    // flat, with a tint on hover
    const bool enabled = stateProperties.value("enabled", true);
    const bool hovered = stateProperties.value("hovered");
    const bool selected = stateProperties.value("selected");
    const bool documentMode = stateProperties.value("documentMode");
    const bool north = stateProperties.value("north");
    const bool south = stateProperties.value("south");
    const bool west = stateProperties.value("west");
    const bool east = stateProperties.value("east");
    const bool isQtQuickControl = stateProperties.value("isQtQuickControl");
    const bool hasAlteredBackground = stateProperties.value("hasAlteredBackground");

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QRectF frameRect = rect;

    if (selected) {
        // reach over the page's border so the two join
        const qreal overlap = PenWidth::Frame + 1;
        frameRect.adjust(east ? -overlap : 0, south ? -overlap : 0, west ? overlap : 0, north ? overlap : 0);

        const QColor fill((documentMode && !isQtQuickControl && !hasAlteredBackground) ? palette.color(QPalette::Window) : palette.color(QPalette::Base));
        const QPainterPath path(roundedPath(strokedRect(frameRect), corners, Metrics::Menu_Radius - 0.5));
        painter->setBrush(fill);
        painter->setPen(Qt::NoPen);
        painter->drawPath(path);

        // the stroke on the three outer sides only
        painter->save();
        QRectF clip(frameRect);
        clip.adjust(east ? 1 : 0, south ? 1 : 0, west ? -1 : 0, north ? -1 : 0);
        painter->setClipRect(clip);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Tokens::strokeControl(palette), PenWidth::Frame));
        painter->drawPath(path);
        painter->restore();
    } else {
        // a tint inside the tab, never touching the page
        QRectF tintRect(frameRect.adjusted(2, 3, -2, -3));
        QColor tint;
        if (enabled && hovered) {
            tint = Tokens::subtleHover(palette);
        }
        if (animation != AnimationData::OpacityInvalid && enabled) {
            tint = Tokens::withAlpha(Tokens::subtleHover(palette), Tokens::subtleHover(palette).alphaF() * animation);
        }
        if (tint.isValid()) {
            painter->setBrush(tint);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(tintRect, Tokens::ControlRadius, Tokens::ControlRadius);
        }
    }
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderArrow(QPainter *painter, const QRectF &rect, const QColor &color, ArrowOrientation orientation) const
{
    int size = std::min({rect.toRect().width(), rect.toRect().height(), Metrics::ArrowSize});
    // No point in trying to draw if it's too small
    if (size <= 0) {
        return;
    }

    qreal penOffset = PenWidth::Symbol / 2.0;
    qreal center = size / 2.0;
    qreal maxExtent = size * 0.75;
    qreal minExtent = size / 4.0;
    qreal sizeOffset = 0;
    int remainder = size % 4;
    if (remainder == 2) {
        sizeOffset = 0.5;
    } else if (remainder == 1) {
        sizeOffset = -0.25;
    } else if (remainder == 3) {
        sizeOffset = 0.25;
    }

    QPolygonF arrow;
    switch (orientation) {
    case ArrowUp:
        arrow = QVector<QPointF>{
            {penOffset, maxExtent - penOffset - sizeOffset}, // left
            {center, minExtent - sizeOffset}, // mid
            {size - penOffset, maxExtent - penOffset - sizeOffset} // right
        };
        break;
    case ArrowDown:
        arrow = QVector<QPointF>{
            {penOffset, minExtent + penOffset + sizeOffset}, // left
            {center, maxExtent + sizeOffset}, // mid
            {size - penOffset, minExtent + penOffset + sizeOffset} // right
        };
        break;
    case ArrowLeft:
        arrow = QVector<QPointF>{
            {maxExtent - penOffset - sizeOffset, penOffset}, // top
            {minExtent - sizeOffset, center}, // mid
            {maxExtent - penOffset - sizeOffset, size - penOffset}, // bottom
        };
        break;
    case ArrowRight:
        arrow = QVector<QPointF>{
            {minExtent + penOffset + sizeOffset, penOffset}, // top
            {maxExtent + sizeOffset, center}, // mid
            {minExtent + penOffset + sizeOffset, size - penOffset}, // bottom
        };
        break;
    default:
        break;
    }

    arrow.translate(rect.x() + (rect.width() - size) / 2.0, rect.y() + (rect.height() - size) / 2.0);

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    QPen pen(color, PenWidth::Symbol);
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    painter->setPen(pen);
    painter->drawPolyline(arrow);
    painter->restore();
}

//______________________________________________________________________________
void Helper::renderDecorationButton(QPainter *painter, const QRectF &rect, const QColor &color, ButtonType buttonType, bool inverted) const
{
    painter->save();
    painter->setViewport(rect.toRect());
    painter->setWindow(0, 0, 18, 18);
    painter->setRenderHints(QPainter::Antialiasing);

    // initialize pen
    QPen pen;
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);

    if (inverted) {
        // render circle
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawEllipse(QRectF(0, 0, 18, 18));

        // take out the inner part
        painter->setCompositionMode(QPainter::CompositionMode_DestinationOut);
        painter->setBrush(Qt::NoBrush);
        pen.setColor(Qt::black);

    } else {
        painter->setBrush(Qt::NoBrush);
        pen.setColor(color);
    }

    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);
    pen.setWidthF(PenWidth::Symbol * qMax(1.0, 18.0 / rect.width()));
    painter->setPen(pen);

    switch (buttonType) {
    case ButtonClose: {
        painter->drawLine(QPointF(5, 5), QPointF(13, 13));
        painter->drawLine(13, 5, 5, 13);
        break;
    }

    case ButtonMaximize: {
        painter->drawPolyline(QVector<QPointF>{QPointF(4, 11), QPointF(9, 6), QPointF(14, 11)});
        break;
    }

    case ButtonMinimize: {
        painter->drawPolyline(QVector<QPointF>{QPointF(4, 7), QPointF(9, 12), QPointF(14, 7)});
        break;
    }

    case ButtonRestore: {
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->drawPolygon(QVector<QPointF>{QPointF(4.5, 9), QPointF(9, 4.5), QPointF(13.5, 9), QPointF(9, 13.5)});
        break;
    }

    default:
        break;
    }

    painter->restore();
}

//______________________________________________________________________________
void Helper::renderRoundedRectShadow(QPainter *painter, const QRectF &rect, const QColor &color, qreal radius) const
{
    if (!color.isValid()) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);

    qreal adjustment = 0.5 * PenWidth::Shadow; // Translate for the pen
    QRectF shadowRect = rect.adjusted(adjustment, adjustment, -adjustment, adjustment);

    painter->setPen(QPen(color, PenWidth::Shadow));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(shadowRect, radius, radius);
}

//______________________________________________________________________________
void Helper::renderEllipseShadow(QPainter *painter, const QRectF &rect, const QColor &color) const
{
    if (!color.isValid()) {
        return;
    }

    painter->save();

    // Clipping does not improve performance here

    qreal adjustment = 0.5 * PenWidth::Shadow; // Adjust for the pen

    qreal radius = rect.width() / 2 - adjustment;

    /* The right side is offset by +0.5 for the visible part of the shadow.
     * The other sides are offset by +0.5 or -0.5 because of the pen.
     */
    QRectF shadowRect = rect.adjusted(adjustment, adjustment, adjustment, -adjustment);

    painter->translate(rect.center());
    painter->rotate(45);
    painter->translate(-rect.center());
    painter->setPen(color);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(shadowRect, radius, radius);

    painter->restore();
}

//______________________________________________________________________________
bool Helper::isX11()
{
    static const bool s_isX11 = KWindowSystem::isPlatformX11();
    return s_isX11;
}

//______________________________________________________________________________
bool Helper::isWayland()
{
    static const bool s_isWayland = KWindowSystem::isPlatformWayland();
    return s_isWayland;
}

//______________________________________________________________________________
QRectF Helper::strokedRect(const QRectF &rect, const qreal penWidth) const
{
    /* With a pen stroke width of 1, the rectangle should have each of its
     * sides moved inwards by half a pixel. This allows the stroke to be
     * pixel perfect instead of blurry from sitting between pixels and
     * prevents the rectangle with a stroke from becoming larger than the
     * original size of the rectangle.
     */
    qreal adjustment = 0.5 * penWidth;
    return rect.adjusted(adjustment, adjustment, -adjustment, -adjustment);
}

//______________________________________________________________________________
QPainterPath Helper::roundedPath(const QRectF &rect, Corners corners, qreal radius) const
{
    QPainterPath path;

    // simple cases
    if (corners == 0) {
        path.addRect(rect);
        return path;
    }

    if (corners == AllCorners) {
        path.addRoundedRect(rect, radius, radius);
        return path;
    }

    const QSizeF cornerSize(2 * radius, 2 * radius);

    // rotate counterclockwise
    // top left corner
    if (corners & CornerTopLeft) {
        path.moveTo(rect.topLeft() + QPointF(radius, 0));
        path.arcTo(QRectF(rect.topLeft(), cornerSize), 90, 90);

    } else {
        path.moveTo(rect.topLeft());
    }

    // bottom left corner
    if (corners & CornerBottomLeft) {
        path.lineTo(rect.bottomLeft() - QPointF(0, radius));
        path.arcTo(QRectF(rect.bottomLeft() - QPointF(0, 2 * radius), cornerSize), 180, 90);

    } else {
        path.lineTo(rect.bottomLeft());
    }

    // bottom right corner
    if (corners & CornerBottomRight) {
        path.lineTo(rect.bottomRight() - QPointF(radius, 0));
        path.arcTo(QRectF(rect.bottomRight() - QPointF(2 * radius, 2 * radius), cornerSize), 270, 90);

    } else {
        path.lineTo(rect.bottomRight());
    }

    // top right corner
    if (corners & CornerTopRight) {
        path.lineTo(rect.topRight() + QPointF(0, radius));
        path.arcTo(QRectF(rect.topRight() - QPointF(2 * radius, 0), cornerSize), 0, 90);

    } else {
        path.lineTo(rect.topRight());
    }

    path.closeSubpath();
    return path;
}

//________________________________________________________________________________________________________
bool Helper::compositingActive() const
{
    if (isX11()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        return KWindowSystem::compositingActive();
#elif __has_include(<KX11Extras>)
        return KX11Extras::compositingActive();
#endif
    }

    return true;
}

//____________________________________________________________________
bool Helper::hasAlphaChannel(const QWidget *widget) const
{
    return compositingActive() && widget && widget->testAttribute(Qt::WA_TranslucentBackground);
}

//______________________________________________________________________________________

QPixmap Helper::coloredIcon(const QIcon &icon, const QPalette &palette, const QSize &size, qreal devicePixelRatio, QIcon::Mode mode, QIcon::State state)
{
    const QPalette activePalette = KIconLoader::global()->customPalette();
    const bool changePalette = activePalette != palette;
    if (changePalette) {
        KIconLoader::global()->setCustomPalette(palette);
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPixmap pixmap = icon.pixmap(size, devicePixelRatio, mode, state);
#else
    Q_UNUSED(devicePixelRatio);
    const QPixmap pixmap = icon.pixmap(size, mode, state);
#endif
    if (changePalette) {
        if (activePalette == QPalette()) {
            KIconLoader::global()->resetPalette();
        } else {
            KIconLoader::global()->setCustomPalette(activePalette);
        }
    }
    return pixmap;
}

bool Helper::shouldDrawToolsArea(const QWidget *widget) const
{
    if (!widget) {
        return false;
    }
    static bool isAuto = false;
    static QString borderSize;
    if (!_cachedAutoValid) {
        KConfigGroup kdecorationGroup(_kwinConfig->group(QStringLiteral("org.kde.kdecoration2")));
        isAuto = kdecorationGroup.readEntry("BorderSizeAuto", true);
        borderSize = kdecorationGroup.readEntry("BorderSize", "Normal");
        _cachedAutoValid = true;
    }
    if (isAuto) {
        auto window = widget->window();
        if (qobject_cast<const QDialog *>(widget)) {
            return true;
        }
        if (window) {
            auto handle = window->windowHandle();
            if (handle) {
                auto toolbar = qobject_cast<const QToolBar *>(widget);
                if (toolbar) {
                    if (toolbar->isFloating()) {
                        return false;
                    }
                }
                return true;
            }
        } else {
            return false;
        }
    }
    if (borderSize != "None" && borderSize != "NoSides") {
        return false;
    }
    return true;
}

Qt::Edges Helper::menuSeamlessEdges(const QWidget *widget)
{
    if (widget) {
        auto edges = widget->property(PropertyNames::menuSeamlessEdges).value<Qt::Edges>();
        // Fallback to older property
        if (edges == Qt::Edges() && widget->property(PropertyNames::isTopMenu).toBool()) {
            edges = Qt::TopEdge;
        }
        return edges;
    }
    return Qt::Edges();
}

qreal Helper::devicePixelRatio(QPainter *painter) const
{
    return painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
}
}
