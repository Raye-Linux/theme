// The C++ part of org.raye.ui, imported as org.raye.ui.effects: what QML
// cannot do on its own. A module of its own, so the QML primitives still
// load from the source tree, where it is not built.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "windoweffects.h"

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class RayeUiPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("org.raye.ui.effects"));
        qmlRegisterType<WindowEffects>(uri, 1, 0, "WindowEffects");
    }
};

#include "rayeuiplugin.moc"
