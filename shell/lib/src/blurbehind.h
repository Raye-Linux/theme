// Frosted glass behind a shell surface: asks the compositor to blur what
// lies under the window, cut to the surface's rounded shape so nothing
// shows at the corners. Nothing happens when the user turned
// transparency effects off (Settings > Personalisation > Colours), or
// when the compositor cannot blur: the surface then paints itself opaque.
//
//   BlurBehind { item: surface; radius: 8 }
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigWatcher>
#include <KSharedConfig>

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QRegion>
#include <qqmlregistration.h>

class BlurBehind : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // the surface: its window gets the blur, its rectangle is the shape
    Q_PROPERTY(QQuickItem *item READ item WRITE setItem NOTIFY itemChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    // the blur is on: draw see-through; false: draw opaque
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    explicit BlurBehind(QObject *parent = nullptr);
    ~BlurBehind() override;

    QQuickItem *item() const { return m_item; }
    void setItem(QQuickItem *item);
    qreal radius() const { return m_radius; }
    void setRadius(qreal radius);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    bool active() const { return m_active; }
    // how many times the window was told to blur or not (tests read it)
    int requests() const { return m_requests; }

    // What is asked of the compositor: can it blur, and blur (or not) this
    // window over this region. KWindowEffects unless a test puts its own.
    using CanBlur = bool (*)();
    using SetBlur = void (*)(QWindow *window, bool enable, const QRegion &region);
    static void setCompositor(CanBlur canBlur, SetBlur setBlur);

    // a rectangle with rounded corners as a region (whole pixels)
    static QRegion roundedRegion(const QRect &rect, qreal radius);
    // kdeglobals [Raye] TransparencyEffects, true when not set
    static bool transparencyEffects(const KSharedConfig::Ptr &config);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

Q_SIGNALS:
    void itemChanged();
    void radiusChanged();
    void enabledChanged();
    void activeChanged();

private:
    void schedule();
    void apply();
    void flush();
    void watchAncestors();
    void setActive(bool active);

    QPointer<QQuickItem> m_item;
    QPointer<QWindow> m_window;
    QList<QMetaObject::Connection> m_ancestors;
    qreal m_radius = 8;
    bool m_enabled = true;
    bool m_active = false;
    bool m_pending = false;
    // the next apply() sends the shape even if it did not change
    bool m_again = true;
    bool m_transparency = true;
    QMetaObject::Connection m_frames;
    int m_requests = 0;
    KSharedConfig::Ptr m_globals;
    KConfigWatcher::Ptr m_watcher;
};
