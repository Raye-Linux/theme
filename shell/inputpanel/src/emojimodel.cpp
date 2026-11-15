#include "emojimodel.h"

EmojiModel::EmojiModel(const EmojiDict *dict, QObject *parent)
    : QAbstractListModel(parent)
    , m_dict(dict)
{
    rebuild();
}

int EmojiModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant EmojiModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Emoji &emoji = m_dict->all().at(m_rows.at(index.row()));
    switch (role) {
    case EmojiRole:
    case Qt::DisplayRole:
        return EmojiDict::withTone(emoji, m_tone);
    case BaseRole:
        return emoji.text;
    case NameRole:
        return emoji.name;
    default:
        return {};
    }
}

QHash<int, QByteArray> EmojiModel::roleNames() const
{
    return {{EmojiRole, "emoji"}, {BaseRole, "base"}, {NameRole, "name"}};
}

void EmojiModel::setGroup(int group)
{
    if (m_group == group)
        return;
    m_group = group;
    Q_EMIT groupChanged();
    rebuild();
}

void EmojiModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    Q_EMIT queryChanged();
    rebuild();
}

void EmojiModel::setTone(int tone)
{
    tone = qBound(0, tone, 5);
    if (m_tone == tone)
        return;
    m_tone = tone;
    Q_EMIT toneChanged();
    if (!m_rows.isEmpty())
        Q_EMIT dataChanged(index(0), index(int(m_rows.size()) - 1), {EmojiRole, Qt::DisplayRole});
}

void EmojiModel::setRecent(const QStringList &recent)
{
    m_recent = recent;
    if (m_group == Recent && m_query.trimmed().isEmpty())
        rebuild();
}

void EmojiModel::reload()
{
    rebuild();
}

void EmojiModel::rebuild()
{
    beginResetModel();
    if (!m_query.trimmed().isEmpty()) {
        m_rows = m_dict->search(m_query);
    } else if (m_group == Recent) {
        m_rows.clear();
        for (const QString &text : std::as_const(m_recent)) {
            const int i = m_dict->indexOf(text);
            if (i >= 0 && !m_rows.contains(i))
                m_rows.append(i);
        }
    } else {
        m_rows = m_dict->group(m_group);
    }
    endResetModel();
    Q_EMIT countChanged();
}
