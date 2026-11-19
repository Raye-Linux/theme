// Raye title bars for Qt apps on Hyprland, a QtWayland decoration plugin
// (QT_WAYLAND_DECORATION=raye, set by the Raye Hyprland session).
// Hyprland draws no title bars, so Qt draws its own: this one looks like
// the KWin decoration of the Plasma desktop.
//
// Hyprland ignores xdg_toplevel.set_minimized, so Minimize asks the Raye
// shell to put the window away, the way the taskbar does.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "titlebar.h"

#include "rayedecorationmetrics.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDateTime>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QStyleHints>
#include <QTimer>

#include <qpa/qwindowsysteminterface.h>

#include <QtWaylandClient/private/qwaylandabstractdecoration_p.h>
#include <QtWaylandClient/private/qwaylanddecorationplugin_p.h>
#include <QtWaylandClient/private/qwaylandinputdevice_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <QtWaylandClient/private/qwaylandshmbackingstore_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

#include <cmath>

using namespace Raye;
using TitleBar::Part;

namespace QtWaylandClient
{
class RayeDecoration : public QWaylandAbstractDecoration
{
public:
    RayeDecoration()
    {
        // the pointer can leave the window straight from a button, and
        // Qt tells the decoration nothing then
        m_leaveCheck.setInterval(150);
        QObject::connect(&m_leaveCheck, &QTimer::timeout, this, [this] {
            if (!m_device || m_device->pointerFocus() != waylandWindow())
                setHovered(Part::None);
        });
    }

protected:
    QMargins margins(MarginsType type = Full) const override
    {
        // Hyprland draws the border and resizes from it: only the bar here
        if (type == ShadowsOnly)
            return QMargins();
        return QMargins(0, TitleBar::height(), 0, 0);
    }

    void paint(QPaintDevice *device) override;
    bool handleMouse(QWaylandInputDevice *inputDevice, const QPointF &local, const QPointF &global,
                     Qt::MouseButtons b, Qt::KeyboardModifiers mods) override;
    bool handleTouch(QWaylandInputDevice *inputDevice, const QPointF &local, const QPointF &global,
                     QEventPoint::State state, Qt::KeyboardModifiers mods) override;

private:
    TitleBar::Buttons buttons() const
    {
        const QWindow *w = window();
        const bool fixed = w->minimumSize() == w->maximumSize() && !w->minimumSize().isEmpty();
        return TitleBar::buttonsFor(w->type() == Qt::Dialog, fixed);
    }
    qreal width() const { return waylandWindow()->surfaceSize().width(); }
    void setHovered(Part part);
    void repaint();
    void act(Part part);
    void drawGlyph(QPainter &p, Part part, const QRectF &rect, const QColor &color) const;

    Part m_hovered = Part::None;
    Part m_pressed = Part::None;
    qint64 m_lastPressMs = -1;
    QPointF m_lastPress;
    QPointer<QWaylandInputDevice> m_device;
    QTimer m_leaveCheck;
};

void RayeDecoration::repaint()
{
    update();
    // an update request alone draws nothing when the app has nothing new
    // to draw: Qt Quick windows are told to render, the others flush the
    // bar from the backing store
    QWindow *w = window();
    if (w->metaObject()->indexOfMethod("update()") >= 0) {
        QMetaObject::invokeMethod(w, "update");
    } else if (QWaylandShmBackingStore *store = waylandWindow()->backingStore()) {
        const int h = TitleBar::height();
        store->flush(w, QRegion(0, -h, waylandWindow()->surfaceSize().width(), h), QPoint());
    } else {
        w->requestUpdate();
    }
}

void RayeDecoration::setHovered(Part part)
{
    if (part != Part::Minimize && part != Part::Maximize && part != Part::Close)
        part = Part::None;
    if (part == m_hovered)
        return;
    m_hovered = part;
    if (m_hovered == Part::None)
        m_leaveCheck.stop();
    else
        m_leaveCheck.start();
    repaint();
}

void RayeDecoration::act(Part part)
{
    QWindow *w = window();
    switch (part) {
    case Part::Close:
        QWindowSystemInterface::handleCloseEvent(w);
        break;
    case Part::Maximize:
        w->setWindowStates(w->windowStates() ^ Qt::WindowMaximized);
        break;
    case Part::Minimize: {
        QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.raye.HyprShell"), QStringLiteral("/Shell"),
                                                           QStringLiteral("org.raye.HyprShell"), QStringLiteral("Minimize"));
        call << qlonglong(QCoreApplication::applicationPid()) << w->title();
        if (!QDBusConnection::sessionBus().send(call))
            w->setWindowState(Qt::WindowMinimized);
        break;
    }
    default:
        break;
    }
}

bool RayeDecoration::handleMouse(QWaylandInputDevice *inputDevice, const QPointF &local, const QPointF &global,
                                 Qt::MouseButtons b, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(global)
    Q_UNUSED(mods)
    m_device = inputDevice;
    const Part part = TitleBar::hitTest(local, width(), buttons());
    setHovered(part);

    if (part == Part::None) {
        if (isLeftReleased(b) && m_pressed != Part::None) {
            m_pressed = Part::None;
            repaint();
        }
        setMouseButtons(b);
        return false;
    }

#if QT_CONFIG(cursor)
    waylandWindow()->restoreMouseCursor(inputDevice);
#endif
    if (isRightClicked(b) && part == Part::Title) {
        showWindowMenu(inputDevice);
    } else if (isLeftClicked(b)) {
        if (part == Part::Title) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const int interval = QGuiApplication::styleHints()->mouseDoubleClickInterval();
            if (TitleBar::isDoubleClick(m_lastPressMs, m_lastPress, now, local, interval)
                && buttons().maximize) {
                m_lastPressMs = -1;
                act(Part::Maximize);
            } else {
                m_lastPressMs = now;
                m_lastPress = local;
                startMove(inputDevice, b);
            }
        } else {
            m_pressed = part;
            repaint();
        }
    } else if (isLeftReleased(b)) {
        const Part pressed = m_pressed;
        m_pressed = Part::None;
        repaint();
        if (pressed == part)
            act(part);
    }
    setMouseButtons(b);
    return true;
}

bool RayeDecoration::handleTouch(QWaylandInputDevice *inputDevice, const QPointF &local, const QPointF &global,
                                 QEventPoint::State state, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(global)
    Q_UNUSED(mods)
    const Part part = TitleBar::hitTest(local, width(), buttons());
    if (part == Part::None || state != QEventPoint::Pressed)
        return false;
    if (part == Part::Title)
        startMove(inputDevice, Qt::LeftButton);
    else
        act(part);
    return true;
}

void RayeDecoration::drawGlyph(QPainter &p, Part part, const QRectF &rect, const QColor &color) const
{
    // 10 px monoline glyphs on the device pixel grid, 1 px strokes, as the
    // KWin decoration draws them
    const qreal scale = waylandWindow()->scale();
    const qreal stroke = qMax<qreal>(1.0, qRound(scale)) / qMax<qreal>(scale, 0.1);
    const qreal half = stroke / 2;
    const QRectF glyph = DecorationMetrics::glyphRect(rect, scale);
    QPen pen(color, stroke);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (part) {
    case Part::Close:
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.drawLine(glyph.topLeft() + QPointF(half, half), glyph.bottomRight() - QPointF(half, half));
        p.drawLine(glyph.topRight() + QPointF(-half, half), glyph.bottomLeft() + QPointF(half, -half));
        break;
    case Part::Maximize: {
        const qreal radius = 1.5;
        if (window()->windowStates() & Qt::WindowMaximized) {
            const QRectF front(glyph.left() + half, glyph.top() + 2 + half, 8 - stroke, 8 - stroke);
            p.drawRoundedRect(front, radius, radius);
            QPainterPath back;
            back.moveTo(glyph.left() + 2 + half, glyph.top() + 2);
            back.lineTo(glyph.left() + 2 + half, glyph.top() + half + radius);
            back.arcTo(QRectF(glyph.left() + 2 + half, glyph.top() + half, 2 * radius, 2 * radius), 180, -90);
            back.lineTo(glyph.right() - half - radius, glyph.top() + half);
            back.arcTo(QRectF(glyph.right() - half - 2 * radius, glyph.top() + half, 2 * radius, 2 * radius), 90, -90);
            back.lineTo(glyph.right() - half, glyph.bottom() - 2 - half);
            back.lineTo(glyph.right() - 2, glyph.bottom() - 2 - half);
            p.drawPath(back);
        } else {
            p.drawRoundedRect(glyph.adjusted(half, half, -half, -half), radius, radius);
        }
        break;
    }
    case Part::Minimize: {
        const qreal s = qMax<qreal>(scale, 1.0);
        const qreal y = std::floor(glyph.center().y() * s) / s + half;
        p.drawLine(QPointF(glyph.left(), y), QPointF(glyph.right(), y));
        break;
    }
    default:
        break;
    }
}

void RayeDecoration::paint(QPaintDevice *device)
{
    const bool active = waylandWindow()->isActive();
    const qreal w = width();
    const TitleBar::Buttons b = buttons();
    const QPalette palette = QGuiApplication::palette();
    // the bar continues the window, active or not
    const QColor bar = palette.color(QPalette::Active, QPalette::Window);
    const QColor text = TitleBar::textColor(palette.color(QPalette::Active, QPalette::WindowText), active);

    QPainter p(device);
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(QRectF(0, 0, w, TitleBar::height()), bar);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const QIcon icon = waylandWindow()->windowIcon();
    if (!icon.isNull())
        icon.paint(&p, TitleBar::iconRect().toRect(), Qt::AlignCenter, active ? QIcon::Normal : QIcon::Disabled);

    const QString title = waylandWindow()->windowTitle();
    if (!title.isEmpty()) {
        QFont font = QGuiApplication::font();
        font.setPixelSize(12);
        font.setWeight(QFont::Medium);
        p.setFont(font);
        p.setPen(text);
        const QRectF r = TitleBar::titleRect(w, !icon.isNull(), b);
        p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                   p.fontMetrics().elidedText(title, Qt::ElideRight, int(r.width())));
    }

    for (Part part : {Part::Minimize, Part::Maximize, Part::Close}) {
        const QRectF r = TitleBar::buttonRect(part, w, b);
        if (!r.isValid())
            continue;
        const bool hovered = m_hovered == part;
        const bool pressed = m_pressed == part && hovered;
        const QColor back = TitleBar::buttonBackground(part, hovered, pressed, bar);
        if (back.isValid())
            p.fillRect(r, back);
        drawGlyph(p, part, r, TitleBar::glyphColor(part, hovered, pressed, text));
    }
}

class RayeDecorationPlugin : public QWaylandDecorationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QWaylandDecorationFactoryInterface_iid FILE "raye.json")
public:
    QWaylandAbstractDecoration *create(const QString &key, const QStringList &params) override
    {
        Q_UNUSED(key)
        Q_UNUSED(params)
        return new RayeDecoration();
    }
};
}

#include "decoration.moc"
