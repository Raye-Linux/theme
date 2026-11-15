// "Recently added" in Start: apps that arrived in the last days and were
// not opened since. An app counts as new from the first time the shell
// sees it, not from its file dates (an update rewrites those); the first
// run records every app already there as old.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <qqmlregistration.h>

#include <functional>

struct AppEntry {
    QString desktopId;
    QString name;
    QString iconName;
};

namespace RecentlyAdded
{
// First-seen times (seconds since 1970) after looking at the apps
// installed now. On the first run every app gets 0: nothing is new.
// Apps that went away are forgotten, so installing one again makes it new.
QHash<QString, qint64> updateSeen(const QHash<QString, qint64> &seen, const QStringList &installed, qint64 now, bool firstRun);

// The new apps, newest first: seen less than maxAgeDays ago, not opened
// since (lastLaunched: last start of each app, seconds since 1970), at
// most limit of them. A time far in the future (a clock that jumped
// back) is not new.
QStringList select(const QHash<QString, qint64> &seen, const QHash<QString, qint64> &lastLaunched, qint64 now, int maxAgeDays, int limit);
}

class RecentlyAddedModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int limit READ limit WRITE setLimit NOTIFY limitChanged)
    Q_PROPERTY(int maxAgeDays READ maxAgeDays WRITE setMaxAgeDays NOTIFY maxAgeDaysChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles { DesktopIdRole = Qt::UserRole + 1, NameRole, IconNameRole, AddedRole };
    Q_ENUM(Roles)

    explicit RecentlyAddedModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int limit() const { return m_limit; }
    void setLimit(int limit);
    int maxAgeDays() const { return m_maxAgeDays; }
    void setMaxAgeDays(int days);

    Q_INVOKABLE void refresh();

    // C++ only, for the tests: where the apps, their last starts, the
    // store of first-seen times and the clock come from
    void setAppSource(std::function<QList<AppEntry>()> source);
    void setLaunchSource(std::function<QHash<QString, qint64>()> source);
    void setStorePath(const QString &path);
    void setClock(std::function<qint64()> clock);

Q_SIGNALS:
    void limitChanged();
    void maxAgeDaysChanged();
    void countChanged();

private:
    static QList<AppEntry> installedApps();
    static QHash<QString, qint64> launchedApps();

    std::function<QList<AppEntry>()> m_apps;
    std::function<QHash<QString, qint64>()> m_launches;
    std::function<qint64()> m_clock;
    QString m_storePath;
    int m_limit = 3;
    int m_maxAgeDays = 7;
    QList<AppEntry> m_rows;
    QHash<QString, qint64> m_added;
    QTimer m_later;
};
