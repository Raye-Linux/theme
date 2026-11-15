// org.raye.desktopmenu: the right-click menu of the desktop and of the
// taskbar, in place of Plasma's. Desktop: View, Sort by, Refresh, New,
// Display settings, Personalize, Open in Terminal. Taskbar: Task Manager,
// Taskbar settings. View and Sort by write the desktop's Folder view
// settings, which apply at once.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Plasma/ContainmentActions>

#include <QPointer>
#include <QUrl>

class KNewFileMenu;
class QMenu;

class DesktopMenu : public Plasma::ContainmentActions
{
    Q_OBJECT

public:
    DesktopMenu(QObject *parent, const QVariantList &args);
    ~DesktopMenu() override;

    QList<QAction *> contextualActions() override;

    // Ctrl + wheel on the desktop resizes the icons, as it does on Windows.
    //
    // Measured in libPlasmaQuick 6.6.6: ContainmentItem::wheelEvent reads
    // the trigger, finds this plugin, then calls one of these two by the
    // SIGN of the wheel delta, once per 120 units -- a scroll UP calls
    // performPreviousAction, a scroll DOWN performNextAction. The names
    // read the other way round, so do not swap the bodies: the icons would
    // shrink when you scroll up.
    void performPreviousAction() override; // wheel up: bigger
    void performNextAction() override; // wheel down: smaller

    // the Folder view settings of the containment (empty when it has none)
    QVariantMap folderValues() const;
    // write some of them; false when the containment has no such setting
    Q_INVOKABLE bool applyFolderValues(const QVariantMap &values);

private:
    void build();
    // move the Folder view's icon size by one step, within its own range
    void stepIconSize(int by);
    void clear();
    void trigger(const QString &id);
    QUrl folderUrl() const;
    QString localFolder() const;

    QObject *m_holder = nullptr;
    QList<QPointer<QMenu>> m_menus;
    KNewFileMenu *m_newMenu = nullptr;
    QList<QAction *> m_actions;
};
