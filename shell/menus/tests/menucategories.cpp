// Prints where every app of the menu lands, one line per entry:
//   category <menu path> <caption>     in the order Start shows them
//   app <desktop id> <menu path>       sorted by desktop id
// Apps hidden from menus (NoDisplay, other desktops) are not listed.

#include <KService>
#include <KServiceGroup>

#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

static void walk(const KServiceGroup::Ptr &group, QStringList &categories, QStringList &apps)
{
    const KServiceGroup::List entries = group->entries(true /* sorted */, true /* no NoDisplay */);
    for (const KSycocaEntry::Ptr &entry : entries) {
        if (entry->isType(KST_KServiceGroup)) {
            const KServiceGroup::Ptr sub(static_cast<KServiceGroup *>(entry.data()));
            if (sub->noDisplay() || sub->childCount() == 0)
                continue;
            categories << QStringLiteral("category %1 %2").arg(sub->relPath(), sub->caption());
            walk(sub, categories, apps);
        } else if (entry->isType(KST_KService)) {
            const KService::Ptr service(static_cast<KService *>(entry.data()));
            if (service->noDisplay())
                continue;
            apps << QStringLiteral("app %1 %2").arg(service->menuId(),
                                                    group->relPath().isEmpty() ? QStringLiteral("/") : group->relPath());
        }
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    KServiceGroup::Ptr root = KServiceGroup::root();
    if (!root || !root->isValid()) {
        QTextStream(stderr) << "no application menu found\n";
        return 2;
    }
    QStringList categories, apps;
    walk(root, categories, apps);
    apps.sort();
    QTextStream out(stdout);
    for (const QString &line : std::as_const(categories))
        out << line << '\n';
    for (const QString &line : std::as_const(apps))
        out << line << '\n';
    return 0;
}
