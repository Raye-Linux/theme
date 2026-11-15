#include "emojidict.h"

#include <QDir>
#include <QFile>
#include <QTest>
#include <QtEndian>

namespace
{
using Raw = EmojiDict::RawEntry;

void putU32(QByteArray &out, quint32 v)
{
    char b[4];
    qToLittleEndian<quint32>(v, b);
    out.append(b, 4);
}

void putString(QByteArray &out, const QString &s)
{
    const QByteArray utf8 = s.toUtf8();
    putU32(out, quint32(utf8.size()));
    out.append(utf8);
}

// The file format of Plasma's emoji dictionaries.
QByteArray dictFile(const QList<Raw> &entries)
{
    QByteArray data;
    putU32(data, quint32(entries.size()));
    for (const Raw &e : entries) {
        putString(data, e.text);
        putString(data, e.name);
        putU32(data, quint32(e.group));
        putU32(data, quint32(e.keywords.size()));
        for (const QString &k : e.keywords)
            putString(data, k);
    }
    return qCompress(data);
}

QList<Raw> english()
{
    return {
        {QStringLiteral("😀"), QStringLiteral("grinning face"), 1, {QStringLiteral("face"), QStringLiteral("grin"), QStringLiteral("happy")}},
        {QStringLiteral("😢"), QStringLiteral("crying face"), 1, {QStringLiteral("sad"), QStringLiteral("tear")}},
        {QStringLiteral("👋"), QStringLiteral("waving hand"), 2, {QStringLiteral("hello"), QStringLiteral("wave")}},
        {QStringLiteral("👋🏻"), QStringLiteral("waving hand: light skin tone"), 2, {QStringLiteral("hello")}},
        {QStringLiteral("👋🏿"), QStringLiteral("waving hand: dark skin tone"), 2, {QStringLiteral("hello")}},
        {QStringLiteral("☝️"), QStringLiteral("index pointing up"), 2, {QStringLiteral("point")}},
        {QStringLiteral("☝🏽"), QStringLiteral("index pointing up: medium skin tone"), 2, {QStringLiteral("point")}},
        {QStringLiteral("🧑‍🤝‍🧑"), QStringLiteral("people holding hands"), 2, {QStringLiteral("hold")}},
        {QStringLiteral("🧑🏻‍🤝‍🧑🏻"), QStringLiteral("people holding hands: light skin tone"), 2, {QStringLiteral("hold")}},
        {QStringLiteral("🧑🏻‍🤝‍🧑🏿"), QStringLiteral("people holding hands: light skin tone, dark skin tone"), 2, {QStringLiteral("hold")}},
        {QStringLiteral("💖"), QStringLiteral("sparkling heart"), 9, {QStringLiteral("love")}},
        {QStringLiteral("🍕"), QStringLiteral("pizza"), 5, {QStringLiteral("cheese"), QStringLiteral("slice")}},
        {QStringLiteral("❤️"), QStringLiteral("red heart"), 9, {QStringLiteral("heart"), QStringLiteral("love")}},
        {QStringLiteral("🏴󠁧󠁢󠁳󠁣󠁴󠁿"), QStringLiteral("flag: Scotland"), 10, {QStringLiteral("flag")}},
    };
}

QList<Raw> french()
{
    return {
        {QStringLiteral("😀"), QStringLiteral("visage rieur"), 1, {QStringLiteral("sourire"), QStringLiteral("heureux")}},
        {QStringLiteral("😢"), QStringLiteral("visage qui pleure"), 1, {QStringLiteral("triste"), QStringLiteral("larme")}},
        {QStringLiteral("💖"), QStringLiteral("cœur étincelant"), 9, {QStringLiteral("amour")}},
        {QStringLiteral("❤️"), QStringLiteral("cœur rouge"), 9, {QStringLiteral("amour"), QStringLiteral("cœur")}},
        {QStringLiteral("🏴󠁧󠁢󠁳󠁣󠁴󠁿"), QString::fromUtf8("drapeau : Écosse"), 10, {QStringLiteral("drapeau")}},
    };
}

QList<Raw> canadianFrench()
{
    return {{QStringLiteral("😀"), QStringLiteral("visage avec large sourire"), 1, {}}};
}
} // namespace

class TestEmojiDict : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parseRoundTrip()
    {
        QList<Raw> parsed;
        QVERIFY(EmojiDict::parse(dictFile(english()), &parsed));
        QCOMPARE(parsed.size(), english().size());
        QCOMPARE(parsed.at(0).text, QStringLiteral("😀"));
        QCOMPARE(parsed.at(0).name, QStringLiteral("grinning face"));
        QCOMPARE(parsed.at(0).group, 1);
        QCOMPARE(parsed.at(0).keywords, (QStringList{QStringLiteral("face"), QStringLiteral("grin"), QStringLiteral("happy")}));
    }

    void damagedFile()
    {
        QList<Raw> parsed;
        QVERIFY(!EmojiDict::parse(QByteArray(), &parsed));
        QVERIFY(!EmojiDict::parse(QByteArrayLiteral("not a dictionary"), &parsed));
        // cut in the middle of an entry
        QByteArray data = qUncompress(dictFile(english()));
        data.chop(5);
        QVERIFY(!EmojiDict::parse(qCompress(data), &parsed));
        // a length that points past the end
        QByteArray bad;
        putU32(bad, 1);
        putU32(bad, 0x7fffffff);
        QVERIFY(!EmojiDict::parse(qCompress(bad), &parsed));
        // trailing garbage
        data = qUncompress(dictFile(english()));
        data.append('x');
        QVERIFY(!EmojiDict::parse(qCompress(data), &parsed));
    }

    void localeChain()
    {
        QCOMPARE(EmojiDict::localeChain(QStringLiteral("fr-CA")), (QStringList{QStringLiteral("fr"), QStringLiteral("fr_CA")}));
        QCOMPARE(EmojiDict::localeChain(QStringLiteral("fr")), QStringList{QStringLiteral("fr")});
        QCOMPARE(EmojiDict::localeChain(QStringLiteral("en-US")), QStringList{QStringLiteral("en_US")});
        QCOMPARE(EmojiDict::localeChain(QStringLiteral("en")), QStringList());
    }

    void tonesFolded()
    {
        EmojiDict dict;
        dict.load(english(), {});
        // only base emoji are listed
        for (const Emoji &e : dict.all())
            QVERIFY2(!e.text.contains(QString::fromUcs4(U"\U0001F3FB")) && !e.text.contains(QString::fromUcs4(U"\U0001F3FF")), qPrintable(e.text));
        const Emoji &wave = dict.all().at(dict.indexOf(QStringLiteral("👋")));
        QCOMPARE(EmojiDict::withTone(wave, 0), QStringLiteral("👋"));
        QCOMPARE(EmojiDict::withTone(wave, 1), QStringLiteral("👋🏻"));
        QCOMPARE(EmojiDict::withTone(wave, 5), QStringLiteral("👋🏿"));
        // a tone the file does not have shows the base emoji
        QCOMPARE(EmojiDict::withTone(wave, 3), QStringLiteral("👋"));
        // variation selector in the base, none in the variant
        const Emoji &point = dict.all().at(dict.indexOf(QStringLiteral("☝️")));
        QCOMPARE(EmojiDict::withTone(point, 3), QStringLiteral("☝🏽"));
        // two people of the same tone are a variant, mixed tones are skipped
        const Emoji &hold = dict.all().at(dict.indexOf(QStringLiteral("🧑‍🤝‍🧑")));
        QCOMPARE(EmojiDict::withTone(hold, 1), QStringLiteral("🧑🏻‍🤝‍🧑🏻"));
        QCOMPARE(EmojiDict::withTone(hold, 5), QStringLiteral("🧑‍🤝‍🧑"));
        // no tones at all
        const Emoji &pizza = dict.all().at(dict.indexOf(QStringLiteral("🍕")));
        QVERIFY(pizza.tones.isEmpty());
        QCOMPARE(EmojiDict::withTone(pizza, 2), QStringLiteral("🍕"));
        QCOMPARE(dict.indexOf(QStringLiteral("👋🏿")), dict.indexOf(QStringLiteral("👋")));
    }

    void groups()
    {
        EmojiDict dict;
        dict.load(english(), {});
        QCOMPARE(dict.group(EmojiDict::Smileys).size(), 2);
        QCOMPARE(dict.group(EmojiDict::People).size(), 3);
        QCOMPARE(dict.group(EmojiDict::Flags).size(), 1);
    }

    void searchEnglish()
    {
        EmojiDict dict;
        dict.load(english(), {});
        auto first = [&dict](const QString &q) {
            const QList<int> r = dict.search(q);
            return r.isEmpty() ? QString() : dict.all().at(r.first()).text;
        };
        QCOMPARE(first(QStringLiteral("grin")), QStringLiteral("😀"));
        QCOMPARE(first(QStringLiteral("  HAPPY ")), QStringLiteral("😀"));
        QCOMPARE(first(QStringLiteral("pizza")), QStringLiteral("🍕"));
        QCOMPARE(first(QStringLiteral("red heart")), QStringLiteral("❤️"));
        QCOMPARE(first(QStringLiteral("heart red")), QStringLiteral("❤️"));
        QCOMPARE(first(QStringLiteral("scot")), QStringLiteral("🏴󠁧󠁢󠁳󠁣󠁴󠁿"));
        QVERIFY(dict.search(QStringLiteral("red pizza")).isEmpty());
        QVERIFY(dict.search(QStringLiteral("")).isEmpty());
        QCOMPARE(dict.search(QStringLiteral("face")).size(), 2);
        // the same kind of match: the shorter name first
        QCOMPARE(first(QStringLiteral("heart")), QStringLiteral("❤️"));
        // a name match beats a keyword match: "tear" is a keyword of the
        // crying face, the start of no name
        QCOMPARE(first(QStringLiteral("cry")), QStringLiteral("😢"));
        QCOMPARE(first(QStringLiteral("sparkl")), QStringLiteral("💖"));
    }

    void searchFrench()
    {
        EmojiDict dict;
        dict.load(english(), {french(), canadianFrench()});
        const Emoji &grin = dict.all().at(dict.indexOf(QStringLiteral("😀")));
        // the more specific file wins
        QCOMPARE(grin.name, QStringLiteral("visage avec large sourire"));
        QCOMPARE(grin.nameEn, QStringLiteral("grinning face"));
        // keywords of fr stay when fr_CA has none
        QVERIFY(grin.keywords.contains(QStringLiteral("heureux")));
        auto first = [&dict](const QString &q) {
            const QList<int> r = dict.search(q);
            return r.isEmpty() ? QString() : dict.all().at(r.first()).text;
        };
        QCOMPARE(first(QStringLiteral("triste")), QStringLiteral("😢"));
        QCOMPARE(first(QStringLiteral("coeur")), QStringLiteral("❤️")); // œ written oe, shorter name first
        QCOMPARE(dict.search(QStringLiteral("coeur")).size(), 2);
        QCOMPARE(first(QStringLiteral("cœur rouge")), QStringLiteral("❤️"));
        QCOMPARE(first(QStringLiteral("ecosse")), QStringLiteral("🏴󠁧󠁢󠁳󠁣󠁴󠁿")); // É without accent
        // English still works in French, after French matches
        QCOMPARE(first(QStringLiteral("pizza")), QStringLiteral("🍕"));
        QCOMPARE(first(QStringLiteral("sad")), QStringLiteral("😢"));
        // untranslated emoji keep their English name
        QCOMPARE(dict.all().at(dict.indexOf(QStringLiteral("🍕"))).name, QStringLiteral("pizza"));
    }

    void fold()
    {
        QCOMPARE(EmojiDict::fold(QStringLiteral("  Éléphant ")), QStringLiteral("elephant"));
        QCOMPARE(EmojiDict::fold(QStringLiteral("l’Œil")), QStringLiteral("l'oeil"));
    }

    // The dictionaries of the system, when installed.
    void installedFiles()
    {
        const QString dir = QStringLiteral("/usr/share/plasma/emoji");
        if (!QFile::exists(dir + QStringLiteral("/en.dict")))
            QSKIP("Plasma's emoji dictionaries are not installed");
        EmojiDict dict;
        QVERIFY(dict.load({dir}, EmojiDict::localeChain(QStringLiteral("fr-FR"))));
        QVERIFY(dict.all().size() > 1000);
        const Emoji &grin = dict.all().at(dict.indexOf(QStringLiteral("😀")));
        QCOMPARE(grin.nameEn, QStringLiteral("grinning face"));
        QCOMPARE(grin.name, QStringLiteral("visage rieur"));
        QVERIFY(!dict.search(QStringLiteral("chat")).isEmpty());
        QVERIFY(!dict.search(QStringLiteral("cat")).isEmpty());
        const Emoji &wave = dict.all().at(dict.indexOf(QStringLiteral("👋")));
        QCOMPARE(EmojiDict::withTone(wave, 1), QStringLiteral("👋🏻"));
    }
};

QTEST_GUILESS_MAIN(TestEmojiDict)
#include "tst_emojidict.moc"
