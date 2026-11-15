#include "runhistory.h"

namespace
{
const char s_key[] = "Entries";
}

RunHistory::RunHistory(const KConfigGroup &group)
    : m_group(group)
{
}

QStringList RunHistory::entries() const
{
    QStringList list = m_group.readEntry(s_key, QStringList());
    list.removeAll(QString());
    return list.mid(0, MaxEntries);
}

void RunHistory::add(const QString &line)
{
    const QStringList list = added(entries(), line);
    m_group.writeEntry(s_key, list);
    m_group.sync();
}

QStringList RunHistory::added(const QStringList &entries, const QString &line)
{
    const QString entry = line.trimmed();
    QStringList list = entries;
    if (entry.isEmpty())
        return list.mid(0, MaxEntries);
    list.removeAll(entry);
    list.prepend(entry);
    return list.mid(0, MaxEntries);
}
