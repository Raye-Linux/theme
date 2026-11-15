// The shell's options: rayeshellrc (defaults in /etc/xdg, the user's
// choices in ~/.config, written by Raye Settings), read and checked here,
// followed live through KConfigWatcher. Every part of the shell reads its
// options from this one object, never from the file itself.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigWatcher>
#include <KSharedConfig>

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVariant>
#include <qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class ShellSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // [Taskbar]
    Q_PROPERTY(QString alignment READ alignment NOTIFY taskbarChanged)
    Q_PROPERTY(QString searchMode READ searchMode NOTIFY taskbarChanged)
    Q_PROPERTY(bool taskView READ taskView NOTIFY taskbarChanged)
    Q_PROPERTY(bool widgets READ widgets NOTIFY taskbarChanged)
    Q_PROPERTY(bool autoHide READ autoHide NOTIFY taskbarChanged)
    Q_PROPERTY(bool badges READ badges NOTIFY taskbarChanged)
    Q_PROPERTY(bool flashing READ flashing NOTIFY taskbarChanged)
    Q_PROPERTY(bool allDisplays READ allDisplays NOTIFY taskbarChanged)
    Q_PROPERTY(QString multiDisplayApps READ multiDisplayApps NOTIFY taskbarChanged)
    Q_PROPERTY(bool shareWindow READ shareWindow NOTIFY taskbarChanged)
    Q_PROPERTY(bool showDesktopCorner READ showDesktopCorner NOTIFY taskbarChanged)
    Q_PROPERTY(QString combine READ combine NOTIFY taskbarChanged)
    Q_PROPERTY(QString smallButtons READ smallButtons NOTIFY taskbarChanged)
    Q_PROPERTY(bool recentSearches READ recentSearches NOTIFY taskbarChanged)
    Q_PROPERTY(bool endTask READ endTask NOTIFY taskbarChanged)
    Q_PROPERTY(QStringList taskbarPinned READ taskbarPinned WRITE setTaskbarPinned NOTIFY taskbarChanged)
    // [Tray]
    Q_PROPERTY(bool trayHiddenMenu READ trayHiddenMenu NOTIFY trayChanged)
    Q_PROPERTY(QStringList trayShown READ trayShown NOTIFY trayChanged)
    Q_PROPERTY(QStringList trayHidden READ trayHidden NOTIFY trayChanged)
    // [Start]
    Q_PROPERTY(QString startLayout READ startLayout NOTIFY startChanged)
    Q_PROPERTY(bool startRecentApps READ startRecentApps NOTIFY startChanged)
    Q_PROPERTY(bool startMostUsed READ startMostUsed NOTIFY startChanged)
    Q_PROPERTY(bool startRecommended READ startRecommended NOTIFY startChanged)
    Q_PROPERTY(QStringList startFolders READ startFolders NOTIFY startChanged)
    Q_PROPERTY(QString startAllView READ startAllView WRITE setStartAllView NOTIFY startChanged)
    Q_PROPERTY(QStringList startPinned READ startPinned WRITE setStartPinned NOTIFY startChanged)
    Q_PROPERTY(QString startPinnedFolders READ startPinnedFolders WRITE setStartPinnedFolders NOTIFY startChanged)
    Q_PROPERTY(QString restartNeeded READ restartNeeded NOTIFY startChanged)
    // [Clock]
    Q_PROPERTY(bool clockShowSeconds READ clockShowSeconds NOTIFY clockChanged)
    Q_PROPERTY(bool clockShowDate READ clockShowDate NOTIFY clockChanged)
    Q_PROPERTY(QString clockUse24h READ clockUse24h NOTIFY clockChanged)
    // [Focus]
    Q_PROPERTY(QDateTime focusUntil READ focusUntil WRITE setFocusUntil NOTIFY focusChanged)
    // [QuickSettings]
    Q_PROPERTY(QStringList quickSettingsTiles READ quickSettingsTiles WRITE setQuickSettingsTiles NOTIFY quickSettingsChanged)
    // [Notifications]
    Q_PROPERTY(int toastSeconds READ toastSeconds NOTIFY notificationsChanged)
    Q_PROPERTY(bool lockBadges READ lockBadges NOTIFY notificationsChanged)

    // [Sound] a short sound when the volume is changed from the shell
    Q_PROPERTY(bool volumeChime READ volumeChime NOTIFY soundChanged)
    // [Search]
    Q_PROPERTY(QStringList searchRecent READ searchRecent NOTIFY searchChanged)

public:
    // rayeshellrc of the session (system defaults and the user's file)
    explicit ShellSettings(QObject *parent = nullptr);
    // a given config, for the tests
    explicit ShellSettings(KSharedConfig::Ptr config, QObject *parent = nullptr);

    // most searches kept, longest search kept
    static constexpr int MaxRecentSearches = 10;
    static constexpr int MaxSearchLength = 256;

    QString alignment() const;
    QString searchMode() const;
    bool taskView() const;
    bool widgets() const;
    bool autoHide() const;
    bool badges() const;
    bool flashing() const;
    bool allDisplays() const;
    QString multiDisplayApps() const;
    bool shareWindow() const;
    bool showDesktopCorner() const;
    QString combine() const;
    QString smallButtons() const;
    bool recentSearches() const;
    bool endTask() const;
    QStringList taskbarPinned() const;
    void setTaskbarPinned(const QStringList &ids);

    bool trayHiddenMenu() const;
    QStringList trayShown() const;
    QStringList trayHidden() const;

    QString startLayout() const;
    bool startRecentApps() const;
    bool startMostUsed() const;
    bool startRecommended() const;
    QStringList startFolders() const;
    QString startAllView() const;
    void setStartAllView(const QString &view);
    QStringList startPinned() const;
    void setStartPinned(const QStringList &ids);
    QString startPinnedFolders() const;
    void setStartPinnedFolders(const QString &folders);
    QString restartNeeded() const;

    bool clockShowSeconds() const;
    bool clockShowDate() const;
    QString clockUse24h() const;

    QDateTime focusUntil() const;
    void setFocusUntil(const QDateTime &until);

    QStringList quickSettingsTiles() const;
    void setQuickSettingsTiles(const QStringList &tiles);

    int toastSeconds() const;
    bool lockBadges() const;
    bool volumeChime() const;

    QStringList searchRecent() const;
    // puts a search first in [Search] Recent (nothing when RecentSearches is off)
    Q_INVOKABLE void addRecentSearch(const QString &text);
    Q_INVOKABLE void removeRecentSearch(const QString &text);
    Q_INVOKABLE void clearRecentSearches();

    // the files pinned to an app's jump list (local file URLs only)
    Q_INVOKABLE QStringList jumpListPins(const QString &desktopId) const;
    Q_INVOKABLE void setJumpListPins(const QString &desktopId, const QStringList &urls);

    // the checked value of any key of the table (a string, bool, int or
    // string list), or an invalid QVariant for an unknown key
    Q_INVOKABLE QVariant value(const QString &group, const QString &key) const;
    // the default of a key, as /etc/xdg/rayeshellrc gives it
    static QVariant defaultValue(const QString &group, const QString &key);
    // "Group/Key" for every key the shell reads ([JumpLists] keys are per app)
    static QStringList keys();

    // read the file again (KConfigWatcher does it on its own)
    Q_INVOKABLE void reload();

    KSharedConfig::Ptr config() const;

    // the checks, shared with the tests
    static bool isDesktopId(const QString &id);
    static bool isToken(const QString &id);

Q_SIGNALS:
    void taskbarChanged();
    void trayChanged();
    void startChanged();
    void clockChanged();
    void focusChanged();
    void quickSettingsChanged();
    void notificationsChanged();
    void soundChanged();
    void searchChanged();
    void jumpListsChanged();
    // any change, with the group and the keys the writer named
    void changed(const QString &group, const QStringList &keys);

private:
    void watch();
    void notifyGroup(const QString &group);
    void write(const QString &group, const QString &key, const QVariant &value);

    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
};
