// The emoji and clipboard panel (Win+. and Win+V). It stays running once
// opened, hidden between uses, so it opens at once the next time.
#pragma once

#include "chargroups.h"
#include "clipboardstore.h"
#include "emojidict.h"
#include "emojimodel.h"

#include <KSharedConfig>

#include <QObject>
#include <QPoint>
#include <QTimer>
#include <QVariantList>

class QQuickWindow;
class FakeInput;
class KlipperBackend;

class Panel : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.raye.InputPanel")
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(EmojiModel *emoji READ emoji CONSTANT)
    Q_PROPERTY(bool emojiLoaded READ emojiLoaded CONSTANT)
    Q_PROPERTY(int tone READ tone WRITE setTone NOTIFY toneChanged)
    Q_PROPERTY(bool hasRecent READ hasRecent NOTIFY recentChanged)
    Q_PROPERTY(QVariantList kaomoji READ kaomoji CONSTANT)
    Q_PROPERTY(QVariantList symbols READ symbols CONSTANT)
    Q_PROPERTY(QVariantList clipboard READ clipboard NOTIFY clipboardChanged)
    Q_PROPERTY(bool clipboardAvailable READ clipboardAvailable NOTIFY clipboardChanged)

public:
    static const QStringList Modes; // emoji, kaomoji, symbols, clipboard

    explicit Panel(KSharedConfigPtr state, QObject *parent = nullptr);
    ~Panel() override;

    void setWindow(QQuickWindow *window, int shadowMargin);
    bool emojiLoaded() const;

    QString mode() const
    {
        return m_mode;
    }
    void setMode(const QString &mode);
    EmojiModel *emoji()
    {
        return &m_emojiModel;
    }
    int tone() const;
    void setTone(int tone);
    bool hasRecent() const;
    QVariantList kaomoji() const;
    QVariantList symbols() const;
    QVariantList clipboard() const;
    bool clipboardAvailable() const;

    // Picking: the panel hides, then the text goes into the window it
    // opened over.
    Q_INVOKABLE void pickEmoji(const QString &emoji, const QString &base);
    Q_INVOKABLE void pickText(const QString &text);
    Q_INVOKABLE void pickClipboard(const QString &text);

    Q_INVOKABLE void pin(const QString &text);
    Q_INVOKABLE void unpin(const QString &text);
    Q_INVOKABLE void remove(const QString &text);
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE void close();

public Q_SLOTS:
    // From the desktop file actions and a second start: open on a tab
    // (shown again: closes it, like pressing Win+V twice).
    Q_SCRIPTABLE void open(const QString &mode);
    // From the KWin script: where the pointer is and what window is active.
    Q_SCRIPTABLE void placeAt(const QString &x, const QString &y, const QString &windowClass);

Q_SIGNALS:
    void modeChanged();
    void toneChanged();
    void recentChanged();
    void clipboardChanged();
    void opened();

private:
    void askKWin();
    void showAt(const QPoint &pointer, bool known);
    void deliver(const QString &text, bool keepPrevious);
    void setClipboard(const QString &text);
    QString clipboardText() const;
    void hideWindow();

    KSharedConfigPtr m_state;
    EmojiDict m_dict;
    bool m_dictLoaded = false;
    EmojiModel m_emojiModel;
    QList<CharGroup> m_kaomoji;
    QList<CharGroup> m_symbols;
    KlipperBackend *m_klipper;
    ClipboardStore m_clipboard;
    FakeInput *m_fakeInput = nullptr;
    QQuickWindow *m_window = nullptr;
    int m_shadowMargin = 0;
    QString m_mode = QStringLiteral("emoji");
    QString m_targetClass;
    QString m_kwinScript;
    bool m_waitingForKWin = false;
    QTimer m_kwinTimeout;
    QTimer m_idle;
};
