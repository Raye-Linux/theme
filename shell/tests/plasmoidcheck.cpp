// Checks run on every applet of the shell before it ships. See
// plasmoidcheck.h; running an applet is in plasmoidrun.cpp, the tests of
// the checks are in tst_plasmoidcheck.cpp.

#include "plasmoidcheck.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>

#include <algorithm>
#include <utility>

namespace RayeShellCheck
{

QString Problem::toString() const
{
    if (file.isEmpty()) {
        return message;
    }
    if (line > 0) {
        return QStringLiteral("%1:%2: %3").arg(file).arg(line).arg(message);
    }
    return QStringLiteral("%1: %2").arg(file, message);
}

QString describe(const QList<Problem> &problems)
{
    QStringList lines;
    for (const Problem &p : problems) {
        lines << p.toString();
    }
    return lines.join(QLatin1Char('\n'));
}

namespace
{

QString readText(const QString &path, bool *ok = nullptr)
{
    QFile file(path);
    const bool opened = file.open(QIODevice::ReadOnly);
    if (ok) {
        *ok = opened;
    }
    return opened ? QString::fromUtf8(file.readAll()) : QString();
}

bool isIdentChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('$');
}

// Walks JS/QML source, skipping comments and string literals.
class Lexer
{
public:
    explicit Lexer(QString source)
        : m_s(std::move(source))
    {
    }

    bool atEnd() const
    {
        return m_pos >= m_s.size();
    }
    int pos() const
    {
        return m_pos;
    }
    QChar peek(int ahead = 0) const
    {
        const int i = m_pos + ahead;
        return i < m_s.size() ? m_s.at(i) : QChar();
    }
    int lineAt(int index) const
    {
        return int(std::count(m_s.cbegin(), m_s.cbegin() + std::min<qsizetype>(index, m_s.size()), QLatin1Char('\n'))) + 1;
    }

    // Moves past a comment when one starts here.
    bool skipComment()
    {
        if (peek() != QLatin1Char('/')) {
            return false;
        }
        if (peek(1) == QLatin1Char('/')) {
            while (!atEnd() && peek() != QLatin1Char('\n')) {
                ++m_pos;
            }
            return true;
        }
        if (peek(1) == QLatin1Char('*')) {
            const int end = m_s.indexOf(QLatin1String("*/"), m_pos + 2);
            m_pos = end < 0 ? m_s.size() : end + 2;
            return true;
        }
        return false;
    }

    void skipSpace()
    {
        while (!atEnd()) {
            if (peek().isSpace()) {
                ++m_pos;
            } else if (!skipComment()) {
                return;
            }
        }
    }

    // Reads a string literal starting here. Template literals are skipped
    // but never count as plain strings.
    bool readString(QString *value, bool *plain)
    {
        const QChar quote = peek();
        if (quote != QLatin1Char('"') && quote != QLatin1Char('\'') && quote != QLatin1Char('`')) {
            return false;
        }
        ++m_pos;
        QString out;
        bool closed = false;
        while (!atEnd()) {
            const QChar c = peek();
            ++m_pos;
            if (c == quote) {
                closed = true;
                break;
            }
            if (c == QLatin1Char('\\') && !atEnd()) {
                const QChar e = peek();
                ++m_pos;
                switch (e.unicode()) {
                case 'n':
                    out += QLatin1Char('\n');
                    break;
                case 't':
                    out += QLatin1Char('\t');
                    break;
                case 'r':
                    out += QLatin1Char('\r');
                    break;
                case 'u': {
                    bool ok = false;
                    const uint code = m_s.mid(m_pos, 4).toUInt(&ok, 16);
                    if (ok) {
                        out += QChar(code);
                        m_pos += 4;
                    }
                    break;
                }
                case '\n':
                    break;
                default:
                    out += e;
                }
                continue;
            }
            if (c == QLatin1Char('\n') && quote != QLatin1Char('`')) {
                break; // unterminated
            }
            out += c;
        }
        if (value) {
            *value = out;
        }
        if (plain) {
            *plain = closed && quote != QLatin1Char('`');
        }
        return true;
    }

    // Skips one token: a comment, a string or a single character.
    void skipToken()
    {
        if (skipComment() || readString(nullptr, nullptr)) {
            return;
        }
        ++m_pos;
    }

    QString readIdentifier()
    {
        const int start = m_pos;
        while (!atEnd() && isIdentChar(peek())) {
            ++m_pos;
        }
        return m_s.mid(start, m_pos - start);
    }

    // At an opening parenthesis: the raw text of each argument, up to the
    // closing one.
    QStringList readArguments()
    {
        QStringList args;
        ++m_pos; // (
        int depth = 0;
        int start = m_pos;
        while (!atEnd()) {
            const QChar c = peek();
            if (c == QLatin1Char('/') && (peek(1) == QLatin1Char('/') || peek(1) == QLatin1Char('*'))) {
                skipComment();
                continue;
            }
            if (c == QLatin1Char('"') || c == QLatin1Char('\'') || c == QLatin1Char('`')) {
                readString(nullptr, nullptr);
                continue;
            }
            if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{')) {
                ++depth;
            } else if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) {
                if (depth == 0) {
                    const QString last = m_s.mid(start, m_pos - start);
                    if (!last.trimmed().isEmpty() || !args.isEmpty()) {
                        args << last;
                    }
                    ++m_pos;
                    return args;
                }
                --depth;
            } else if (c == QLatin1Char(',') && depth == 0) {
                args << m_s.mid(start, m_pos - start);
                start = m_pos + 1;
            }
            ++m_pos;
        }
        args << m_s.mid(start);
        return args;
    }

private:
    QString m_s;
    int m_pos = 0;
};

struct Call {
    QString name;
    int line = 0;
    QStringList args;
};

// Every call to a function whose name passes match(), not as a member
// (obj.f()) and not inside a comment or a string.
template<typename Match>
QList<Call> findCalls(const QString &source, Match match)
{
    QList<Call> calls;
    Lexer lex(source);
    while (!lex.atEnd()) {
        const QChar c = lex.peek();
        if (isIdentChar(c) && !c.isDigit()) {
            int back = lex.pos() - 1;
            while (back >= 0 && source.at(back).isSpace()) {
                --back;
            }
            const bool member = back >= 0 && source.at(back) == QLatin1Char('.');
            const int at = lex.pos();
            const QString name = lex.readIdentifier();
            if (member || !match(name)) {
                continue;
            }
            Lexer probe = lex;
            probe.skipSpace();
            if (probe.peek() != QLatin1Char('(')) {
                continue;
            }
            lex = probe;
            Call call;
            call.name = name;
            call.line = lex.lineAt(at);
            call.args = lex.readArguments();
            calls << call;
            continue;
        }
        if (c.isDigit()) {
            lex.readIdentifier(); // a number, never the start of a name
            continue;
        }
        lex.skipToken();
    }
    return calls;
}

// "a" + 'b' + "c": the joined value, when the argument is only that.
bool literalValue(const QString &arg, QString *value)
{
    Lexer lex(arg);
    QString out;
    bool any = false;
    while (true) {
        lex.skipSpace();
        QString part;
        bool plain = false;
        if (!lex.readString(&part, &plain) || !plain) {
            return false;
        }
        out += part;
        any = true;
        lex.skipSpace();
        if (lex.atEnd()) {
            break;
        }
        if (lex.peek() != QLatin1Char('+')) {
            return false;
        }
        lex.skipToken();
    }
    if (value) {
        *value = out;
    }
    return any;
}

const QRegularExpression &i18nName()
{
    static const QRegularExpression re(QStringLiteral("^x?i18n(d?)(c?)(p?)$"));
    return re;
}

// Folders that never ship with an applet.
bool isSkippedPath(const QString &relative)
{
    static const QStringList skipped = {QStringLiteral("tests"), QStringLiteral("dummydata")};
    for (const QString &part : relative.split(QLatin1Char('/'))) {
        if (skipped.contains(part)) {
            return true;
        }
    }
    return false;
}

QStringList filesUnder(const QString &dir, const QStringList &nameFilters)
{
    QStringList files;
    QDirIterator it(dir, nameFilters, QDir::Files, QDirIterator::Subdirectories);
    const QDir base(dir);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString relative = base.relativeFilePath(path);
        if (!isSkippedPath(relative)) {
            files << relative;
        }
    }
    files.sort();
    return files;
}

QString languageName(const QString &lang)
{
    if (lang == QLatin1String("fr")) {
        return QStringLiteral("French");
    }
    return lang;
}

QString placeholderList(const QList<int> &numbers)
{
    QStringList out;
    for (int n : numbers) {
        out << QLatin1Char('%') + QString::number(n);
    }
    return out.join(QStringLiteral(", "));
}

} // namespace

QList<I18nCall> scanI18nCalls(const QString &source)
{
    QList<I18nCall> result;
    const QList<Call> calls = findCalls(source, [](const QString &name) {
        return i18nName().match(name).hasMatch();
    });
    for (const Call &call : calls) {
        const QRegularExpressionMatch m = i18nName().match(call.name);
        I18nCall out;
        out.function = call.name;
        out.line = call.line;
        out.hasDomain = !m.captured(1).isEmpty();
        const bool hasContext = !m.captured(2).isEmpty();
        const bool hasPlural = !m.captured(3).isEmpty();

        int index = 0;
        auto next = [&](QString *value) {
            if (index >= call.args.size()) {
                return false;
            }
            return literalValue(call.args.at(index++), value);
        };
        if (out.hasDomain) {
            out.domainLiteral = next(&out.domain);
        }
        bool literal = true;
        if (hasContext) {
            literal = next(&out.context) && literal;
        }
        literal = next(&out.text) && literal;
        if (hasPlural) {
            literal = next(&out.plural) && literal;
        }
        out.textLiteral = literal;
        result << out;
    }
    return result;
}

QList<int> scanQtTranslateCalls(const QString &source)
{
    static const QStringList names = {QStringLiteral("qsTr"),
                                      QStringLiteral("qsTranslate"),
                                      QStringLiteral("qsTrId"),
                                      QStringLiteral("QT_TR_NOOP"),
                                      QStringLiteral("QT_TRANSLATE_NOOP")};
    QList<int> lines;
    for (const Call &call : findCalls(source, [](const QString &name) {
             return names.contains(name);
         })) {
        lines << call.line;
    }
    return lines;
}

QString poKey(const QString &context, const QString &msgid)
{
    return context.isEmpty() ? msgid : context + QChar(0x04) + msgid;
}

QHash<QString, PoEntry> parsePo(const QString &text, QString *error)
{
    QHash<QString, PoEntry> entries;
    if (error) {
        error->clear();
    }
    auto fail = [&](int line, const QString &what) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("line %1: %2").arg(line).arg(what);
        }
    };

    PoEntry entry;
    bool haveId = false;
    bool haveStr = false;
    bool pendingFuzzy = false;
    QString *last = nullptr;

    auto flush = [&]() {
        if (haveId && !(entry.msgid.isEmpty() && entry.context.isEmpty())) {
            entries.insert(poKey(entry.context, entry.msgid), entry);
        }
        entry = PoEntry();
        haveId = haveStr = false;
        last = nullptr;
    };

    // "..." on its own: the value between the quotes, unescaped.
    auto quoted = [&](const QString &raw, int line, QString *out) {
        const QString t = raw.trimmed();
        if (t.size() < 2 || !t.startsWith(QLatin1Char('"')) || !t.endsWith(QLatin1Char('"'))) {
            fail(line, QStringLiteral("expected a string in quotes"));
            return false;
        }
        QString value;
        Lexer lex(t);
        bool plain = false;
        lex.readString(&value, &plain);
        if (!plain || !lex.atEnd()) {
            fail(line, QStringLiteral("a string is not closed"));
            return false;
        }
        *out = value;
        return true;
    };

    static const QRegularExpression keyword(QStringLiteral("^(msgctxt|msgid_plural|msgid|msgstr(?:\\[(\\d+)\\])?)\\s+(.*)$"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const int number = i + 1;
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty()) {
            flush();
            pendingFuzzy = false;
            continue;
        }
        if (line.startsWith(QLatin1Char('#'))) {
            if (haveStr) {
                flush();
            }
            if (line.startsWith(QLatin1String("#,")) && line.contains(QLatin1String("fuzzy"))) {
                pendingFuzzy = true;
            }
            continue; // #~ obsolete entries, references and comments
        }
        if (line.startsWith(QLatin1Char('"'))) {
            QString value;
            if (!last) {
                fail(number, QStringLiteral("a string that belongs to nothing"));
            } else if (quoted(line, number, &value)) {
                *last += value;
            }
            continue;
        }
        const QRegularExpressionMatch m = keyword.match(line);
        if (!m.hasMatch()) {
            fail(number, QStringLiteral("not a po line: %1").arg(line));
            continue;
        }
        const QString word = m.captured(1);
        QString value;
        if (!quoted(m.captured(3), number, &value)) {
            last = nullptr;
            continue;
        }
        if ((word == QLatin1String("msgctxt") || word == QLatin1String("msgid")) && haveStr) {
            flush();
        }
        if (word == QLatin1String("msgctxt")) {
            entry.context = value;
            last = &entry.context;
        } else if (word == QLatin1String("msgid")) {
            entry.msgid = value;
            entry.fuzzy = pendingFuzzy;
            pendingFuzzy = false;
            haveId = true;
            last = &entry.msgid;
        } else if (word == QLatin1String("msgid_plural")) {
            entry.plural = value;
            last = &entry.plural;
        } else {
            const int n = m.captured(2).isEmpty() ? 0 : m.captured(2).toInt();
            if (n > 16) {
                fail(number, QStringLiteral("too many plural forms"));
                continue;
            }
            while (entry.msgstr.size() <= n) {
                entry.msgstr << QString();
            }
            entry.msgstr[n] = value;
            haveStr = true;
            last = &entry.msgstr[n];
        }
    }
    flush();
    return entries;
}

QList<int> placeholders(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("%(\\d+)"));
    QSet<int> found;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        found.insert(it.next().captured(1).toInt());
    }
    QList<int> out(found.cbegin(), found.cend());
    std::sort(out.begin(), out.end());
    return out;
}

bool isCompiledApplet(const QString &appletDir)
{
    return QFileInfo::exists(appletDir + QStringLiteral("/CMakeLists.txt"));
}

QList<Problem> checkMetadata(const QString &appletDir)
{
    QList<Problem> problems;
    const QString folder = QFileInfo(appletDir).fileName();
    auto add = [&](const QString &message, const QString &file = QStringLiteral("metadata.json")) {
        problems << Problem{file, 0, message};
    };

    if (!folder.startsWith(QLatin1String("org.raye."))) {
        add(QStringLiteral("the folder %1 is not an org.raye.* name").arg(folder), QString());
    }

    bool ok = false;
    const QString text = readText(appletDir + QStringLiteral("/metadata.json"), &ok);
    if (!ok) {
        add(QStringLiteral("there is no metadata.json"), QString());
        return problems;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        add(QStringLiteral("not valid JSON: %1").arg(parseError.errorString()));
        return problems;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("KPackageStructure")).toString() != QLatin1String("Plasma/Applet")) {
        add(QStringLiteral("KPackageStructure must be \"Plasma/Applet\""));
    }
    const QJsonValue api = root.value(QStringLiteral("X-Plasma-API-Minimum-Version"));
    if (!api.toString().startsWith(QLatin1Char('6'))) {
        add(QStringLiteral("X-Plasma-API-Minimum-Version must be \"6.0\", or Plasma will not load the applet"));
    }

    const QJsonObject plugin = root.value(QStringLiteral("KPlugin")).toObject();
    const QString id = plugin.value(QStringLiteral("Id")).toString();
    if (isCompiledApplet(appletDir)) {
        // a compiled applet takes its id from the plugin file name
        if (plugin.contains(QStringLiteral("Id"))) {
            add(QStringLiteral("a compiled applet must not have a KPlugin Id: Plasma takes it from the plugin, name the target %1").arg(folder));
        }
    } else if (id.isEmpty()) {
        add(QStringLiteral("KPlugin has no Id"));
    } else if (id != folder) {
        add(QStringLiteral("the Id is %1 but the folder is %2: Plasma looks the applet up by its folder").arg(id, folder));
    }
    for (const QString &key : {QStringLiteral("Name"),
                               QStringLiteral("Name[fr]"),
                               QStringLiteral("Description"),
                               QStringLiteral("Description[fr]"),
                               QStringLiteral("Icon"),
                               QStringLiteral("License"),
                               QStringLiteral("Version")}) {
        if (plugin.value(key).toString().trimmed().isEmpty()) {
            add(QStringLiteral("KPlugin has no %1").arg(key));
        }
    }
    const QJsonArray authors = plugin.value(QStringLiteral("Authors")).toArray();
    if (authors.isEmpty() || authors.first().toObject().value(QStringLiteral("Name")).toString().isEmpty()) {
        add(QStringLiteral("KPlugin has no Authors"));
    }

    if (!isCompiledApplet(appletDir)) {
        QString main = root.value(QStringLiteral("X-Plasma-MainScript")).toString();
        if (main.isEmpty()) {
            main = QStringLiteral("ui/main.qml");
        }
        if (main.contains(QLatin1String("..")) || main.startsWith(QLatin1Char('/'))) {
            add(QStringLiteral("X-Plasma-MainScript must stay inside contents/"));
        } else if (!QFileInfo::exists(appletDir + QStringLiteral("/contents/") + main)) {
            add(QStringLiteral("contents/%1 is missing").arg(main), QString());
        }
    }
    return problems;
}

QList<Problem> checkTranslations(const QString &appletDir, const QString &poDir, const QString &lang)
{
    QList<Problem> problems;
    struct Po {
        bool exists = false;
        QString error;
        QHash<QString, PoEntry> entries;
    };
    QHash<QString, Po> pos;
    QSet<QString> reported; // domains whose file problem was already named

    for (const QString &file : filesUnder(appletDir, {QStringLiteral("*.qml"), QStringLiteral("*.js"), QStringLiteral("*.mjs")})) {
        const QString source = readText(appletDir + QLatin1Char('/') + file);

        for (int line : scanQtTranslateCalls(source)) {
            problems << Problem{file, line, QStringLiteral("qsTr() is not read by the shell's translations: use i18nd(\"raye_shell_<part>\", ...)")};
        }

        for (const I18nCall &call : scanI18nCalls(source)) {
            auto add = [&](const QString &message) {
                problems << Problem{file, call.line, message};
            };
            if (!call.hasDomain) {
                add(QStringLiteral("%1() has no domain: use the d form, i18nd(\"raye_shell_<part>\", ...)").arg(call.function));
                continue;
            }
            if (!call.domainLiteral) {
                add(QStringLiteral("the domain of %1() is not a plain string").arg(call.function));
                continue;
            }
            if (!call.domain.startsWith(QLatin1String("raye_shell_"))) {
                add(QStringLiteral("the domain \"%1\" is not one of the shell (raye_shell_<part>)").arg(call.domain));
                continue;
            }
            if (!call.textLiteral) {
                add(QStringLiteral("the text of %1() is not a plain string, so it can never be translated").arg(call.function));
                continue;
            }

            const QString poName = call.domain + QStringLiteral(".po");
            const QString poRelative = lang + QLatin1Char('/') + poName;
            if (!pos.contains(call.domain)) {
                Po po;
                const QString text = readText(poDir + QLatin1Char('/') + poRelative, &po.exists);
                if (po.exists) {
                    po.entries = parsePo(text, &po.error);
                }
                pos.insert(call.domain, po);
            }
            const Po &po = pos[call.domain];
            if (!po.exists) {
                add(QStringLiteral("no %1 strings: po/%2 does not exist").arg(languageName(lang), poRelative));
                continue;
            }
            if (!po.error.isEmpty() && !reported.contains(call.domain)) {
                reported.insert(call.domain);
                add(QStringLiteral("po/%1 cannot be read, %2").arg(poRelative, po.error));
            }

            const PoEntry entry = po.entries.value(poKey(call.context, call.text));
            QString shown = QLatin1Char('"') + call.text + QLatin1Char('"');
            if (!call.context.isEmpty()) {
                shown += QStringLiteral(" (") + call.context + QLatin1Char(')');
            }
            const int forms = call.plural.isEmpty() ? 1 : 2;
            bool translated = entry.msgstr.size() >= forms;
            for (const QString &form : entry.msgstr) {
                translated = translated && !form.isEmpty();
            }
            if (!translated) {
                add(QStringLiteral("%1 is not translated in po/%2").arg(shown, poRelative));
                continue;
            }
            if (entry.fuzzy) {
                add(QStringLiteral("%1 only has a fuzzy translation in po/%2").arg(shown, poRelative));
                continue;
            }

            QList<int> source = placeholders(call.text) + placeholders(call.plural);
            QList<int> target;
            for (const QString &form : entry.msgstr) {
                target += placeholders(form);
            }
            std::sort(source.begin(), source.end());
            source.erase(std::unique(source.begin(), source.end()), source.end());
            std::sort(target.begin(), target.end());
            target.erase(std::unique(target.begin(), target.end()), target.end());
            QList<int> extra;
            QList<int> dropped;
            std::set_difference(target.cbegin(), target.cend(), source.cbegin(), source.cend(), std::back_inserter(extra));
            std::set_difference(source.cbegin(), source.cend(), target.cbegin(), target.cend(), std::back_inserter(dropped));
            if (!extra.isEmpty() || !dropped.isEmpty()) {
                QStringList what;
                if (!extra.isEmpty()) {
                    what << QStringLiteral("uses %1, which the English does not have").arg(placeholderList(extra));
                }
                if (!dropped.isEmpty()) {
                    what << QStringLiteral("drops %1").arg(placeholderList(dropped));
                }
                add(QStringLiteral("%1: the %2 text %3").arg(shown, languageName(lang), what.join(QStringLiteral(" and "))));
            }
        }
    }
    return problems;
}

QList<Problem> checkConfigXml(const QString &appletDir)
{
    QList<Problem> problems;
    for (const QString &file : filesUnder(appletDir, {QStringLiteral("main.xml")})) {
        if (QFileInfo(file).dir().dirName() != QLatin1String("config")) {
            continue;
        }
        QFile xml(appletDir + QLatin1Char('/') + file);
        if (!xml.open(QIODevice::ReadOnly)) {
            problems << Problem{file, 0, QStringLiteral("cannot be read")};
            continue;
        }
        QXmlStreamReader reader(&xml);
        bool sawRoot = false;
        QStringList missing;
        int missingLine = 0;
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement()) {
                continue;
            }
            if (!sawRoot) {
                sawRoot = true;
                if (reader.name() != QLatin1String("kcfg")) {
                    reader.raiseError(QStringLiteral("the root element must be <kcfg>"));
                }
            } else if (reader.name() == QLatin1String("entry")) {
                const auto attrs = reader.attributes();
                if (attrs.value(QLatin1String("name")).isEmpty() || attrs.value(QLatin1String("type")).isEmpty()) {
                    missing << attrs.value(QLatin1String("name")).toString();
                    if (!missingLine) {
                        missingLine = int(reader.lineNumber());
                    }
                }
            }
        }
        if (reader.hasError()) {
            problems << Problem{file, int(reader.lineNumber()), QStringLiteral("not valid: %1").arg(reader.errorString())};
        } else if (!missing.isEmpty()) {
            problems << Problem{file, missingLine, QStringLiteral("every <entry> needs a name and a type")};
        }
    }
    return problems;
}

QList<Problem> compileQml(const QString &appletDir, QQmlEngine *engine)
{
    QList<Problem> problems;
    const QDir base(appletDir);
    for (const QString &file : filesUnder(appletDir + QStringLiteral("/contents"), {QStringLiteral("*.qml")})) {
        const QString path = appletDir + QStringLiteral("/contents/") + file;
        QQmlComponent component(engine, QUrl::fromLocalFile(path), QQmlComponent::PreferSynchronous);
        if (!component.isError()) {
            continue;
        }
        for (const QQmlError &error : component.errors()) {
            QString where = QStringLiteral("contents/") + file;
            if (error.url().isLocalFile()) {
                const QString relative = base.relativeFilePath(error.url().toLocalFile());
                where = relative.startsWith(QLatin1String("..")) ? error.url().toLocalFile() : relative;
            }
            problems << Problem{where, error.line(), error.description()};
        }
    }
    return problems;
}

QList<Problem> checkApplet(const QString &appletDir, const QString &poDir, QQmlEngine *engine)
{
    QList<Problem> problems = checkMetadata(appletDir);
    problems += checkConfigXml(appletDir);
    problems += checkTranslations(appletDir, poDir);
    if (!isCompiledApplet(appletDir) && engine) {
        problems += compileQml(appletDir, engine);
    }
    return problems;
}

} // namespace RayeShellCheck
