#include "clipboardstore.h"

#include <KConfig>

#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using Item = ClipboardStore::Item;

// Behaves like Klipper: newest first, setting a text moves it to the top.
class FakeKlipper : public ClipboardBackend
{
public:
    bool up = true;
    QStringList items;
    int clears = 0;

    bool available() const override
    {
        return up;
    }
    QStringList history() const override
    {
        return items;
    }
    void setCurrent(const QString &text) override
    {
        items.removeAll(text);
        items.prepend(text);
        Q_EMIT changed();
    }
    void clear() override
    {
        ++clears;
        items.clear();
        Q_EMIT changed();
    }
};

class TestClipboardStore : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString file() const
    {
        return m_dir.filePath(QStringLiteral("raye-inputpanelstaterc"));
    }

private Q_SLOTS:
    void init()
    {
        QFile::remove(file());
    }

    void mergePinsFirst()
    {
        const auto items = ClipboardStore::merge({QStringLiteral("b"), QStringLiteral("x")},
                                                 {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QString(), QStringLiteral("a")});
        QCOMPARE(items,
                 (QList<Item>{{QStringLiteral("b"), true}, {QStringLiteral("x"), true}, {QStringLiteral("a"), false}, {QStringLiteral("c"), false}}));
    }

    void pinAndUnpin()
    {
        FakeKlipper klipper;
        klipper.items = {QStringLiteral("one"), QStringLiteral("two")};
        KConfig config(file(), KConfig::SimpleConfig);
        ClipboardStore store(&klipper, config.group(QStringLiteral("Clipboard")));
        QSignalSpy spy(&store, &ClipboardStore::changed);

        store.pin(QStringLiteral("two"));
        QCOMPARE(store.items().first(), (Item{QStringLiteral("two"), true}));
        QCOMPARE(spy.count(), 1);
        store.pin(QStringLiteral("two")); // already pinned: nothing
        QCOMPARE(store.pins(), QStringList{QStringLiteral("two")});

        // kept on disk, and only the user can read it
        KConfig again(file(), KConfig::SimpleConfig);
        QCOMPARE(again.group(QStringLiteral("Clipboard")).readEntry("Pinned", QStringList()), QStringList{QStringLiteral("two")});
        QCOMPARE(QFileInfo(file()).permissions() & (QFile::ReadGroup | QFile::ReadOther), QFile::Permissions());

        store.unpin(QStringLiteral("two"));
        QCOMPARE(store.items(), (QList<Item>{{QStringLiteral("one"), false}, {QStringLiteral("two"), false}}));
    }

    void pinsCapped()
    {
        FakeKlipper klipper;
        KConfig config(file(), KConfig::SimpleConfig);
        ClipboardStore store(&klipper, config.group(QStringLiteral("Clipboard")));
        for (int i = 0; i < ClipboardStore::MaxPins + 5; ++i)
            store.pin(QString::number(i));
        QCOMPARE(store.pins().size(), ClipboardStore::MaxPins);
        QCOMPARE(store.pins().first(), QString::number(ClipboardStore::MaxPins + 4));
    }

    void clearAllKeepsPins()
    {
        FakeKlipper klipper;
        klipper.items = {QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three")};
        KConfig config(file(), KConfig::SimpleConfig);
        ClipboardStore store(&klipper, config.group(QStringLiteral("Clipboard")));
        store.pin(QStringLiteral("two"));
        store.clearAll();
        QCOMPARE(klipper.clears, 1);
        QCOMPARE(store.items(), QList<Item>{(Item{QStringLiteral("two"), true})});
    }

    void removeOne()
    {
        FakeKlipper klipper;
        klipper.items = {QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three")};
        KConfig config(file(), KConfig::SimpleConfig);
        ClipboardStore store(&klipper, config.group(QStringLiteral("Clipboard")));
        store.pin(QStringLiteral("three"));
        store.remove(QStringLiteral("two"));
        // order kept, the current clipboard is still the newest item
        QCOMPARE(klipper.items, (QStringList{QStringLiteral("one"), QStringLiteral("three")}));
        store.remove(QStringLiteral("three"));
        QCOMPARE(store.pins(), QStringList());
        QCOMPARE(klipper.items, QStringList{QStringLiteral("one")});
        // not in the history: nothing is rebuilt
        const int clears = klipper.clears;
        store.remove(QStringLiteral("nowhere"));
        QCOMPARE(klipper.clears, clears);
    }

    void withoutKlipper()
    {
        FakeKlipper klipper;
        klipper.up = false;
        klipper.items = {QStringLiteral("hidden")};
        KConfig config(file(), KConfig::SimpleConfig);
        ClipboardStore store(&klipper, config.group(QStringLiteral("Clipboard")));
        QVERIFY(!store.available());
        store.pin(QStringLiteral("kept"));
        QCOMPARE(store.items(), QList<Item>{(Item{QStringLiteral("kept"), true})});
        store.clearAll();
        store.remove(QStringLiteral("hidden"));
        QCOMPARE(klipper.clears, 0);
    }

    void use()
    {
        FakeKlipper klipper;
        klipper.items = {QStringLiteral("one"), QStringLiteral("two")};
        KConfig config(file(), KConfig::SimpleConfig);
        ClipboardStore store(&klipper, config.group(QStringLiteral("Clipboard")));
        store.use(QStringLiteral("two"));
        QCOMPARE(klipper.items.first(), QStringLiteral("two"));
    }
};

QTEST_GUILESS_MAIN(TestClipboardStore)
#include "tst_clipboardstore.moc"
