// What the right-click menus of the desktop and the taskbar hold, apart
// from Plasma: the items, their order, what is ticked, and what each one
// writes to the desktop's Folder view settings. The plugin (desktopmenu.cpp)
// turns this into QActions; the tests read it directly.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

namespace RayeMenu
{

// which menu a containment gets
enum class Kind {
    None, // something else: no items, Plasma's own menu stays
    Desktop,
    Panel,
};

// Plasma::Containment::Type and Plasma::Types::FormFactor, as numbers
Kind kindFor(int containmentType, int formFactor);

// the Folder view settings the menu reads and writes (General group of
// org.kde.desktopcontainment)
struct FolderSettings {
    bool available = false; // the desktop is a Folder view
    int iconSize = 3; // index into Folder view's sizes, 3 = 64 px
    int sortMode = 0; // -1 manual, 0 name, 1 size, 2 date, 6 type
    int filterMode = 0; // 0 all, 1 only matching, 2 all but matching
    QString filterPattern = QStringLiteral("*");

    static FolderSettings fromMap(const QVariantMap &values);
};

// Folder view's sizes are an index; these are Windows' three
constexpr int SmallIcons = 1; // 32 px
constexpr int MediumIcons = 2; // 48 px
constexpr int LargeIcons = 4; // 96 px

// Ctrl + wheel on the desktop walks the whole range the setting allows,
// one step per notch; the three above are the stops the View menu offers
constexpr int SmallestIcons = 0; // 22 px
constexpr int BiggestIcons = 6; // 256 px

constexpr int SortManual = -1;
constexpr int SortName = 0;
constexpr int SortSize = 1;
constexpr int SortDate = 2;
constexpr int SortType = 6;

struct Entry {
    QString id; // stable name, also the QAction objectName
    QString text;
    QString icon;
    bool separator = false;
    bool checkable = false;
    bool checked = false;
    bool enabled = true;
    bool exclusive = false; // a choice among its neighbours (View sizes, Sort by)
    bool newMenu = false; // the "New" submenu, filled by KNewFileMenu
    QList<Entry> children;
};

// the items of the menu, in order (text in the user's language)
QList<Entry> entries(Kind kind, const FolderSettings &folder);

// the Folder view settings an item changes; empty when it changes none
QVariantMap changesFor(const QString &id, const FolderSettings &folder);

// what an item opens: a Raye Settings page, or empty
QString settingsPageFor(const QString &id);

// every id of an item that does something (for the tests)
QStringList actionIds(const QList<Entry> &entries);

} // namespace RayeMenu
