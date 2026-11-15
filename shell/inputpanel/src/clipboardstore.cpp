#include "clipboardstore.h"

#include <KConfig>

#include <QFile>

namespace
{
const char s_pinsKey[] = "Pinned";
}

ClipboardStore::ClipboardStore(ClipboardBackend *backend, const KConfigGroup &pins, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_pins(pins)
{
    connect(m_backend, &ClipboardBackend::changed, this, &ClipboardStore::changed);
}

bool ClipboardStore::available() const
{
    return m_backend->available();
}

QStringList ClipboardStore::pins() const
{
    return m_pins.readEntry(s_pinsKey, QStringList());
}

QList<ClipboardStore::Item> ClipboardStore::items() const
{
    return merge(pins(), m_backend->available() ? m_backend->history() : QStringList());
}

QList<ClipboardStore::Item> ClipboardStore::merge(const QStringList &pins, const QStringList &history)
{
    QList<Item> out;
    for (const QString &text : pins) {
        if (!text.isEmpty() && !out.contains(Item{text, true}))
            out.append({text, true});
    }
    for (const QString &text : history) {
        if (text.isEmpty() || pins.contains(text))
            continue;
        if (!out.contains(Item{text, false}))
            out.append({text, false});
    }
    return out;
}

void ClipboardStore::pin(const QString &text)
{
    QStringList list = pins();
    if (text.isEmpty() || list.contains(text))
        return;
    list.prepend(text);
    savePins(list.mid(0, MaxPins));
}

void ClipboardStore::unpin(const QString &text)
{
    QStringList list = pins();
    if (list.removeAll(text) > 0)
        savePins(list);
}

void ClipboardStore::remove(const QString &text)
{
    unpin(text);
    if (!m_backend->available())
        return;
    const QStringList history = m_backend->history();
    if (!history.contains(text))
        return;
    // Klipper cannot drop one item: rebuild the history without it,
    // oldest first so the order stays.
    QStringList kept = history;
    kept.removeAll(text);
    m_backend->clear();
    for (auto it = kept.crbegin(); it != kept.crend(); ++it)
        m_backend->setCurrent(*it);
    Q_EMIT changed();
}

void ClipboardStore::clearAll()
{
    if (m_backend->available())
        m_backend->clear();
    Q_EMIT changed();
}

void ClipboardStore::use(const QString &text)
{
    if (m_backend->available())
        m_backend->setCurrent(text);
}

void ClipboardStore::savePins(const QStringList &pins)
{
    m_pins.writeEntry(s_pinsKey, pins);
    m_pins.sync();
    // Pinned items can be anything the user copied: keep the file private.
    const QString file = m_pins.config()->name();
    if (!file.isEmpty() && QFile::exists(file))
        QFile::setPermissions(file, QFile::ReadOwner | QFile::WriteOwner);
    Q_EMIT changed();
}
