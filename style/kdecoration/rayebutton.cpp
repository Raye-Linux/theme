/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "rayebutton.h"

#include <KColorUtils>
#include <KDecoration3/DecoratedWindow>
#include <KIconLoader>

#include <cmath>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace Raye
{
using KDecoration3::ColorGroup;
using KDecoration3::ColorRole;
using KDecoration3::DecorationButtonType;

//__________________________________________________________________
Button::Button(DecorationButtonType type, Decoration *decoration, QObject *parent)
    : DecorationButton(type, decoration, parent)
    , m_animation(new QVariantAnimation(this))
{
    // setup animation
    // It is important start and end value are of the same type, hence 0.0 and not just 0
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setOpacity(value.toReal());
    });

    // connections
    connect(decoration, &Decoration::tabletModeChanged, this, &Button::reconfigure);
    connect(decoration->window(), SIGNAL(iconChanged(QIcon)), this, SLOT(update()));
    connect(decoration->settings().get(), &KDecoration3::DecorationSettings::reconfigured, this, &Button::reconfigure);
    connect(this, &KDecoration3::DecorationButton::hoveredChanged, this, &Button::updateAnimationState);

    // resting on Maximize opens Snap layouts, as on Windows
    if (type == DecorationButtonType::Maximize) {
        m_snapTimer = new QTimer(this);
        m_snapTimer->setSingleShot(true);
        m_snapTimer->setInterval(DecorationMetrics::SnapLayoutsDelayMs);
        connect(m_snapTimer, &QTimer::timeout, this, &Button::requestSnapLayouts);
        connect(this, &KDecoration3::DecorationButton::hoveredChanged, this, [this](bool hovered) {
            if (hovered && isEnabled()) {
                m_snapTimer->start();
            } else {
                m_snapTimer->stop();
            }
        });
        connect(this, &KDecoration3::DecorationButton::pressedChanged, m_snapTimer, &QTimer::stop);
    }

    reconfigure();
}

void Button::requestSnapLayouts()
{
    if (!isHovered() || isPressed()) {
        return;
    }
    // The decoration cannot know KWin's id of its window nor where the window
    // is on screen: it sends an empty id and the point under the button in
    // window coordinates; the service takes the window under the pointer.
    // Sent without waiting for a reply, so KWin never blocks on it; when no
    // service runs the call just fails.
    const QPointF anchor(geometry().center().x(), geometry().bottom());
    QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String(DecorationMetrics::SnapService),
                                                          QLatin1String(DecorationMetrics::SnapPath),
                                                          QLatin1String(DecorationMetrics::SnapInterface),
                                                          QLatin1String(DecorationMetrics::SnapMethod));
    message.setArguments({QString(), qRound(anchor.x()), qRound(anchor.y())});
    message.setAutoStartService(false);
    QDBusConnection::sessionBus().send(message);
}

//__________________________________________________________________
Button::Button(QObject *parent, const QVariantList &args)
    : Button(args.at(0).value<DecorationButtonType>(), args.at(1).value<Decoration *>(), parent)
{
    setGeometry(QRectF(QPointF(0, 0), preferredSize()));
}

//__________________________________________________________________
Button *Button::create(DecorationButtonType type, KDecoration3::Decoration *decoration, QObject *parent)
{
    if (auto d = qobject_cast<Decoration *>(decoration)) {
        Button *b = new Button(type, d, parent);
        const auto c = d->window();
        switch (type) {
        case DecorationButtonType::Close:
            b->setVisible(c->isCloseable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::closeableChanged, b, &Raye::Button::setVisible);
            break;

        case DecorationButtonType::Maximize:
            b->setVisible(c->isMaximizeable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::maximizeableChanged, b, &Raye::Button::setVisible);
            break;

        case DecorationButtonType::Minimize:
            b->setVisible(c->isMinimizeable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::minimizeableChanged, b, &Raye::Button::setVisible);
            break;

        case DecorationButtonType::ContextHelp:
            b->setVisible(c->providesContextHelp());
            QObject::connect(c, &KDecoration3::DecoratedWindow::providesContextHelpChanged, b, &Raye::Button::setVisible);
            break;

        case DecorationButtonType::Shade:
            b->setVisible(c->isShadeable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::shadeableChanged, b, &Raye::Button::setVisible);
            break;

        case DecorationButtonType::Menu:
            QObject::connect(c, &KDecoration3::DecoratedWindow::iconChanged, b, [b]() {
                b->update();
            });
            break;

        default:
            break;
        }

        return b;
    }

    return nullptr;
}

//__________________________________________________________________
void Button::paint(QPainter *painter, const QRectF &repaintRegion)
{
    Q_UNUSED(repaintRegion)

    auto d = qobject_cast<Decoration *>(decoration());
    if (!d) {
        return;
    }

    if (type() == KDecoration3::DecorationButtonType::Spacer) {
        return;
    }

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing);
    // the window corners clip the backplate (the red close button follows the top right corner)
    painter->setClipPath(d->titleBarShape(), Qt::IntersectClip);

    const QColor background(backgroundColor());
    if (background.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(background);
        painter->drawRect(geometry());
    }

    if (type() == KDecoration3::DecorationButtonType::Menu) {
        // the window icon, 16 px
        const qreal size(DecorationMetrics::IconSize * d->unit());
        const QRectF iconRect(geometry().center() - QPointF(size / 2, size / 2), QSizeF(size, size));
        d->window()->icon().paint(painter, iconRect.toRect());
    } else {
        drawIcon(painter);
    }
    painter->restore();
}

void Button::drawIcon(QPainter *painter) const
{
    auto d = qobject_cast<Decoration *>(decoration());
    if (!d) {
        return;
    }

    const QColor foreground(foregroundColor());
    if (!foreground.isValid()) {
        return;
    }

    // 10 px monoline glyphs on the device pixel grid, 1 px strokes
    const qreal scale(d->window()->scale());
    const qreal pixel(1.0 / qMax<qreal>(scale, 1.0));
    const qreal stroke(qMax<qreal>(1.0, qRound(scale)) / qMax<qreal>(scale, 0.1));
    const QRectF glyph(DecorationMetrics::glyphRect(geometry(), scale));
    // centre a 1 px line on a pixel row
    const qreal half(stroke / 2);

    QPen pen(foreground, stroke);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    switch (type()) {
    case DecorationButtonType::Close: {
        pen.setCapStyle(Qt::RoundCap);
        painter->setPen(pen);
        painter->drawLine(glyph.topLeft() + QPointF(half, half), glyph.bottomRight() - QPointF(half, half));
        painter->drawLine(glyph.topRight() + QPointF(-half, half), glyph.bottomLeft() + QPointF(half, -half));
        break;
    }

    case DecorationButtonType::Maximize: {
        const qreal radius(1.5);
        if (isChecked()) {
            // restore: a square with a second one behind it, up and right
            const QRectF front(glyph.left() + half, glyph.top() + 2 + half, 8 - stroke, 8 - stroke);
            painter->drawRoundedRect(front, radius, radius);
            QPainterPath back;
            back.moveTo(glyph.left() + 2 + half, glyph.top() + 2);
            back.lineTo(glyph.left() + 2 + half, glyph.top() + half + radius);
            back.arcTo(QRectF(glyph.left() + 2 + half, glyph.top() + half, 2 * radius, 2 * radius), 180, -90);
            back.lineTo(glyph.right() - half - radius, glyph.top() + half);
            back.arcTo(QRectF(glyph.right() - half - 2 * radius, glyph.top() + half, 2 * radius, 2 * radius), 90, -90);
            back.lineTo(glyph.right() - half, glyph.bottom() - 2 - half);
            back.lineTo(glyph.right() - 2, glyph.bottom() - 2 - half);
            painter->drawPath(back);
        } else {
            painter->drawRoundedRect(glyph.adjusted(half, half, -half, -half), radius, radius);
        }
        break;
    }

    case DecorationButtonType::Minimize: {
        // on a device pixel row, so the line stays crisp
        const qreal y(std::floor(glyph.center().y() * qMax<qreal>(scale, 1.0)) / qMax<qreal>(scale, 1.0) + half);
        painter->drawLine(QPointF(glyph.left(), y), QPointF(glyph.right(), y));
        break;
    }

    case DecorationButtonType::OnAllDesktops: {
        // a pin
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        const QPointF c(glyph.center());
        if (isChecked()) {
            painter->setBrush(foreground);
        }
        painter->drawEllipse(QRectF(c.x() - 3, glyph.top() + half, 6, 6));
        painter->drawLine(QPointF(c.x(), glyph.top() + 6), QPointF(c.x(), glyph.bottom()));
        break;
    }

    case DecorationButtonType::Shade: {
        const qreal y(glyph.top() + 2 + half);
        painter->drawLine(QPointF(glyph.left(), y), QPointF(glyph.right(), y));
        QPainterPath chevron;
        if (isChecked()) {
            chevron.moveTo(glyph.left() + 1, glyph.top() + 5);
            chevron.lineTo(glyph.center().x(), glyph.bottom() - 1);
            chevron.lineTo(glyph.right() - 1, glyph.top() + 5);
        } else {
            chevron.moveTo(glyph.left() + 1, glyph.bottom() - 1);
            chevron.lineTo(glyph.center().x(), glyph.top() + 5);
            chevron.lineTo(glyph.right() - 1, glyph.bottom() - 1);
        }
        painter->drawPath(chevron);
        break;
    }

    case DecorationButtonType::KeepBelow:
    case DecorationButtonType::KeepAbove: {
        // two chevrons, pointing the way the window goes
        const bool up(type() == DecorationButtonType::KeepAbove);
        for (const qreal dy : {0.0, 4.0}) {
            QPainterPath chevron;
            const qreal top(glyph.top() + 1 + dy);
            if (up) {
                chevron.moveTo(glyph.left() + 1, top + 4);
                chevron.lineTo(glyph.center().x(), top);
                chevron.lineTo(glyph.right() - 1, top + 4);
            } else {
                chevron.moveTo(glyph.left() + 1, top);
                chevron.lineTo(glyph.center().x(), top + 4);
                chevron.lineTo(glyph.right() - 1, top);
            }
            painter->drawPath(chevron);
        }
        if (isChecked()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(foreground);
            painter->drawEllipse(QPointF(glyph.center().x(), up ? glyph.bottom() : glyph.top()), 1, 1);
        }
        break;
    }

    case DecorationButtonType::ExcludeFromCapture: {
        // a camera lens crossed out when the window is hidden from captures
        painter->drawEllipse(glyph.adjusted(1.5, 1.5, -1.5, -1.5));
        if (isChecked()) {
            painter->drawLine(glyph.topLeft() + QPointF(half, half), glyph.bottomRight() - QPointF(half, half));
        }
        break;
    }

    case DecorationButtonType::ApplicationMenu: {
        for (const qreal y : {1.0, 5.0, 9.0}) {
            painter->drawLine(QPointF(glyph.left(), glyph.top() + y + half - pixel / 2), QPointF(glyph.right(), glyph.top() + y + half - pixel / 2));
        }
        break;
    }

    case DecorationButtonType::ContextHelp: {
        pen.setCapStyle(Qt::RoundCap);
        painter->setPen(pen);
        QPainterPath path;
        path.moveTo(glyph.left() + 2, glyph.top() + 3);
        path.arcTo(QRectF(glyph.left() + 2, glyph.top(), 6, 5), 180, -180);
        path.cubicTo(QPointF(glyph.right() - 2, glyph.top() + 5), QPointF(glyph.center().x(), glyph.top() + 5), QPointF(glyph.center().x(), glyph.top() + 7));
        painter->drawPath(path);
        painter->drawPoint(QPointF(glyph.center().x(), glyph.bottom()));
        break;
    }

    default:
        break;
    }
}

QColor Button::foregroundColor() const
{
    auto d = qobject_cast<Decoration *>(decoration());
    if (!d) {
        return QColor();
    }
    // white on the red close button; otherwise the title colour, which is
    // text.primary on the active window and text.tertiary on the others
    if (type() == DecorationButtonType::Close && (isHovered() || isPressed())) {
        return DecorationMetrics::closeGlyphOnRed();
    }
    if (!isEnabled()) {
        QColor color(d->fontColor());
        color.setAlphaF(color.alphaF() * 0.4);
        return color;
    }
    return d->fontColor();
}

QColor Button::backgroundColor() const
{
    auto d = qobject_cast<Decoration *>(decoration());
    if (!d || !isEnabled()) {
        return QColor();
    }

    const bool close(type() == DecorationButtonType::Close);
    const bool animating(m_animation->state() == QAbstractAnimation::Running);
    const bool checkedToggle((type() == DecorationButtonType::KeepBelow || type() == DecorationButtonType::KeepAbove || type() == DecorationButtonType::Shade
                              || type() == DecorationButtonType::OnAllDesktops || type() == DecorationButtonType::ExcludeFromCapture)
                             && isChecked());

    QColor color(DecorationMetrics::buttonBackground(close, isHovered() || animating || checkedToggle, isPressed(), d->titleBarColor(), d->isDark()));
    if (color.isValid() && animating && !isPressed()) {
        // hover fades in and out (motion.faster)
        color.setAlphaF(color.alphaF() * m_opacity);
    }
    return color;
}

void Button::reconfigure()
{
    // animation
    auto d = qobject_cast<Decoration *>(decoration());
    if (!d) {
        return;
    }

    // caption buttons are 46x32, the height of the title bar
    const qreal height(DecorationMetrics::ButtonHeight * d->unit());
    switch (type()) {
    case KDecoration3::DecorationButtonType::Spacer:
        setPreferredSize(QSizeF(DecorationMetrics::IconMargin * d->unit(), height));
        break;
    case KDecoration3::DecorationButtonType::Menu:
        setPreferredSize(QSizeF((DecorationMetrics::IconSize + 2 * DecorationMetrics::IconMargin) * d->unit(), height));
        break;
    default:
        setPreferredSize(QSizeF(DecorationMetrics::ButtonWidth * d->unit(), height));
        break;
    }

    m_animation->setDuration(d->animationsDuration());
}

//__________________________________________________________________
void Button::updateAnimationState(bool hovered)
{
    auto d = qobject_cast<Decoration *>(decoration());
    if (!(d && d->animationsDuration() > 0)) {
        return;
    }

    m_animation->setDirection(hovered ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
    if (m_animation->state() != QAbstractAnimation::Running) {
        m_animation->start();
    }
}

} // namespace
