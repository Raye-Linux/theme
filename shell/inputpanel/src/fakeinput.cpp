#include "fakeinput.h"
#include "placement.h"

#include <KLocalizedString>

#include <QGuiApplication>

#include <wayland-client.h>

namespace
{
constexpr quint32 s_pressed = 1; // WL_KEYBOARD_KEY_STATE_PRESSED
constexpr quint32 s_released = 0;
constexpr quint32 s_keysymV = 0x0076; // XKB_KEY_v
}

FakeInput::FakeInput()
    : QWaylandClientExtensionTemplate<FakeInput>(6)
{
    initialize();
}

FakeInput::~FakeInput()
{
    if (isActive() && QtWayland::org_kde_kwin_fake_input::version() >= 5)
        destroy();
}

bool FakeInput::press(const QList<quint32> &keys)
{
    if (!isActive() || QtWayland::org_kde_kwin_fake_input::version() < 4)
        return false;
    if (!m_authenticated) {
        authenticate(i18nd("raye_shell_inputpanel", "Emoji and clipboard"),
                     i18nd("raye_shell_inputpanel", "Paste what you picked into the window you were typing in"));
        m_authenticated = true;
    }
    // A keysym follows the keyboard layout (V is not on the same key in
    // every layout); KWin has keysyms from version 6.
    const bool keysyms = QtWayland::org_kde_kwin_fake_input::version() >= 6;
    auto send = [this, keysyms](quint32 key, quint32 state) {
        if (keysyms && key == PasteKeys::V)
            keyboard_keysym(s_keysymV, state);
        else
            keyboard_key(key, state);
    };
    for (const quint32 key : keys)
        send(key, s_pressed);
    for (auto it = keys.crbegin(); it != keys.crend(); ++it)
        send(*it, s_released);
    if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
        wl_display_flush(wayland->display());
    return true;
}
