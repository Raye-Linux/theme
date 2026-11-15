// The menus on screen, to look at under Xvfb (not a test):
//
//   desktopmenu_preview [desktop|panel] [view|sort|new]
//
// opens the desktop (or taskbar) menu at the top left, with one submenu
// open. The look itself comes from the Qt widget style in use.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KPluginFactory>
#include <KPluginMetaData>
#include <Plasma/Containment>
#include <Plasma/ContainmentActions>
#include <Plasma/Corona>

#include <QApplication>
#include <QMenu>
#include <QTimer>

using namespace Qt::StringLiterals;

class PreviewCorona : public Plasma::Corona
{
public:
    QRect screenGeometry(int) const override
    {
        return QRect(0, 0, 1920, 1080);
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList args = app.arguments();
    const bool panel = args.contains(u"panel"_s);
    QString open = args.size() > 2 ? args.at(2) : QString();

    PreviewCorona corona;
    Plasma::Containment *c = corona.createContainment(panel ? u"org.kde.panel"_s : u"org.kde.desktopcontainment"_s);
    if (panel)
        c->setFormFactor(Plasma::Types::Horizontal);
    auto result = KPluginFactory::instantiatePlugin<Plasma::ContainmentActions>(
        KPluginMetaData(QStringLiteral(RAYE_DESKTOPMENU_PLUGIN)), nullptr);
    if (!result)
        return 1;
    result.plugin->setContainment(c);
    QMetaObject::invokeMethod(result.plugin, "applyFolderValues", Q_ARG(QVariantMap, (QVariantMap{{u"iconSize"_s, 2}, {u"sortMode"_s, 0}})));

    QMenu menu;
    menu.addActions(result.plugin->contextualActions());
    menu.popup(QPoint(40, 40));
    QTimer::singleShot(500, [&] {
        for (QAction *a : menu.actions()) {
            if (!open.isEmpty() && a->objectName() == open) {
                menu.setActiveAction(a);
                if (a->menu())
                    a->menu()->popup(menu.mapToGlobal(menu.actionGeometry(a).topRight()));
            }
        }
    });
    return app.exec();
}
