// SPDX-License-Identifier: GPL-3.0-or-later

#include "blurbehind.h"

#include <KConfigGroup>
#include <KWindowEffects>

#include <QHash>
#include <QPainterPath>
#include <QQuickWindow>
#include <QTimer>

#include <cmath>

using namespace Qt::StringLiterals;

namespace
{
bool compositorCanBlur()
{
    return KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
}

void compositorSetBlur(QWindow *window, bool enable, const QRegion &region)
{
    KWindowEffects::enableBlurBehind(window, enable, region);
}

BlurBehind::CanBlur s_canBlur = compositorCanBlur;
BlurBehind::SetBlur s_setBlur = compositorSetBlur;

// The compositor keeps one blur region per window, and a window can hold
// more than one surface (the notification centre has two cards): each
// surface puts its shape here and the window is given all of them at once.
QHash<QWindow *, QHash<const BlurBehind *, QRegion>> &shapes()
{
    static QHash<QWindow *, QHash<const BlurBehind *, QRegion>> all;
    return all;
}

void tellWindow(QWindow *window)
{
    QRegion region;
    for (const QRegion &shape : shapes().value(window))
        region += shape;
    if (region.isEmpty()) {
        shapes().remove(window);
        s_setBlur(window, false, QRegion());
    } else {
        s_setBlur(window, true, region);
    }
}
}

void BlurBehind::setCompositor(CanBlur canBlur, SetBlur setBlur)
{
    s_canBlur = canBlur ? canBlur : compositorCanBlur;
    s_setBlur = setBlur ? setBlur : compositorSetBlur;
}

BlurBehind::BlurBehind(QObject *parent)
    : QObject(parent)
    , m_globals(KSharedConfig::openConfig(u"kdeglobals"_s))
{
    m_watcher = KConfigWatcher::create(m_globals);
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == u"Raye"_s)
            schedule();
    });
}

BlurBehind::~BlurBehind()
{
    if (m_window && shapes().contains(m_window) && shapes()[m_window].remove(this))
        tellWindow(m_window);
}

bool BlurBehind::transparencyEffects(const KSharedConfig::Ptr &config)
{
    return config->group(u"Raye"_s).readEntry("TransparencyEffects", true);
}

QRegion BlurBehind::roundedRegion(const QRect &rect, qreal radius)
{
    if (rect.isEmpty())
        return {};
    const qreal r = qBound<qreal>(0, radius, qMin(rect.width(), rect.height()) / 2.0);
    if (r < 1)
        return QRegion(rect);
    // one strip per row of the corners, the straight middle in one piece
    QRegion region(rect.adjusted(0, int(r), 0, -int(r)));
    const int rows = int(std::ceil(r));
    for (int i = 0; i < rows; ++i) {
        const qreal dy = r - (i + 0.5);
        const int inset = int(std::round(r - std::sqrt(qMax<qreal>(0, r * r - dy * dy))));
        const QRect top(rect.left() + inset, rect.top() + i, rect.width() - 2 * inset, 1);
        const QRect bottom(rect.left() + inset, rect.bottom() - i, rect.width() - 2 * inset, 1);
        region += top;
        region += bottom;
    }
    return region;
}

void BlurBehind::setItem(QQuickItem *item)
{
    if (item == m_item)
        return;
    if (m_item)
        m_item->disconnect(this);
    m_item = item;
    watchAncestors();
    if (m_item) {
        connect(m_item, &QQuickItem::parentChanged, this, [this] {
            watchAncestors();
            schedule();
        });
        connect(m_item, &QQuickItem::windowChanged, this, &BlurBehind::schedule);
        connect(m_item, &QQuickItem::widthChanged, this, &BlurBehind::schedule);
        connect(m_item, &QQuickItem::heightChanged, this, &BlurBehind::schedule);
        connect(m_item, &QQuickItem::xChanged, this, &BlurBehind::schedule);
        connect(m_item, &QQuickItem::yChanged, this, &BlurBehind::schedule);
        connect(m_item, &QQuickItem::visibleChanged, this, &BlurBehind::schedule);
    }
    Q_EMIT itemChanged();
    schedule();
}

// The shape follows the surface on screen: a panel that slides in moves a
// parent, not the surface itself, and the blur has to go with it.
void BlurBehind::watchAncestors()
{
    for (const QMetaObject::Connection &c : std::as_const(m_ancestors))
        disconnect(c);
    m_ancestors.clear();
    for (QQuickItem *a = m_item ? m_item->parentItem() : nullptr; a; a = a->parentItem()) {
        m_ancestors << connect(a, &QQuickItem::xChanged, this, &BlurBehind::schedule);
        m_ancestors << connect(a, &QQuickItem::yChanged, this, &BlurBehind::schedule);
        m_ancestors << connect(a, &QQuickItem::parentChanged, this, [this] {
            watchAncestors();
            schedule();
        });
    }
}

void BlurBehind::setRadius(qreal radius)
{
    if (qFuzzyCompare(radius, m_radius))
        return;
    m_radius = radius;
    Q_EMIT radiusChanged();
    schedule();
}

void BlurBehind::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    Q_EMIT enabledChanged();
    schedule();
}

void BlurBehind::setActive(bool active)
{
    if (active == m_active)
        return;
    m_active = active;
    Q_EMIT activeChanged();
}

// A Plasma dialog with no background of its own turns the blur off when it
// is shown, exposed or resized (libplasma's DialogPrivate::updateTheme).
// Ask again once the window is done with the event.
bool BlurBehind::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Expose:
        case QEvent::Resize:
            schedule();
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void BlurBehind::schedule()
{
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(0, this, &BlurBehind::apply);
}

void BlurBehind::apply()
{
    m_pending = false;
    ++m_requests;
    QWindow *window = m_item ? m_item->window() : nullptr;
    if (m_window && m_window != window) {
        if (shapes().contains(m_window) && shapes()[m_window].remove(this))
            tellWindow(m_window);
        m_window->removeEventFilter(this);
    }
    if (window && window != m_window) {
        window->installEventFilter(this);
        connect(window, &QObject::destroyed, this, [window] {
            shapes().remove(window);
        });
    }
    m_window = window;
    m_globals->reparseConfiguration();
    const bool want = m_enabled && m_item && m_item->isVisible() && window && transparencyEffects(m_globals)
        && s_canBlur();
    if (!want) {
        if (window && shapes().contains(window) && shapes()[window].remove(this))
            tellWindow(window);
        setActive(false);
        return;
    }
    const QRectF r = m_item->mapRectToScene(QRectF(0, 0, m_item->width(), m_item->height()));
    shapes()[window].insert(this, roundedRegion(r.toAlignedRect(), m_radius));
    tellWindow(window);
    setActive(true);
}
