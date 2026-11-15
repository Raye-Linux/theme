#include "panel.h"
#include "fakeinput.h"
#include "klipperbackend.h"
#include "placement.h"

#include <LayerShellQt/Window>

#include <QClipboard>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QGuiApplication>
#include <QLocale>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>

namespace
{
constexpr int s_maxRecent = 24;
constexpr int s_kwinWaitMs = 300;
constexpr int s_focusBackMs = 150; // the window under the panel gets the keyboard back
constexpr int s_restoreMs = 600; // then the clipboard gets its old text back
constexpr int s_idleMs = 5 * 60 * 1000;
constexpr int s_taskbarHeight = 48;
const QSize s_panelSize(360, 460);
const QString s_kwinScriptName = QStringLiteral("raye-inputpanel-query");

// Run once by KWin: tells the panel where the pointer is and which window
// has the keyboard, so it opens next to it and pastes the right way.
const char s_kwinScript[] =
    "var w = workspace.activeWindow;\n"
    "callDBus(\"org.raye.InputPanel\", \"/InputPanel\", \"org.raye.InputPanel\", \"placeAt\",\n"
    "    String(workspace.cursorPos.x), String(workspace.cursorPos.y), w ? String(w.resourceClass) : \"\");\n";

bool onWayland()
{
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

QString readResource(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QVariantList toVariant(const QList<CharGroup> &groups)
{
    QVariantList out;
    for (const CharGroup &g : groups)
        out.append(QVariantMap{{QStringLiteral("id"), g.id}, {QStringLiteral("items"), g.items}});
    return out;
}

QDBusMessage kwinScripting(const QString &method)
{
    return QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"), method);
}
} // namespace

const QStringList Panel::Modes = {QStringLiteral("emoji"), QStringLiteral("kaomoji"), QStringLiteral("symbols"), QStringLiteral("clipboard")};

Panel::Panel(KSharedConfigPtr state, QObject *parent)
    : QObject(parent)
    , m_state(std::move(state))
    , m_emojiModel(&m_dict)
    , m_klipper(new KlipperBackend(this))
    , m_clipboard(m_klipper, m_state->group(QStringLiteral("Clipboard")))
{
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("plasma/emoji"),
                                                       QStandardPaths::LocateDirectory);
    QStringList locales;
    for (const QString &language : QLocale().uiLanguages()) {
        if (language.startsWith(QLatin1String("en")))
            break;
        locales = EmojiDict::localeChain(language);
        if (!locales.isEmpty())
            break;
    }
    m_dictLoaded = m_dict.load(dirs, locales);
    m_emojiModel.reload();

    m_kaomoji = parseCharGroups(readResource(QStringLiteral(":/raye-inputpanel/kaomoji.txt")), CharSplit::Lines);
    m_symbols = parseCharGroups(readResource(QStringLiteral(":/raye-inputpanel/symbols.txt")), CharSplit::Spaces);

    const KConfigGroup emojiGroup = m_state->group(QStringLiteral("Emoji"));
    m_emojiModel.setTone(emojiGroup.readEntry("Tone", 0));
    m_emojiModel.setRecent(emojiGroup.readEntry("Recent", QStringList()));
    if (hasRecent())
        m_emojiModel.setGroup(EmojiModel::Recent);

    connect(&m_clipboard, &ClipboardStore::changed, this, &Panel::clipboardChanged);

    if (onWayland())
        m_fakeInput = new FakeInput;

    m_kwinTimeout.setSingleShot(true);
    m_kwinTimeout.setInterval(s_kwinWaitMs);
    connect(&m_kwinTimeout, &QTimer::timeout, this, [this]() {
        m_waitingForKWin = false;
        m_targetClass.clear();
        showAt({}, false);
    });

    m_idle.setSingleShot(true);
    m_idle.setInterval(s_idleMs);
    connect(&m_idle, &QTimer::timeout, qApp, &QCoreApplication::quit);

    // The script KWin runs at each opening; private to this user.
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (!runtime.isEmpty()) {
        QFile file(runtime + QStringLiteral("/raye-inputpanel-query.js"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
            file.write(s_kwinScript);
            m_kwinScript = file.fileName();
        }
    }
}

Panel::~Panel()
{
    delete m_fakeInput;
    if (!m_kwinScript.isEmpty())
        QFile::remove(m_kwinScript);
}

void Panel::setWindow(QQuickWindow *window, int shadowMargin)
{
    m_window = window;
    m_shadowMargin = shadowMargin;
    connect(window, &QWindow::activeChanged, this, [this]() {
        // Clicking elsewhere closes the panel, as in Windows.
        if (m_window->isVisible() && !m_window->isActive() && onWayland())
            hideWindow();
    });
}

bool Panel::emojiLoaded() const
{
    return m_dictLoaded;
}

void Panel::setMode(const QString &mode)
{
    if (!Modes.contains(mode) || m_mode == mode)
        return;
    m_mode = mode;
    Q_EMIT modeChanged();
}

int Panel::tone() const
{
    return m_emojiModel.tone();
}

void Panel::setTone(int tone)
{
    if (tone == m_emojiModel.tone())
        return;
    m_emojiModel.setTone(tone);
    KConfigGroup group = m_state->group(QStringLiteral("Emoji"));
    group.writeEntry("Tone", m_emojiModel.tone());
    group.sync();
    Q_EMIT toneChanged();
}

bool Panel::hasRecent() const
{
    return !m_state->group(QStringLiteral("Emoji")).readEntry("Recent", QStringList()).isEmpty();
}

QVariantList Panel::kaomoji() const
{
    return toVariant(m_kaomoji);
}

QVariantList Panel::symbols() const
{
    return toVariant(m_symbols);
}

QVariantList Panel::clipboard() const
{
    QVariantList out;
    for (const ClipboardStore::Item &item : m_clipboard.items())
        out.append(QVariantMap{{QStringLiteral("text"), item.text}, {QStringLiteral("pinned"), item.pinned}});
    return out;
}

bool Panel::clipboardAvailable() const
{
    return m_clipboard.available();
}

void Panel::open(const QString &mode)
{
    const QString wanted = Modes.contains(mode) ? mode : QStringLiteral("emoji");
    if (m_window && m_window->isVisible()) {
        if (wanted == m_mode) {
            hideWindow();
            return;
        }
        setMode(wanted);
        m_window->requestActivate();
        Q_EMIT opened();
        return;
    }
    setMode(wanted);
    if (wanted == QLatin1String("clipboard"))
        Q_EMIT clipboardChanged();
    askKWin();
}

void Panel::askKWin()
{
    if (!onWayland() || m_kwinScript.isEmpty()) {
        showAt({}, false);
        return;
    }
    m_waitingForKWin = true;
    m_kwinTimeout.start();
    QDBusConnection bus = QDBusConnection::sessionBus();
    // a script left by a run that ended badly would hold the name
    bus.call(kwinScripting(QStringLiteral("unloadScript")) << s_kwinScriptName);
    QDBusMessage load = kwinScripting(QStringLiteral("loadScript"));
    load << m_kwinScript << s_kwinScriptName;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(load), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<int> reply = *w;
        w->deleteLater();
        if (reply.isError() || reply.value() < 0)
            return; // the timeout opens the panel anyway
        const QString path = QStringLiteral("/Scripting/Script%1").arg(reply.value());
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), path, QStringLiteral("org.kde.kwin.Script"), QStringLiteral("run")));
    });
}

void Panel::placeAt(const QString &x, const QString &y, const QString &windowClass)
{
    if (!m_waitingForKWin)
        return;
    m_waitingForKWin = false;
    m_kwinTimeout.stop();
    QDBusConnection::sessionBus().asyncCall(kwinScripting(QStringLiteral("unloadScript")) << s_kwinScriptName);
    bool okX = false, okY = false;
    const QPoint pointer(qRound(x.toDouble(&okX)), qRound(y.toDouble(&okY)));
    m_targetClass = windowClass.left(200);
    showAt(pointer, okX && okY);
}

void Panel::showAt(const QPoint &pointer, bool known)
{
    if (!m_window)
        return;
    m_idle.stop();
    if (onWayland()) {
        QScreen *screen = known ? QGuiApplication::screenAt(pointer) : nullptr;
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        const QRect area = screen->geometry();
        const QPoint corner = known ? Placement::near(area, pointer, s_panelSize, s_taskbarHeight)
                                    : Placement::corner(area, s_panelSize, s_taskbarHeight);
        m_window->setScreen(screen);
        auto *layer = LayerShellQt::Window::get(m_window);
        layer->setScope(QStringLiteral("raye-inputpanel"));
        layer->setLayer(LayerShellQt::Window::LayerTop);
        layer->setWantsToBeOnActiveScreen(false);
        layer->setScreen(screen);
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorLeft));
        layer->setExclusiveZone(-1);
        // the window has room for the shadow around the panel
        layer->setMargins(QMargins(corner.x() - area.x() - m_shadowMargin, corner.y() - area.y() - m_shadowMargin, 0, 0));
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    }
    m_window->show();
    m_window->requestActivate();
    Q_EMIT opened();
}

void Panel::hideWindow()
{
    if (m_window)
        m_window->hide();
    m_idle.start();
}

void Panel::close()
{
    hideWindow();
}

void Panel::pickEmoji(const QString &emoji, const QString &base)
{
    KConfigGroup group = m_state->group(QStringLiteral("Emoji"));
    QStringList recent = group.readEntry("Recent", QStringList());
    const bool had = !recent.isEmpty();
    recent.removeAll(base);
    recent.prepend(base);
    recent = recent.mid(0, s_maxRecent);
    group.writeEntry("Recent", recent);
    group.sync();
    m_emojiModel.setRecent(recent);
    if (!had)
        Q_EMIT recentChanged();
    deliver(emoji, true);
}

void Panel::pickText(const QString &text)
{
    deliver(text, true);
}

void Panel::pickClipboard(const QString &text)
{
    // An item of the history becomes the clipboard, as in Windows.
    deliver(text, false);
}

void Panel::deliver(const QString &text, bool keepPrevious)
{
    hideWindow();
    QTimer::singleShot(s_focusBackMs, this, [this, text, keepPrevious]() {
        const QString previous = keepPrevious ? clipboardText() : QString();
        setClipboard(text);
        const bool pasted = m_fakeInput && m_fakeInput->press(PasteKeys::forWindow(m_targetClass));
        // Without a paste the text stays on the clipboard for the user.
        if (pasted && keepPrevious && !previous.isEmpty() && previous != text) {
            QTimer::singleShot(s_restoreMs, this, [this, previous]() {
                setClipboard(previous);
            });
        }
    });
}

QString Panel::clipboardText() const
{
    if (m_klipper->available())
        return m_klipper->current();
    return QGuiApplication::clipboard()->text();
}

void Panel::setClipboard(const QString &text)
{
    // Klipper owns the clipboard for the whole session: it keeps the text
    // there even when the panel has no keyboard focus.
    if (m_klipper->available())
        m_klipper->setCurrent(text);
    else
        QGuiApplication::clipboard()->setText(text);
}

void Panel::pin(const QString &text)
{
    m_clipboard.pin(text);
}

void Panel::unpin(const QString &text)
{
    m_clipboard.unpin(text);
}

void Panel::remove(const QString &text)
{
    m_clipboard.remove(text);
}

void Panel::clearAll()
{
    m_clipboard.clearAll();
}
