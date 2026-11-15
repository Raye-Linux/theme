// SPDX-License-Identifier: GPL-3.0-or-later
#include "displayplan.h"

#include <algorithm>
#include <cmath>

namespace DisplayPlan
{
namespace
{
// how much room a screen takes on the desktop
int logicalWidth(const Screen &screen, QSize size)
{
    const int width = screen.sideways ? size.height() : size.width();
    const double scale = screen.scale > 0 ? screen.scale : 1.0;
    return int(std::ceil(width / scale));
}

// the biggest size every screen can do, empty when they share none
QSize commonSize(const QList<Screen> &screens)
{
    if (screens.isEmpty())
        return {};
    QList<QSize> sizes = screens.first().sizes;
    std::sort(sizes.begin(), sizes.end(), [](QSize a, QSize b) {
        return a.width() * a.height() > b.width() * b.height();
    });
    for (QSize size : std::as_const(sizes)) {
        const bool shared = std::all_of(screens.cbegin(), screens.cend(), [size](const Screen &s) {
            return s.sizes.contains(size);
        });
        if (shared)
            return size;
    }
    return {};
}

QSize comfortable(const Screen &screen)
{
    return screen.preferred.isValid() ? screen.preferred : screen.size;
}
}

int mainScreen(const QList<Screen> &screens)
{
    int best = -1;
    for (int i = 0; i < screens.size(); ++i) {
        if (best < 0) {
            best = i;
            continue;
        }
        const Screen &a = screens.at(i);
        const Screen &b = screens.at(best);
        if (a.embedded != b.embedded) {
            if (a.embedded)
                best = i;
            continue;
        }
        if (a.name.compare(b.name, Qt::CaseInsensitive) < 0)
            best = i;
    }
    return best;
}

Mode current(const QList<Screen> &screens)
{
    if (screens.size() < 2)
        return screens.isEmpty() ? Mode::Unknown : Mode::PcOnly;
    const int main = mainScreen(screens);
    int on = 0;
    for (const Screen &s : screens)
        on += s.enabled ? 1 : 0;
    if (on == 0)
        return Mode::Unknown;
    if (on == screens.size()) {
        const QPoint first = screens.first().pos;
        const bool together = std::all_of(screens.cbegin(), screens.cend(), [first](const Screen &s) {
            return s.pos == first;
        });
        return together ? Mode::Duplicate : Mode::Extend;
    }
    if (screens.at(main).enabled)
        return on == 1 ? Mode::PcOnly : Mode::Unknown;
    return Mode::SecondOnly;
}

bool canChoose(const QList<Screen> &screens)
{
    return screens.size() > 1;
}

QList<Target> plan(Mode mode, const QList<Screen> &screens)
{
    if (mode == Mode::Unknown || !canChoose(screens) || mode == current(screens))
        return {};

    const int main = mainScreen(screens);
    QList<Target> out;
    out.reserve(screens.size());

    switch (mode) {
    case Mode::PcOnly:
        for (int i = 0; i < screens.size(); ++i) {
            const Screen &s = screens.at(i);
            Target t{s.name, i == main, {}, {}, i == main ? 1 : 0};
            if (i == main)
                t.size = comfortable(s);
            out.append(t);
        }
        break;
    case Mode::SecondOnly: {
        int x = 0;
        int priority = 1;
        for (int i = 0; i < screens.size(); ++i) {
            const Screen &s = screens.at(i);
            Target t{s.name, i != main, {}, {}, 0};
            if (i != main) {
                t.size = comfortable(s);
                t.pos = QPoint(x, 0);
                t.priority = priority++;
                x += logicalWidth(s, t.size);
            }
            out.append(t);
        }
        break;
    }
    case Mode::Duplicate: {
        // no size in common: each keeps its own and the compositor scales
        const QSize shared = commonSize(screens);
        for (int i = 0; i < screens.size(); ++i) {
            const Screen &s = screens.at(i);
            Target t{s.name, true, QPoint(0, 0), shared.isValid() ? shared : comfortable(s), 0};
            out.append(t);
        }
        // main first, then the others in order
        int priority = 2;
        for (int i = 0; i < out.size(); ++i)
            out[i].priority = i == main ? 1 : priority++;
        break;
    }
    case Mode::Extend: {
        // the PC screen on the left, the others to its right
        QList<int> order{main};
        for (int i = 0; i < screens.size(); ++i) {
            if (i != main)
                order.append(i);
        }
        int x = 0;
        int priority = 1;
        out.resize(screens.size());
        for (int i : std::as_const(order)) {
            const Screen &s = screens.at(i);
            Target t{s.name, true, QPoint(x, 0), comfortable(s), priority++};
            x += logicalWidth(s, t.size);
            out[i] = t;
        }
        break;
    }
    case Mode::Unknown:
        break;
    }
    return out;
}

QString modeName(Mode mode)
{
    switch (mode) {
    case Mode::PcOnly:
        return QStringLiteral("pc-only");
    case Mode::Duplicate:
        return QStringLiteral("duplicate");
    case Mode::Extend:
        return QStringLiteral("extend");
    case Mode::SecondOnly:
        return QStringLiteral("second-only");
    case Mode::Unknown:
        break;
    }
    return {};
}

Mode modeFromName(const QString &name)
{
    for (Mode mode : {Mode::PcOnly, Mode::Duplicate, Mode::Extend, Mode::SecondOnly}) {
        if (modeName(mode) == name)
            return mode;
    }
    return Mode::Unknown;
}
}
