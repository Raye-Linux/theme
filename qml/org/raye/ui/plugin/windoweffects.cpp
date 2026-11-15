// SPDX-License-Identifier: GPL-3.0-or-later

#include "windoweffects.h"

#include <KConfigGroup>
#include <KWindowEffects>

#include <QEvent>
#include <QSurfaceFormat>

namespace
{
const QString s_group = QStringLiteral("Raye");
const QString s_key = QStringLiteral("TransparencyEffects");
}

WindowEffects::WindowEffects(QObject *parent)
    : QObject(parent)
    , m_config(KSharedConfig::openConfig(QStringLiteral("kdeglobals")))
{
    m_transparencyEffects = readTransparencyEffects(m_config);
    m_watcher = KConfigWatcher::create(m_config);
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == s_group) {
            setTransparencyEffects(readTransparencyEffects(m_config));
        }
    });
    m_blurAvailable = KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
}

WindowEffects::~WindowEffects()
{
    if (m_window && m_applied) {
        KWindowEffects::enableBlurBehind(m_window, false);
    }
}

bool WindowEffects::readTransparencyEffects(const KSharedConfig::Ptr &config)
{
    config->reparseConfiguration();
    return config->group(s_group).readEntry(s_key, true);
}

QWindow *WindowEffects::window() const
{
    return m_window;
}

void WindowEffects::setWindow(QWindow *window)
{
    if (m_window == window) {
        return;
    }
    if (m_window) {
        if (m_applied) {
            KWindowEffects::enableBlurBehind(m_window, false);
            m_applied = false;
        }
        m_window->removeEventFilter(this);
    }
    m_window = window;
    if (m_window) {
        // A see-through window needs an alpha channel, and that is chosen
        // once, when the window is created.
        if (!m_window->handle()) {
            QSurfaceFormat format = m_window->format();
            format.setAlphaBufferSize(8);
            m_window->setFormat(format);
        }
        m_window->installEventFilter(this);
    }
    Q_EMIT windowChanged();
    apply();
}

bool WindowEffects::blurBehind() const
{
    return m_blurBehind;
}

void WindowEffects::setBlurBehind(bool blur)
{
    if (m_blurBehind == blur) {
        return;
    }
    m_blurBehind = blur;
    Q_EMIT blurBehindChanged();
    apply();
}

bool WindowEffects::transparencyEffects() const
{
    return m_transparencyEffects;
}

void WindowEffects::setTransparencyEffects(bool on)
{
    if (m_transparencyEffects == on) {
        return;
    }
    m_transparencyEffects = on;
    Q_EMIT transparencyEffectsChanged();
    apply();
}

bool WindowEffects::blurAvailable() const
{
    return m_blurAvailable;
}

void WindowEffects::setBlurAvailable(bool available)
{
    if (m_blurAvailable == available) {
        return;
    }
    m_blurAvailable = available;
    Q_EMIT blurAvailableChanged();
}

void WindowEffects::refresh()
{
    setTransparencyEffects(readTransparencyEffects(m_config));
    setBlurAvailable(KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind));
    apply();
}

bool WindowEffects::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        switch (event->type()) {
        case QEvent::Expose:
        case QEvent::Show:
            if (m_blurBehind && !m_applied) {
                apply();
            }
            break;
        case QEvent::FocusIn:
            // the user may have turned the compositor's blur on or off
            setBlurAvailable(KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind));
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void WindowEffects::apply()
{
    if (!m_window || !m_window->isVisible()) {
        return;
    }
    const bool want = m_blurBehind && m_transparencyEffects;
    if (want == m_applied) {
        return;
    }
    KWindowEffects::enableBlurBehind(m_window, want);
    m_applied = want;
}
