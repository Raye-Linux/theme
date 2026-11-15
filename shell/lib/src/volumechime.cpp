// SPDX-License-Identifier: GPL-3.0-or-later

#include "volumechime.h"

VolumeChime::VolumeChime(QObject *parent)
    : QObject(parent)
{
    m_settle.setSingleShot(true);
    m_settle.setInterval(150);
    connect(&m_settle, &QTimer::timeout, this, &VolumeChime::settled);
}

bool VolumeChime::isEnabled() const
{
    return m_enabled;
}

void VolumeChime::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    if (!on) {
        m_pending = false;
        m_settle.stop();
    }
    Q_EMIT enabledChanged();
}

int VolumeChime::quietMs() const
{
    return m_quietMs;
}

void VolumeChime::setQuietMs(int ms)
{
    if (ms < 0 || ms == m_quietMs)
        return;
    m_quietMs = ms;
    Q_EMIT timingChanged();
}

int VolumeChime::settleMs() const
{
    return m_settle.interval();
}

void VolumeChime::setSettleMs(int ms)
{
    if (ms < 0 || ms == m_settle.interval())
        return;
    m_settle.setInterval(ms);
    Q_EMIT timingChanged();
}

bool VolumeChime::isHeld() const
{
    return m_held;
}

void VolumeChime::changed()
{
    if (!m_enabled)
        return;
    if (m_held) {
        m_pending = true;
        return;
    }
    if (!m_pending && (!m_sinceRing.isValid() || m_sinceRing.elapsed() >= m_quietMs)) {
        ringNow();
        return;
    }
    m_pending = true;
    m_settle.start();
}

void VolumeChime::hold()
{
    if (m_held)
        return;
    m_held = true;
    m_settle.stop();
    Q_EMIT heldChanged();
}

void VolumeChime::release()
{
    if (!m_held)
        return;
    m_held = false;
    Q_EMIT heldChanged();
    if (m_pending && m_enabled)
        ringNow();
}

void VolumeChime::ringNow()
{
    m_pending = false;
    m_settle.stop();
    m_sinceRing.start();
    Q_EMIT ring();
}

void VolumeChime::settled()
{
    if (m_pending && !m_held && m_enabled)
        ringNow();
}
