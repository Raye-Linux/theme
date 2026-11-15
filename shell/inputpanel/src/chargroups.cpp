#include "chargroups.h"

#include <QRegularExpression>

QList<CharGroup> parseCharGroups(const QString &text, CharSplit split)
{
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    QList<CharGroup> groups;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']')) && line.size() > 2) {
            groups.append({line.mid(1, line.size() - 2).trimmed(), {}});
            continue;
        }
        if (groups.isEmpty())
            continue; // items before the first group have nowhere to go
        QStringList &items = groups.last().items;
        const QStringList found = split == CharSplit::Lines ? QStringList{line} : line.split(spaces, Qt::SkipEmptyParts);
        for (const QString &item : found) {
            if (!items.contains(item))
                items.append(item);
        }
    }
    groups.removeIf([](const CharGroup &g) {
        return g.items.isEmpty();
    });
    return groups;
}
