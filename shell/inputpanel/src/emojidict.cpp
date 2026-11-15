#include "emojidict.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QtEndian>

#include <algorithm>

namespace
{

class Reader
{
public:
    explicit Reader(const QByteArray &data)
        : m_data(data)
    {
    }

    bool u32(quint32 *value)
    {
        if (m_pos + 4 > m_data.size())
            return false;
        *value = qFromLittleEndian<quint32>(m_data.constData() + m_pos);
        m_pos += 4;
        return true;
    }

    bool string(QString *value)
    {
        quint32 size = 0;
        if (!u32(&size) || size > quint32(m_data.size() - m_pos))
            return false;
        *value = QString::fromUtf8(m_data.constData() + m_pos, size);
        m_pos += size;
        return true;
    }

    bool atEnd() const
    {
        return m_pos == m_data.size();
    }

private:
    const QByteArray m_data;
    qsizetype m_pos = 0;
};

constexpr char32_t s_firstTone = 0x1F3FB;
constexpr char32_t s_lastTone = 0x1F3FF;

// The skin tone of a sequence (1..5), 0 for none, -1 when it mixes tones.
int toneOf(const QString &text)
{
    int tone = 0;
    for (const char32_t c : text.toUcs4()) {
        if (c < s_firstTone || c > s_lastTone)
            continue;
        const int t = int(c - s_firstTone) + 1;
        if (tone != 0 && tone != t)
            return -1;
        tone = t;
    }
    return tone;
}

// The sequence without its skin tones and variation selectors.
QString baseKey(const QString &text)
{
    QList<char32_t> kept;
    for (const char32_t c : text.toUcs4()) {
        if ((c >= s_firstTone && c <= s_lastTone) || c == 0xFE0F)
            continue;
        kept.append(c);
    }
    return QString::fromUcs4(kept.constData(), kept.size());
}

QStringList foldAll(const QStringList &list)
{
    QStringList out;
    out.reserve(list.size());
    for (const QString &s : list)
        out.append(EmojiDict::fold(s));
    return out;
}

// How well one word matches a name or keyword list; lower is better,
// -1 when it does not match.
int wordScore(const QString &word, const QString &name, const QStringList &keywords)
{
    static const QRegularExpression separators(QStringLiteral("[\\s:,\\-]+"));
    if (name.startsWith(word))
        return 0;
    const QStringList nameWords = name.split(separators, Qt::SkipEmptyParts);
    for (const QString &w : nameWords) {
        if (w.startsWith(word))
            return 1;
    }
    for (const QString &k : keywords) {
        if (k == word)
            return 2;
    }
    for (const QString &k : keywords) {
        const QStringList parts = k.split(separators, Qt::SkipEmptyParts);
        for (const QString &p : parts) {
            if (p.startsWith(word))
                return 3;
        }
    }
    return -1;
}

} // namespace

QString EmojiDict::fold(const QString &text)
{
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing)
            continue;
        out.append(c.toLower());
    }
    // ligatures typed as two letters, typographic apostrophes, and the
    // narrow spaces of French names
    out.replace(QChar(0x0153), QLatin1String("oe"));
    out.replace(QChar(0x00E6), QLatin1String("ae"));
    out.replace(QChar(0x00DF), QLatin1String("ss"));
    out.replace(QChar(0x2019), QLatin1Char('\''));
    out.replace(QChar(0x202F), QLatin1Char(' '));
    out.replace(QChar(0x00A0), QLatin1Char(' '));
    return out.trimmed();
}

bool EmojiDict::parse(const QByteArray &file, QList<RawEntry> *entries)
{
    if (file.size() < 4)
        return false;
    const QByteArray data = qUncompress(file);
    if (data.isEmpty())
        return false;
    Reader reader(data);
    quint32 count = 0;
    if (!reader.u32(&count))
        return false;
    QList<RawEntry> out;
    out.reserve(qMin<quint32>(count, 10000));
    for (quint32 i = 0; i < count; ++i) {
        RawEntry entry;
        quint32 group = 0, keywordCount = 0;
        if (!reader.string(&entry.text) || !reader.string(&entry.name) || !reader.u32(&group) || !reader.u32(&keywordCount))
            return false;
        if (keywordCount > 1000)
            return false;
        entry.group = int(group);
        for (quint32 k = 0; k < keywordCount; ++k) {
            QString keyword;
            if (!reader.string(&keyword))
                return false;
            entry.keywords.append(keyword);
        }
        out.append(entry);
    }
    if (!reader.atEnd())
        return false;
    *entries = out;
    return true;
}

QStringList EmojiDict::localeChain(const QString &uiLanguage)
{
    QString name = uiLanguage;
    name.replace(QLatin1Char('-'), QLatin1Char('_'));
    const QStringList parts = name.split(QLatin1Char('_'), Qt::SkipEmptyParts);
    QStringList chain;
    QString current;
    for (const QString &part : parts) {
        current = current.isEmpty() ? part : current + QLatin1Char('_') + part;
        if (current != QLatin1String("en"))
            chain.append(current);
    }
    return chain;
}

bool EmojiDict::load(const QStringList &dirs, const QStringList &locales)
{
    auto read = [&dirs](const QString &locale, QList<RawEntry> *entries) {
        for (const QString &dir : dirs) {
            QFile file(dir + QLatin1Char('/') + locale + QStringLiteral(".dict"));
            if (file.open(QIODevice::ReadOnly))
                return parse(file.readAll(), entries);
        }
        return false;
    };
    QList<RawEntry> english;
    if (!read(QStringLiteral("en"), &english))
        return false;
    QList<QList<RawEntry>> translations;
    for (const QString &locale : locales) {
        QList<RawEntry> entries;
        if (read(locale, &entries))
            translations.append(entries);
    }
    load(english, translations);
    return true;
}

void EmojiDict::load(const QList<RawEntry> &english, const QList<QList<RawEntry>> &translations)
{
    m_emoji.clear();
    QHash<QString, int> byText;
    QHash<QString, int> byBase;
    QList<QPair<QString, int>> variants; // (text, english index)

    for (int i = 0; i < english.size(); ++i) {
        const RawEntry &raw = english.at(i);
        const int tone = toneOf(raw.text);
        if (tone != 0) {
            if (tone > 0)
                variants.append({raw.text, i});
            continue;
        }
        Emoji emoji;
        emoji.text = raw.text;
        emoji.name = raw.name;
        emoji.nameEn = raw.name;
        emoji.keywords = raw.keywords;
        emoji.keywordsEn = raw.keywords;
        emoji.group = raw.group;
        byText.insert(raw.text, m_emoji.size());
        byBase.insert(baseKey(raw.text), m_emoji.size());
        m_emoji.append(emoji);
    }

    for (const auto &[text, index] : std::as_const(variants)) {
        const auto base = byBase.constFind(baseKey(text));
        if (base == byBase.cend())
            continue;
        Emoji &emoji = m_emoji[*base];
        if (emoji.tones.isEmpty())
            emoji.tones = QStringList(6);
        emoji.tones[toneOf(text)] = text;
    }
    // A base with only some tones shows itself for the missing ones.
    for (Emoji &emoji : m_emoji) {
        for (int t = 1; t < emoji.tones.size(); ++t) {
            if (emoji.tones.at(t).isEmpty())
                emoji.tones[t] = emoji.text;
        }
    }

    for (const QList<RawEntry> &translation : translations) {
        for (const RawEntry &raw : translation) {
            const auto it = byText.constFind(raw.text);
            if (it == byText.cend())
                continue;
            Emoji &emoji = m_emoji[*it];
            if (!raw.name.isEmpty())
                emoji.name = raw.name;
            if (!raw.keywords.isEmpty())
                emoji.keywords = raw.keywords;
        }
    }

    for (Emoji &emoji : m_emoji) {
        emoji.foldedName = fold(emoji.name);
        emoji.foldedNameEn = fold(emoji.nameEn);
        emoji.foldedKeywords = foldAll(emoji.keywords);
        emoji.foldedKeywordsEn = foldAll(emoji.keywordsEn);
    }
}

QList<int> EmojiDict::group(int group) const
{
    QList<int> out;
    for (int i = 0; i < m_emoji.size(); ++i) {
        if (m_emoji.at(i).group == group)
            out.append(i);
    }
    return out;
}

int EmojiDict::indexOf(const QString &text) const
{
    for (int i = 0; i < m_emoji.size(); ++i) {
        const Emoji &e = m_emoji.at(i);
        if (e.text == text || e.tones.contains(text))
            return i;
    }
    return -1;
}

QList<int> EmojiDict::search(const QString &query, int limit) const
{
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    const QStringList words = fold(query).split(spaces, Qt::SkipEmptyParts);
    if (words.isEmpty())
        return {};

    QList<std::pair<int, int>> scored; // (score, index)
    for (int i = 0; i < m_emoji.size(); ++i) {
        const Emoji &e = m_emoji.at(i);
        int total = 0;
        bool all = true;
        for (const QString &word : words) {
            int best = wordScore(word, e.foldedName, e.foldedKeywords);
            const int english = wordScore(word, e.foldedNameEn, e.foldedKeywordsEn);
            // the user's language first, English as a fallback
            if (english >= 0 && (best < 0 || english + 1 < best))
                best = english + 1;
            if (best < 0) {
                all = false;
                break;
            }
            total += best;
        }
        if (all)
            scored.append({total, i});
    }
    // same score: the shorter name is the closer match ("red heart"
    // before "heart with arrow")
    std::stable_sort(scored.begin(), scored.end(), [this](const auto &a, const auto &b) {
        if (a.first != b.first)
            return a.first < b.first;
        return m_emoji.at(a.second).foldedName.size() < m_emoji.at(b.second).foldedName.size();
    });
    QList<int> out;
    for (int i = 0; i < scored.size() && i < limit; ++i)
        out.append(scored.at(i).second);
    return out;
}

QString EmojiDict::withTone(const Emoji &emoji, int tone)
{
    if (tone <= 0 || tone >= emoji.tones.size())
        return emoji.text;
    return emoji.tones.at(tone);
}
