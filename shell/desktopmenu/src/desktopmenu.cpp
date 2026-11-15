// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopmenu.h"
#include "menumodel.h"

#include <KConfigLoader>
#include <KDirNotify>
#include <KIO/ApplicationLauncherJob>
#include <KIO/CommandLauncherJob>
#include <KLocalizedString>
#include <KNewFileMenu>
#include <KNotificationJobUiDelegate>
#include <KPluginFactory>
#include <KService>
#include <KTerminalLauncherJob>
#include <Plasma/Containment>

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QMenu>
#include <QStandardPaths>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

K_PLUGIN_CLASS_WITH_JSON(DesktopMenu, "metadata.json")

// the settings View and Sort by read
static const QStringList s_folderKeys = {u"iconSize"_s, u"sortMode"_s, u"filterMode"_s, u"filterPattern"_s};

DesktopMenu::DesktopMenu(QObject *parent, const QVariantList &args)
    : Plasma::ContainmentActions(parent, args)
{
}

DesktopMenu::~DesktopMenu()
{
    clear();
}

void DesktopMenu::clear()
{
    m_actions.clear();
    // a menu still on screen keeps its actions until it closes
    for (const QPointer<QMenu> &menu : std::as_const(m_menus)) {
        if (menu)
            menu->deleteLater();
    }
    m_menus.clear();
    if (m_holder) {
        m_holder->deleteLater();
        m_holder = nullptr;
    }
}

QVariantMap DesktopMenu::folderValues() const
{
    Plasma::Containment *c = const_cast<DesktopMenu *>(this)->containment();
    KConfigLoader *scheme = c ? c->configScheme() : nullptr;
    if (!scheme)
        return {};
    QVariantMap values;
    for (const QString &key : s_folderKeys) {
        if (KConfigSkeletonItem *item = scheme->findItemByName(key))
            values.insert(key, item->property());
    }
    return values;
}

bool DesktopMenu::applyFolderValues(const QVariantMap &values)
{
    Plasma::Containment *c = containment();
    KConfigLoader *scheme = c ? c->configScheme() : nullptr;
    if (!scheme || values.isEmpty())
        return false;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (!scheme->findItemByName(it.key()))
            return false;
    }
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        scheme->findItemByName(it.key())->setProperty(it.value());
    // save() tells the Folder view, which applies the change at once
    scheme->save();
    Q_EMIT c->configNeedsSaving();
    return true;
}

QUrl DesktopMenu::folderUrl() const
{
    Plasma::Containment *c = const_cast<DesktopMenu *>(this)->containment();
    KConfigLoader *scheme = c ? c->configScheme() : nullptr;
    if (KConfigSkeletonItem *item = scheme ? scheme->findItemByName(u"url"_s) : nullptr) {
        const QUrl url = QUrl::fromUserInput(item->property().toString());
        if (url.isValid() && !url.isEmpty())
            return url;
    }
    return QUrl(u"desktop:/"_s);
}

QString DesktopMenu::localFolder() const
{
    const QUrl url = folderUrl();
    if (url.isLocalFile())
        return url.toLocalFile();
    return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

static void fill(QMenu *menu, QList<QAction *> *flat, QObject *holder, const QList<RayeMenu::Entry> &entries,
                 const std::function<QAction *(const RayeMenu::Entry &)> &make)
{
    QActionGroup *group = nullptr;
    for (const RayeMenu::Entry &e : entries) {
        QAction *a = make(e);
        if (!a)
            continue;
        if (e.exclusive) {
            if (!group) {
                group = new QActionGroup(holder);
                group->setExclusive(true);
            }
            group->addAction(a);
        } else if (e.separator) {
            group = nullptr;
        }
        if (menu)
            menu->addAction(a);
        else
            flat->append(a);
    }
}

void DesktopMenu::build()
{
    clear();
    m_holder = new QObject(this);

    Plasma::Containment *c = containment();
    const RayeMenu::Kind kind = c ? RayeMenu::kindFor(c->containmentType(), c->formFactor()) : RayeMenu::Kind::None;
    const auto folder = RayeMenu::FolderSettings::fromMap(folderValues());
    const QList<RayeMenu::Entry> entries = RayeMenu::entries(kind, folder);

    std::function<QAction *(const RayeMenu::Entry &)> make = [&](const RayeMenu::Entry &e) -> QAction * {
        if (e.separator) {
            auto *sep = new QAction(m_holder);
            sep->setSeparator(true);
            return sep;
        }
        if (e.newMenu) {
            if (!m_newMenu) {
                m_newMenu = new KNewFileMenu(this);
                m_newMenu->setModal(false);
            }
            m_newMenu->setObjectName(e.id);
            m_newMenu->setText(e.text);
            m_newMenu->setIcon(QIcon::fromTheme(e.icon));
            m_newMenu->setWorkingDirectory(folderUrl());
            m_newMenu->checkUpToDate();
            return m_newMenu;
        }
        auto *a = new QAction(QIcon::fromTheme(e.icon), e.text, m_holder);
        a->setObjectName(e.id);
        a->setCheckable(e.checkable);
        a->setChecked(e.checked);
        a->setEnabled(e.enabled);
        a->setData(RayeMenu::settingsPageFor(e.id));
        if (!e.children.isEmpty()) {
            auto *sub = new QMenu();
            m_menus.append(sub);
            fill(sub, nullptr, m_holder, e.children, make);
            a->setMenu(sub);
        } else {
            const QString id = e.id;
            connect(a, &QAction::triggered, this, [this, id] {
                trigger(id);
            });
        }
        return a;
    };
    fill(nullptr, &m_actions, m_holder, entries, make);
}

QList<QAction *> DesktopMenu::contextualActions()
{
    build();
    return m_actions;
}

static void startJob(KJob *job)
{
    job->setUiDelegate(new KNotificationJobUiDelegate(KJobUiDelegate::AutoErrorHandlingEnabled));
    job->start();
}

void DesktopMenu::trigger(const QString &id)
{
    const auto folder = RayeMenu::FolderSettings::fromMap(folderValues());
    const QVariantMap changes = RayeMenu::changesFor(id, folder);
    if (!changes.isEmpty()) {
        applyFolderValues(changes);
        return;
    }

    const QString page = RayeMenu::settingsPageFor(id);
    if (!page.isEmpty()) {
        // no shell: the page goes to raye-settings as one argument
        auto *job = new KIO::CommandLauncherJob(u"raye-settings"_s, {u"--page"_s, page});
        job->setDesktopName(u"org.raye.settings"_s);
        startJob(job);
        return;
    }

    if (id == u"refresh"_s) {
        // every view of the desktop folder reads it again
        OrgKdeKDirNotifyInterface::emitFilesAdded(folderUrl());
    } else if (id == u"terminal"_s) {
        auto *job = new KTerminalLauncherJob(QString());
        job->setWorkingDirectory(localFolder());
        startJob(job);
    } else if (id == u"task-manager"_s) {
        // Raye's own first, the KDE monitor only as a fallback:
        // raye-taskmanager is a package of its own and may be absent,
        // and a menu item that does nothing is worse than one that is
        // not shown.
        for (const QString &name : {u"org.raye.taskmanager"_s, u"org.kde.plasma-systemmonitor"_s}) {
            const KService::Ptr service = KService::serviceByDesktopName(name);
            if (service) {
                startJob(new KIO::ApplicationLauncherJob(service));
                break;
            }
        }
    }
}

void DesktopMenu::stepIconSize(int by)
{
    // one gate, not two: applyFolderValues refuses a containment that has
    // no such setting (a panel, the plain desktop), so a second check here
    // would be a branch no test could ever turn red
    const auto folder = RayeMenu::FolderSettings::fromMap(folderValues());
    const int next = std::clamp(folder.iconSize + by, RayeMenu::SmallestIcons, RayeMenu::BiggestIcons);
    if (next != folder.iconSize)
        // it stops at the ends rather than wrapping round: a wheel that
        // jumped from the biggest icons back to the smallest would look
        // like the desktop had been wiped
        applyFolderValues({{u"iconSize"_s, next}});
}

// Ctrl + wheel UP. See the note in desktopmenu.h before swapping these.
void DesktopMenu::performPreviousAction()
{
    stepIconSize(1);
}

// Ctrl + wheel DOWN.
void DesktopMenu::performNextAction()
{
    stepIconSize(-1);
}

#include "desktopmenu.moc"
