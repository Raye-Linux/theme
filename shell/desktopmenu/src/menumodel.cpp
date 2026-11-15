// SPDX-License-Identifier: GPL-3.0-or-later

#include "menumodel.h"

#include <KLocalizedString>

using namespace Qt::StringLiterals;

namespace RayeMenu
{

Kind kindFor(int containmentType, int formFactor)
{
    // Plasma::Types::FormFactor: Horizontal = 2, Vertical = 3
    const bool bar = formFactor == 2 || formFactor == 3;
    // Plasma::Containment::Type: Desktop = 0, Panel = 1, Custom = 127, CustomPanel = 128
    switch (containmentType) {
    case 1:
    case 128:
        return Kind::Panel;
    case 0:
    case 127:
        return bar ? Kind::Panel : Kind::Desktop;
    default:
        return Kind::None;
    }
}

FolderSettings FolderSettings::fromMap(const QVariantMap &values)
{
    FolderSettings s;
    s.available = values.contains(u"iconSize"_s) && values.contains(u"sortMode"_s);
    if (!s.available)
        return s;
    s.iconSize = values.value(u"iconSize"_s).toInt();
    s.sortMode = values.value(u"sortMode"_s).toInt();
    s.filterMode = values.value(u"filterMode"_s, 0).toInt();
    s.filterPattern = values.value(u"filterPattern"_s, u"*"_s).toString();
    return s;
}

static bool iconsHidden(const FolderSettings &folder)
{
    return folder.filterMode == 2 && folder.filterPattern.trimmed() == u"*"_s;
}

static Entry item(const QString &id, const QString &text, const QString &icon = {})
{
    Entry e;
    e.id = id;
    e.text = text;
    e.icon = icon;
    return e;
}

static Entry choice(const QString &id, const QString &text, bool checked)
{
    Entry e = item(id, text);
    e.checkable = true;
    e.exclusive = true;
    e.checked = checked;
    return e;
}

static Entry toggle(const QString &id, const QString &text, bool checked, bool enabled = true)
{
    Entry e = item(id, text);
    e.checkable = true;
    e.checked = checked;
    e.enabled = enabled;
    return e;
}

static Entry separator()
{
    Entry e;
    e.separator = true;
    return e;
}

static QList<Entry> desktopEntries(const FolderSettings &folder)
{
    QList<Entry> out;
    if (folder.available) {
        Entry view = item(u"view"_s, i18n("View"), u"view-list-icons"_s);
        view.children = {
            choice(u"view-large"_s, i18n("Large icons"), folder.iconSize >= LargeIcons),
            choice(u"view-medium"_s, i18n("Medium icons"), folder.iconSize > SmallIcons && folder.iconSize < LargeIcons),
            choice(u"view-small"_s, i18n("Small icons"), folder.iconSize <= SmallIcons),
            separator(),
            toggle(u"auto-arrange"_s, i18n("Auto arrange icons"), folder.sortMode != SortManual),
            // the desktop always keeps its icons on a grid
            toggle(u"align-grid"_s, i18n("Align icons to grid"), true, false),
            separator(),
            toggle(u"show-icons"_s, i18n("Show desktop icons"), !iconsHidden(folder)),
        };
        out << view;

        Entry sort = item(u"sort"_s, i18n("Sort by"), u"view-sort"_s);
        sort.children = {
            choice(u"sort-name"_s, i18n("Name"), folder.sortMode == SortName),
            choice(u"sort-size"_s, i18n("Size"), folder.sortMode == SortSize),
            choice(u"sort-type"_s, i18n("Item type"), folder.sortMode == SortType),
            choice(u"sort-date"_s, i18n("Date modified"), folder.sortMode == SortDate),
        };
        out << sort;
    }
    out << item(u"refresh"_s, i18n("Refresh"), u"view-refresh"_s);
    out << separator();
    Entry newMenu = item(u"new"_s, i18n("New"), u"list-add"_s);
    newMenu.newMenu = true;
    out << newMenu;
    out << separator();
    out << item(u"display-settings"_s, i18n("Display settings"), u"video-display"_s);
    out << item(u"personalize"_s, i18n("Personalize"), u"preferences-desktop-theme"_s);
    out << separator();
    out << item(u"terminal"_s, i18n("Open in Terminal"), u"utilities-terminal"_s);
    return out;
}

QList<Entry> entries(Kind kind, const FolderSettings &folder)
{
    switch (kind) {
    case Kind::Desktop:
        return desktopEntries(folder);
    case Kind::Panel:
        return {
            item(u"task-manager"_s, i18n("Task Manager"), u"utilities-system-monitor"_s),
            separator(),
            item(u"taskbar-settings"_s, i18n("Taskbar settings"), u"configure"_s),
        };
    case Kind::None:
        break;
    }
    return {};
}

QVariantMap changesFor(const QString &id, const FolderSettings &folder)
{
    if (!folder.available)
        return {};
    if (id == u"view-large"_s)
        return {{u"iconSize"_s, LargeIcons}};
    if (id == u"view-medium"_s)
        return {{u"iconSize"_s, MediumIcons}};
    if (id == u"view-small"_s)
        return {{u"iconSize"_s, SmallIcons}};
    if (id == u"auto-arrange"_s) {
        if (folder.sortMode == SortManual)
            // back to a sorted desktop: forget where icons were dragged
            return {{u"sortMode"_s, SortName}, {u"positions"_s, QStringList()}};
        return {{u"sortMode"_s, SortManual}};
    }
    if (id == u"show-icons"_s) {
        if (iconsHidden(folder))
            return {{u"filterMode"_s, 0}, {u"filterPattern"_s, u"*"_s}};
        // hide every icon: show none of the names matching "*"
        return {{u"filterMode"_s, 2}, {u"filterPattern"_s, u"*"_s}};
    }
    // sorting arranges the icons again, as on Windows
    if (id == u"sort-name"_s)
        return {{u"sortMode"_s, SortName}, {u"positions"_s, QStringList()}};
    if (id == u"sort-size"_s)
        return {{u"sortMode"_s, SortSize}, {u"positions"_s, QStringList()}};
    if (id == u"sort-type"_s)
        return {{u"sortMode"_s, SortType}, {u"positions"_s, QStringList()}};
    if (id == u"sort-date"_s)
        return {{u"sortMode"_s, SortDate}, {u"positions"_s, QStringList()}};
    return {};
}

QString settingsPageFor(const QString &id)
{
    if (id == u"display-settings"_s)
        return u"system/display"_s;
    if (id == u"personalize"_s)
        return u"personalization/background"_s;
    if (id == u"taskbar-settings"_s)
        return u"personalization/taskbar"_s;
    return {};
}

QStringList actionIds(const QList<Entry> &list)
{
    QStringList out;
    for (const Entry &e : list) {
        if (e.separator)
            continue;
        out << e.id;
        out << actionIds(e.children);
    }
    return out;
}

} // namespace RayeMenu
