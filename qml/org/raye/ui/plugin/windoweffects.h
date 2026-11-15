// Raye's window helper for QML: asks the compositor to blur what is
// behind a window (the "Mica" backdrop of Raye's own apps) and tells the
// window whether it should be see-through at all.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigWatcher>
#include <KSharedConfig>

#include <QObject>
#include <QPointer>
#include <QWindow>

class WindowEffects : public QObject
{
    Q_OBJECT
    // the window to blur behind; a QML Window, or a child item's window
    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    // ask for the blur; nothing happens while the window is not see-through
    Q_PROPERTY(bool blurBehind READ blurBehind WRITE setBlurBehind NOTIFY blurBehindChanged)
    // Settings > Personalisation > Colours > Transparency effects
    // (kdeglobals [Raye] TransparencyEffects, true when not set)
    Q_PROPERTY(bool transparencyEffects READ transparencyEffects NOTIFY transparencyEffectsChanged)
    // the compositor can blur (false without compositing, or with the
    // blur effect turned off)
    Q_PROPERTY(bool blurAvailable READ blurAvailable NOTIFY blurAvailableChanged)

public:
    explicit WindowEffects(QObject *parent = nullptr);
    ~WindowEffects() override;

    QWindow *window() const;
    void setWindow(QWindow *window);
    bool blurBehind() const;
    void setBlurBehind(bool blur);
    bool transparencyEffects() const;
    bool blurAvailable() const;

    // read again; the compositor says nothing when its effects change
    Q_INVOKABLE void refresh();

    // the key in kdeglobals, for the tests and for Settings
    static bool readTransparencyEffects(const KSharedConfig::Ptr &config);

Q_SIGNALS:
    void windowChanged();
    void blurBehindChanged();
    void transparencyEffectsChanged();
    void blurAvailableChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void apply();
    void setTransparencyEffects(bool on);
    void setBlurAvailable(bool available);

    QPointer<QWindow> m_window;
    bool m_blurBehind = false;
    bool m_applied = false;
    bool m_transparencyEffects = true;
    bool m_blurAvailable = false;
    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
};
