// SPDX-License-Identifier: GPL-3.0-or-later

#include "pinrules.h"

namespace RayePins
{

Buttons buttonsFrom(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v == QLatin1String("main-and-window"))
        return Buttons::MainAndWindow;
    if (v == QLatin1String("window-only"))
        return Buttons::WindowOnly;
    return Buttons::All;
}

QHash<QString, QStringList> afterChange(const QList<Bar> &bars, const QString &changed, Buttons buttons)
{
    QHash<QString, QStringList> out;
    const Bar *source = nullptr;
    const Bar *main = nullptr;
    for (const Bar &b : bars) {
        if (b.id == changed)
            source = &b;
        if (b.main && !main)
            main = &b;
    }
    if (!source)
        return out;

    if (buttons == Buttons::All) {
        // the last change wins everywhere: pin, unpin or move
        for (const Bar &b : bars) {
            if (b.id != changed && b.launchers != source->launchers)
                out.insert(b.id, source->launchers);
        }
        return out;
    }

    // Only the main taskbar keeps pins. Something pinned on a copy goes
    // to the main taskbar, and the copies stay without pins.
    if (!source->main && main) {
        QStringList merged = main->launchers;
        for (const QString &l : source->launchers) {
            if (!merged.contains(l))
                merged.append(l);
        }
        if (merged != main->launchers)
            out.insert(main->id, merged);
    }
    for (const Bar &b : bars) {
        if (!b.main && !b.launchers.isEmpty())
            out.insert(b.id, {});
    }
    return out;
}

} // namespace RayePins
