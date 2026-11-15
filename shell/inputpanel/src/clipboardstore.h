// Clipboard history for the panel: the history Klipper keeps (newest
// first) with the items the user pinned on top. Pins belong to the panel
// and survive "Clear all", as in Windows.
#pragma once

#include <KConfigGroup>

#include <QObject>
#include <QStringList>

// Where the history comes from: Klipper on D-Bus, or a fake in tests.
class ClipboardBackend : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    virtual bool available() const = 0;
    virtual QStringList history() const = 0; // newest first
    // Make this text the clipboard (it becomes the newest item).
    virtual void setCurrent(const QString &text) = 0;
    virtual void clear() = 0;

Q_SIGNALS:
    void changed();
};

class ClipboardStore : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaxPins = 50;

    struct Item {
        QString text;
        bool pinned = false;
        bool operator==(const Item &other) const = default;
    };

    ClipboardStore(ClipboardBackend *backend, const KConfigGroup &pins, QObject *parent = nullptr);

    bool available() const;
    QList<Item> items() const;
    QStringList pins() const;

    void pin(const QString &text);
    void unpin(const QString &text);
    // Removes the item from the history and from the pins.
    void remove(const QString &text);
    // Empties the history; pinned items stay.
    void clearAll();
    void use(const QString &text);

    // Pure part, for tests: pinned first (pin order), then the history.
    static QList<Item> merge(const QStringList &pins, const QStringList &history);

Q_SIGNALS:
    void changed();

private:
    void savePins(const QStringList &pins);

    ClipboardBackend *m_backend;
    KConfigGroup m_pins;
};
