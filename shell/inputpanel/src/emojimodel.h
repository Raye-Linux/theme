// The emoji grid: one group, the recent ones, or the results of a search,
// drawn with the chosen skin tone.
#pragma once

#include "emojidict.h"

#include <QAbstractListModel>

class EmojiModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int group READ group WRITE setGroup NOTIFY groupChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int tone READ tone WRITE setTone NOTIFY toneChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    // group() value that lists the recent emoji
    static constexpr int Recent = 0;

    enum Roles {
        EmojiRole = Qt::UserRole + 1, // with the skin tone
        BaseRole, // without it
        NameRole,
    };

    explicit EmojiModel(const EmojiDict *dict, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int group() const
    {
        return m_group;
    }
    void setGroup(int group);
    QString query() const
    {
        return m_query;
    }
    void setQuery(const QString &query);
    int tone() const
    {
        return m_tone;
    }
    void setTone(int tone);
    int count() const
    {
        return int(m_rows.size());
    }

    void setRecent(const QStringList &recent);
    // After the dictionary was (re)loaded.
    void reload();

Q_SIGNALS:
    void groupChanged();
    void queryChanged();
    void toneChanged();
    void countChanged();

private:
    void rebuild();

    const EmojiDict *m_dict;
    QList<int> m_rows;
    QStringList m_recent;
    int m_group = EmojiDict::Smileys;
    QString m_query;
    int m_tone = 0;
};
