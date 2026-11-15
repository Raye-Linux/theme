// The kaomoji and symbol lists of the panel (data/*.txt, compiled in).
//   # a comment
//   [group id]
//   one item per line (kaomoji), or items separated by spaces (symbols)
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct CharGroup {
    QString id;
    QStringList items;
};

enum class CharSplit {
    Lines,
    Spaces,
};

QList<CharGroup> parseCharGroups(const QString &text, CharSplit split);
