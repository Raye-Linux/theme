// What the right-click menu of a taskbar app offers besides pinning and
// closing: the app's own tasks (the actions of its .desktop file, "New
// window", "New private window"...) and the files it opened last. Set
// desktopId; the lists follow.
//
//   AppJumpList { id: jump; desktopId: "org.kde.kate.desktop" }
//   ... jump.recent[i].name, jump.openRecent(jump.recent[i].url)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <qqmlregistration.h>

#include <functional>

namespace JumpList
{
// "org.kde.kate.desktop" -> "org.kde.kate", the name the activity
// manager records an app's files under. "" when not a desktop id.
QString storageId(const QString &desktopId);

// The recent files to show, newest first: local files that still exist,
// each once, at most `limit`.
QList<QUrl> pickRecent(const QList<QUrl> &newestFirst, int limit, const std::function<bool(const QUrl &)> &exists);
}

class AppJumpList : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString desktopId READ desktopId WRITE setDesktopId NOTIFY desktopIdChanged)
    Q_PROPERTY(int recentLimit READ recentLimit WRITE setRecentLimit NOTIFY recentLimitChanged)
    // the app is installed: its name and icon are known, it can start
    Q_PROPERTY(bool valid READ valid NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    // [{ text, icon }] in the order of the .desktop file
    Q_PROPERTY(QVariantList actions READ actions NOTIFY changed)
    // [{ url, name, icon }], newest first
    Q_PROPERTY(QVariantList recent READ recent NOTIFY changed)

public:
    explicit AppJumpList(QObject *parent = nullptr);

    QString desktopId() const { return m_desktopId; }
    void setDesktopId(const QString &id);
    int recentLimit() const { return m_recentLimit; }
    void setRecentLimit(int limit);
    bool valid() const { return m_valid; }
    QString name() const { return m_name; }
    QString iconName() const { return m_iconName; }
    QVariantList actions() const { return m_actions; }
    QVariantList recent() const { return m_recent; }

    // read the app and its recent files again (the menu calls it when it opens)
    Q_INVOKABLE void reload();
    // a new window of the app
    Q_INVOKABLE bool launch();
    Q_INVOKABLE bool runAction(int index);
    Q_INVOKABLE bool openRecent(const QUrl &url);

Q_SIGNALS:
    void desktopIdChanged();
    void recentLimitChanged();
    void changed();

private:
    QString m_desktopId;
    int m_recentLimit = 8;
    bool m_valid = false;
    QString m_name;
    QString m_iconName;
    QVariantList m_actions;
    QVariantList m_recent;
};
