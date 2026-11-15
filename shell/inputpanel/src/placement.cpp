#include "placement.h"

#include <QStringList>

#include <algorithm>

namespace Placement
{

QPoint near(const QRect &screen, const QPoint &pointer, const QSize &panel, int taskbarHeight)
{
    const int left = screen.left() + Gap;
    const int right = screen.right() + 1 - Gap - panel.width();
    const int top = screen.top() + Gap;
    const int bottom = screen.bottom() + 1 - taskbarHeight - Gap - panel.height();

    int x = pointer.x() - 2 * Gap;
    int y = pointer.y() + 2 * Gap;
    if (y > bottom) {
        // no room below: above the pointer, or as low as it fits
        const int above = pointer.y() - 2 * Gap - panel.height();
        y = above >= top ? above : bottom;
    }
    x = std::clamp(x, left, std::max(left, right));
    y = std::clamp(y, top, std::max(top, bottom));
    return {x, y};
}

QPoint corner(const QRect &screen, const QSize &panel, int taskbarHeight)
{
    return {std::max(screen.left() + Gap, screen.right() + 1 - Gap - panel.width()),
            std::max(screen.top() + Gap, screen.bottom() + 1 - taskbarHeight - Gap - panel.height())};
}

} // namespace Placement

namespace PasteKeys
{

bool isTerminal(const QString &windowClass)
{
    static const QStringList terminals = {
        QStringLiteral("konsole"),        QStringLiteral("org.kde.konsole"), QStringLiteral("org.raye.terminal"),
        QStringLiteral("yakuake"),        QStringLiteral("org.kde.yakuake"), QStringLiteral("kitty"),
        QStringLiteral("alacritty"),      QStringLiteral("foot"),            QStringLiteral("footclient"),
        QStringLiteral("wezterm"),        QStringLiteral("org.wezfurlong.wezterm"), QStringLiteral("ghostty"),
        QStringLiteral("com.mitchellh.ghostty"), QStringLiteral("gnome-terminal-server"), QStringLiteral("org.gnome.terminal"),
        QStringLiteral("org.gnome.console"), QStringLiteral("kgx"),          QStringLiteral("xterm"),
        QStringLiteral("uxterm"),         QStringLiteral("urxvt"),           QStringLiteral("terminator"),
        QStringLiteral("tilix"),          QStringLiteral("com.gexperts.tilix"), QStringLiteral("xfce4-terminal"),
        QStringLiteral("qterminal"),      QStringLiteral("lxterminal"),      QStringLiteral("st"),
    };
    return terminals.contains(windowClass.trimmed().toLower());
}

QList<quint32> forWindow(const QString &windowClass)
{
    if (isTerminal(windowClass))
        return {LeftCtrl, LeftShift, V};
    return {LeftCtrl, V};
}

} // namespace PasteKeys
