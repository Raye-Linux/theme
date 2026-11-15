// Pastes into the window the panel opened over, the way KDE Connect sends
// keys: KWin's fake input protocol, allowed for this program by
// X-KDE-Wayland-Interfaces in its desktop file. Wayland only.
#pragma once

#include "qwayland-fake-input.h"

#include <QWaylandClientExtensionTemplate>

class FakeInput : public QWaylandClientExtensionTemplate<FakeInput>, public QtWayland::org_kde_kwin_fake_input
{
public:
    FakeInput();
    ~FakeInput() override;

    // Presses the keys in order and releases them in reverse. False when
    // KWin did not give the protocol.
    bool press(const QList<quint32> &keys);

private:
    bool m_authenticated = false;
};
