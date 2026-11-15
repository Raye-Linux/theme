// The emoji names Plasma ships (/usr/share/plasma/emoji/<locale>.dict,
// the data of the stock emoji selector): English first, then the user's
// language on top, so a search finds a face by its French or English
// name. Skin tones are folded into their base emoji.
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct Emoji {
    QString text;
    QString name; // in the user's language, English when untranslated
    QString nameEn;
    QStringList keywords; // user's language
    QStringList keywordsEn;
    int group = 0; // Unicode group, see EmojiDict::Group
    // Skin tone variants (index 1..5: light to dark), empty when none.
    QStringList tones;

    // the same, folded for matching (see EmojiDict::fold)
    QString foldedName;
    QString foldedNameEn;
    QStringList foldedKeywords;
    QStringList foldedKeywordsEn;
};

class EmojiDict
{
public:
    // The groups of the dictionary files (Unicode's order, without the
    // skin tone components).
    enum Group {
        Smileys = 1,
        People = 2,
        Animals = 4,
        Food = 5,
        Travel = 6,
        Activities = 7,
        Objects = 8,
        Symbols = 9,
        Flags = 10,
    };

    struct RawEntry {
        QString text;
        QString name;
        int group = 0;
        QStringList keywords;
    };

    // One .dict file: qCompress'ed, little-endian count, then per entry
    // the emoji, its name, its group and its keywords (UTF-8 strings with
    // a 32-bit length). Returns false on a damaged file.
    static bool parse(const QByteArray &file, QList<RawEntry> *entries);

    // Reads en.dict, then each locale in order (less to more specific:
    // "fr", "fr_CA") from the first folder that has it. Returns false
    // when en.dict is missing or damaged.
    bool load(const QStringList &dirs, const QStringList &locales);
    // Same, from data already read (tests).
    void load(const QList<RawEntry> &english, const QList<QList<RawEntry>> &translations);

    // The files to read for a UI language ("fr-CA" -> fr, fr_CA).
    static QStringList localeChain(const QString &uiLanguage);

    const QList<Emoji> &all() const
    {
        return m_emoji;
    }
    // Indexes of the emoji of one group, in file order.
    QList<int> group(int group) const;
    // Indexes of the best matches for a query (every word must match the
    // start of a word of the name or of a keyword, in either language;
    // case and accents ignored), best first.
    QList<int> search(const QString &query, int limit = 200) const;
    int indexOf(const QString &text) const;

    // The emoji as shown with a skin tone (0 = none, 1..5).
    static QString withTone(const Emoji &emoji, int tone);

    // Lower case, accents removed, for matching.
    static QString fold(const QString &text);

private:
    QList<Emoji> m_emoji;
};
