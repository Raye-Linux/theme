// When the volume sound plays. Every change asks for it; the first one
// rings at once, and the changes that follow close behind it (a held key,
// a turned wheel) ring once more when they stop. While the slider is held
// nothing rings, then one sound on release.
//
//   VolumeChime {
//       enabled: ShellSettings.volumeChime
//       onRing: if (!sink.muted) feedback.play(sink.index)
//   }
//   ... chime.changed() / chime.hold() / chime.release()
//
// Whether the output is muted is the caller's to check when it rings: a
// sound on a muted output would not be heard, and a key that mutes rings
// nothing.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <qqmlregistration.h>

class VolumeChime : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    // a change this long after the last sound rings at once; it is longer
    // than the keyboard's wait before a held key repeats (600 ms)
    Q_PROPERTY(int quietMs READ quietMs WRITE setQuietMs NOTIFY timingChanged)
    // the closing sound waits this long after the last change
    Q_PROPERTY(int settleMs READ settleMs WRITE setSettleMs NOTIFY timingChanged)
    // the slider is held
    Q_PROPERTY(bool held READ isHeld NOTIFY heldChanged)

public:
    explicit VolumeChime(QObject *parent = nullptr);

    bool isEnabled() const;
    void setEnabled(bool on);
    int quietMs() const;
    void setQuietMs(int ms);
    int settleMs() const;
    void setSettleMs(int ms);
    bool isHeld() const;

    Q_INVOKABLE void changed();
    Q_INVOKABLE void hold();
    Q_INVOKABLE void release();

Q_SIGNALS:
    void ring();
    void enabledChanged();
    void timingChanged();
    void heldChanged();

private:
    void ringNow();
    void settled();

    bool m_enabled = true;
    bool m_held = false;
    bool m_pending = false;
    int m_quietMs = 650;
    QElapsedTimer m_sinceRing;
    QTimer m_settle;
};
