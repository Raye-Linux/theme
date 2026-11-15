/*
 * SPDX-FileCopyrightText: 2026 techoraye
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Loads the built decoration plugin through a small stand-in for KWin and
// paints it: title bar height, 46x32 caption buttons in the top right
// corner, rounded corners that go square when maximized, the red close
// button and the dimmer inactive title. With RAYE_DECO_SHOTS=<folder>
// it also writes pictures of every state, night and day, 100 and 150 %.

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButton>
#include <KDecoration3/DecorationSettings>
#include <KDecoration3/DecorationShadow>
#include <KDecoration3/Private/DecoratedWindowPrivate>
#include <KDecoration3/Private/DecorationBridge>
#include <KDecoration3/Private/DecorationSettingsPrivate>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <QApplication>
#include <QDir>
#include <QHoverEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTest>

#include <memory>

namespace
{
struct Scheme {
    bool dark;
    QColor title;
    QColor active;
    QColor inactive;
    QColor accent;
    QColor client;
};

const Scheme night{true, QColor(0x13, 0x0E, 0x1F), QColor(0xF3, 0xEA, 0xFF), QColor(0x8C, 0x7D, 0xA6), QColor(0x7B, 0x3C, 0xFF), QColor(0x1B, 0x15, 0x29)};
const Scheme day{false, QColor(0xF6, 0xF3, 0xFB), QColor(0x1A, 0x10, 0x28), QColor(0x7A, 0x6D, 0x8F), QColor(0x7B, 0x3C, 0xFF), QColor(0xFC, 0xFB, 0xFE)};

// what the stand-in compositor tells the decoration about its window
struct WindowState {
    Scheme scheme = night;
    bool active = true;
    bool maximized = false;
    qreal scale = 1.0;
    QString caption = QStringLiteral("Document - Notepad");
};

class Window : public KDecoration3::DecoratedWindowPrivateV4
{
public:
    Window(KDecoration3::DecoratedWindow *client, KDecoration3::Decoration *decoration, const WindowState *state)
        : DecoratedWindowPrivateV4(client, decoration)
        , m_state(state)
    {
    }

    bool isActive() const override { return m_state->active; }
    QString caption() const override { return m_state->caption; }
    bool isOnAllDesktops() const override { return false; }
    bool isShaded() const override { return false; }
    QIcon icon() const override { return QIcon::fromTheme(QStringLiteral("text-plain"), m_icon); }
    bool isMaximized() const override { return m_state->maximized; }
    bool isMaximizedHorizontally() const override { return m_state->maximized; }
    bool isMaximizedVertically() const override { return m_state->maximized; }
    bool isKeepAbove() const override { return false; }
    bool isKeepBelow() const override { return false; }
    bool isCloseable() const override { return true; }
    bool isMaximizeable() const override { return true; }
    bool isMinimizeable() const override { return true; }
    bool providesContextHelp() const override { return false; }
    bool isModal() const override { return false; }
    bool isShadeable() const override { return false; }
    bool isMoveable() const override { return true; }
    bool isResizeable() const override { return true; }
    qreal width() const override { return 480; }
    qreal height() const override { return 200; }
    QSizeF size() const override { return QSizeF(width(), height()); }
    QPalette palette() const override
    {
        QPalette p;
        p.setColor(QPalette::Window, m_state->scheme.title);
        p.setColor(QPalette::WindowText, m_state->scheme.active);
        p.setColor(QPalette::Text, m_state->scheme.active);
        p.setColor(QPalette::Highlight, m_state->scheme.accent);
        return p;
    }
    Qt::Edges adjacentScreenEdges() const override { return Qt::Edges(); }
    qreal scale() const override { return m_state->scale; }
    qreal nextScale() const override { return m_state->scale; }
    void requestShowToolTip(const QString &) override { }
    void requestHideToolTip() override { }
    void requestClose() override { }
    void requestToggleMaximization(Qt::MouseButtons) override { }
    void requestMinimize() override { }
    void requestContextHelp() override { }
    void requestToggleOnAllDesktops() override { }
    void requestToggleShade() override { }
    void requestToggleKeepAbove() override { }
    void requestToggleKeepBelow() override { }
    void requestShowWindowMenu(const QRect &) override { }
    QColor color(KDecoration3::ColorGroup group, KDecoration3::ColorRole role) const override
    {
        using KDecoration3::ColorGroup;
        using KDecoration3::ColorRole;
        if (group == ColorGroup::Warning) {
            return QColor(0xFF, 0x6B, 0x85);
        }
        switch (role) {
        case ColorRole::Frame:
        case ColorRole::TitleBar:
            return m_state->scheme.title;
        case ColorRole::Foreground:
            return group == ColorGroup::Active ? m_state->scheme.active : m_state->scheme.inactive;
        default:
            return QColor();
        }
    }
    QString windowClass() const override { return QStringLiteral("notepad"); }
    bool hasApplicationMenu() const override { return false; }
    bool isApplicationMenuActive() const override { return false; }
    void showApplicationMenu(int) override { }
    void requestShowApplicationMenu(const QRect &, int) override { }
    QString applicationMenuServiceName() const override { return QString(); }
    QString applicationMenuObjectPath() const override { return QString(); }
    void popup(const KDecoration3::Positioner &, QMenu *) override { }
    bool isExcludedFromCapture() const override { return false; }
    void requestToggleExcludeFromCapture() override { }

private:
    const WindowState *m_state;
    QIcon m_icon;
};

class Settings : public KDecoration3::DecorationSettingsPrivateV2
{
public:
    explicit Settings(KDecoration3::DecorationSettings *parent)
        : DecorationSettingsPrivateV2(parent)
    {
        setGridUnit(10);
        setSmallSpacing(2);
        setLargeSpacing(8);
    }
    bool isOnAllDesktopsAvailable() const override { return true; }
    bool isAlphaChannelSupported() const override { return true; }
    bool isCloseOnDoubleClickOnMenu() const override { return false; }
    QList<KDecoration3::DecorationButtonType> decorationButtonsLeft() const override { return {}; }
    QList<KDecoration3::DecorationButtonType> decorationButtonsRight() const override
    {
        using T = KDecoration3::DecorationButtonType;
        return {T::Minimize, T::Maximize, T::Close};
    }
    KDecoration3::BorderSize borderSize() const override { return KDecoration3::BorderSize::Normal; }
    QFont font() const override
    {
        QFont f(QStringLiteral("Inter"));
        f.setPointSizeF(9);
        f.setWeight(QFont::Medium);
        return f;
    }
    QFontMetricsF fontMetrics() const override { return QFontMetricsF(font()); }
    bool isAlwaysShowExcludeFromCapture() const override { return false; }
};

class Bridge : public KDecoration3::DecorationBridge
{
public:
    WindowState state;
    std::unique_ptr<KDecoration3::DecoratedWindowPrivate> createClient(KDecoration3::DecoratedWindow *client, KDecoration3::Decoration *decoration) override
    {
        return std::make_unique<Window>(client, decoration, &state);
    }
    std::unique_ptr<KDecoration3::DecorationSettingsPrivate> settings(KDecoration3::DecorationSettings *parent) override
    {
        return std::make_unique<Settings>(parent);
    }
};

int channelDistance(const QColor &a, const QColor &b)
{
    return qMax(qAbs(a.red() - b.red()), qMax(qAbs(a.green() - b.green()), qAbs(a.blue() - b.blue())));
}
}

class TestRayeDecorationRender : public QObject
{
    Q_OBJECT

    std::unique_ptr<Bridge> m_bridge;
    std::unique_ptr<KDecoration3::Decoration> m_decoration;
    std::shared_ptr<KDecoration3::DecorationSettings> m_settings;

    void build(const WindowState &state)
    {
        m_decoration.reset();
        m_settings.reset();
        m_bridge = std::make_unique<Bridge>();
        m_bridge->state = state;
        auto factory = KPluginFactory::loadFactory(KPluginMetaData(QStringLiteral(DECORATION_PLUGIN))).plugin;
        QVERIFY(factory);
        const QVariantMap args({{QStringLiteral("bridge"), QVariant::fromValue<KDecoration3::DecorationBridge *>(m_bridge.get())}});
        m_decoration.reset(factory->create<KDecoration3::Decoration>(nullptr, QVariantList({args})));
        QVERIFY(m_decoration);
        m_settings = std::make_shared<KDecoration3::DecorationSettings>(m_bridge.get());
        m_decoration->setSettings(m_settings);
        m_decoration->create();
        // like KWin, take each new state of the decoration at once
        auto decoration = m_decoration.get();
        connect(decoration, &KDecoration3::Decoration::nextStateChanged, decoration, [decoration](std::shared_ptr<KDecoration3::DecorationState> state) {
            decoration->apply(state->clone());
        });
        QVERIFY(m_decoration->init());
        m_decoration->apply(m_decoration->nextState()->clone());
        // buttons are placed on the next turn of the event loop
        QTest::qWait(50);
    }

    KDecoration3::DecorationButton *button(KDecoration3::DecorationButtonType type) const
    {
        const auto buttons = m_decoration->findChildren<KDecoration3::DecorationButton *>();
        for (auto b : buttons) {
            if (b->type() == type && b->isVisible()) {
                return b;
            }
        }
        return nullptr;
    }

    void hover(const QPointF &point)
    {
        QHoverEvent event(QEvent::HoverMove, point, point, QPointF(-1, -1));
        QCoreApplication::sendEvent(m_decoration.get(), &event);
    }

    void press(const QPointF &point)
    {
        QMouseEvent event(QEvent::MouseButtonPress, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(m_decoration.get(), &event);
    }

    //* the decoration alone, at the window's scale, transparent around it
    QImage paint() const
    {
        const qreal scale = m_bridge->state.scale;
        const QSizeF size = m_decoration->size();
        QImage image((size * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(scale);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        m_decoration->paint(&painter, m_decoration->rect());
        painter.end();
        return image;
    }

    //* a picture for people: wallpaper, shadow, window contents and decoration
    void shoot(const QString &name) const
    {
        const QString folder(qEnvironmentVariable("RAYE_DECO_SHOTS"));
        if (folder.isEmpty()) {
            return;
        }
        QDir().mkpath(folder);
        const qreal scale = m_bridge->state.scale;
        const QImage deco(paint());
        const QSizeF size = m_decoration->size();
        const int margin = 72;
        QImage out((size * scale).toSize() + QSize(2 * margin, 2 * margin) * scale, QImage::Format_ARGB32_Premultiplied);
        out.setDevicePixelRatio(scale);
        out.fill(m_bridge->state.scheme.dark ? QColor(0x3A, 0x2E, 0x55) : QColor(0xC9, 0xBF, 0xDC));
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        // shadow: the nine-patch KWin would draw
        if (auto shadow = m_decoration->shadow()) {
            const QImage tex(shadow->shadow());
            const QMarginsF pad(shadow->padding());
            const QRectF inner(shadow->innerShadowRect());
            const QRectF frame(margin, margin, size.width(), size.height());
            const QRectF outer(frame.marginsAdded(pad));
            const qreal xs[] = {0, inner.left(), inner.right(), qreal(tex.width())};
            const qreal ys[] = {0, inner.top(), inner.bottom(), qreal(tex.height())};
            const qreal ox[] = {outer.left(), outer.left() + inner.left(), outer.right() - (tex.width() - inner.right()), outer.right()};
            const qreal oy[] = {outer.top(), outer.top() + inner.top(), outer.bottom() - (tex.height() - inner.bottom()), outer.bottom()};
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    p.drawImage(QRectF(QPointF(ox[i], oy[j]), QPointF(ox[i + 1], oy[j + 1])), tex, QRectF(QPointF(xs[i], ys[j]), QPointF(xs[i + 1], ys[j + 1])));
                }
            }
        }
        // client area, clipped to the bottom corners KWin rounds
        const auto radius = m_decoration->borderRadius();
        QPainterPath client;
        const QRectF clientRect(margin, margin + m_decoration->borderTop(), size.width(), size.height() - m_decoration->borderTop());
        client.addRoundedRect(clientRect.adjusted(0, -8, 0, 0), radius.bottomLeft(), radius.bottomLeft());
        p.setPen(Qt::NoPen);
        p.setBrush(m_bridge->state.scheme.client);
        p.save();
        p.setClipRect(clientRect);
        p.drawPath(client);
        p.restore();
        p.drawImage(QPointF(margin, margin), deco);
        // outline, as KWin draws it
        const auto outline = m_decoration->borderOutline();
        if (outline.thickness() > 0) {
            p.setPen(QPen(outline.color(), outline.thickness()));
            p.setBrush(Qt::NoBrush);
            const auto r = outline.radius();
            p.drawRoundedRect(QRectF(margin, margin, size.width(), size.height()).adjusted(0.5, 0.5, -0.5, -0.5), r.topLeft(), r.topLeft());
        }
        p.end();
        out.save(folder + QLatin1Char('/') + name + QStringLiteral(".png"));
    }

private Q_SLOTS:
    void initTestCase()
    {
        qRegisterMetaType<KDecoration3::DecorationBridge *>();
    }

    void titleBarAndButtons()
    {
        build(WindowState());
        QCOMPARE(m_decoration->borderTop(), 32.0);
        QCOMPARE(m_decoration->borderLeft(), 0.0);
        QCOMPARE(m_decoration->borderRight(), 0.0);
        QCOMPARE(m_decoration->borderBottom(), 0.0);
        // an 8 px resize band outside the window instead
        QCOMPARE(m_decoration->resizeOnlyBorders().left(), 8.0);
        QCOMPARE(m_decoration->resizeOnlyBorders().bottom(), 8.0);

        auto close = button(KDecoration3::DecorationButtonType::Close);
        auto maximize = button(KDecoration3::DecorationButtonType::Maximize);
        auto minimize = button(KDecoration3::DecorationButtonType::Minimize);
        QVERIFY(close && maximize && minimize);
        QCOMPARE(close->geometry().size(), QSizeF(46, 32));
        QCOMPARE(close->geometry().right(), m_decoration->size().width());
        QCOMPARE(close->geometry().top(), 0.0);
        // no gap between the buttons
        QCOMPARE(maximize->geometry().right(), close->geometry().left());
        QCOMPARE(minimize->geometry().right(), maximize->geometry().left());
        shoot(QStringLiteral("night-100-active"));
    }

    void roundedCornersAndBottomRadius()
    {
        build(WindowState());
        const QImage image(paint());
        // the top left corner pixel is outside the 8 px rounding
        QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
        // the title bar is the window colour
        QVERIFY(channelDistance(image.pixelColor(4, 20), night.title) <= 2);
        // KWin rounds the bottom corners too
        QCOMPARE(m_decoration->borderRadius().bottomLeft(), 8.0);
        QCOMPARE(m_decoration->borderRadius().bottomRight(), 8.0);
        // 1 px outline in stroke.surface
        QCOMPARE(m_decoration->borderOutline().color().alpha(), qRound(0.09 * 255));
    }

    void maximizedIsSquare()
    {
        WindowState state;
        state.maximized = true;
        build(state);
        const QImage image(paint());
        QVERIFY(channelDistance(image.pixelColor(0, 0), night.title) <= 2);
        QCOMPARE(qAlpha(image.pixel(0, 0)), 255);
        QCOMPARE(m_decoration->borderRadius().bottomLeft(), 0.0);
        QCOMPARE(m_decoration->resizeOnlyBorders().left(), 0.0);
        shoot(QStringLiteral("night-100-maximized"));
    }

    void closeTurnsRed()
    {
        build(WindowState());
        auto close = button(KDecoration3::DecorationButtonType::Close);
        QVERIFY(close);
        hover(close->geometry().center());
        QTRY_VERIFY(close->isHovered());
        // the hover colour fades in: wait for the end of it
        QTest::qWait(250);
        const QImage image(paint());
        const QPointF inside(close->geometry().left() + 4, close->geometry().center().y());
        QVERIFY2(channelDistance(image.pixelColor(inside.toPoint()), QColor(0xE5, 0x33, 0x4F)) <= 3, qPrintable(image.pixelColor(inside.toPoint()).name()));
        // and follows the rounded top right corner of the window
        QCOMPARE(qAlpha(image.pixel(image.width() - 1, 0)), 0);
        shoot(QStringLiteral("night-100-hover-close"));
        press(close->geometry().center());
        QTRY_VERIFY(close->isPressed());
        shoot(QStringLiteral("night-100-pressed-close"));
        const QImage pressed(paint());
        QVERIFY(channelDistance(pressed.pixelColor(inside.toPoint()), QColor(0xC2, 0x2A, 0x44)) <= 3);
    }

    void hoverIsSubtle()
    {
        build(WindowState());
        auto maximize = button(KDecoration3::DecorationButtonType::Maximize);
        QVERIFY(maximize);
        hover(maximize->geometry().center());
        QTRY_VERIFY(maximize->isHovered());
        QTest::qWait(250);
        const QImage image(paint());
        const QColor c(image.pixelColor(QPointF(maximize->geometry().left() + 3, maximize->geometry().top() + 3).toPoint()));
        // white at 6 % over the title bar
        QVERIFY(c.lightness() > night.title.lightness());
        QVERIFY(channelDistance(c, night.title) <= 20);
        shoot(QStringLiteral("night-100-hover-maximize"));
    }

    void inactiveIsDimmer()
    {
        WindowState state;
        state.active = false;
        build(state);
        shoot(QStringLiteral("night-100-inactive"));
        QVERIFY(m_decoration->window()->isActive() == false);
    }

    void dayAndScales()
    {
        for (const qreal scale : {1.0, 1.5}) {
            for (const bool dark : {true, false}) {
                WindowState state;
                state.scheme = dark ? night : day;
                state.scale = scale;
                build(state);
                // 32 logical pixels at any scale
                QCOMPARE(m_decoration->borderTop(), 32.0);
                const QString prefix = QStringLiteral("%1-%2").arg(dark ? QStringLiteral("night") : QStringLiteral("day")).arg(qRound(scale * 100));
                shoot(prefix + QStringLiteral("-active"));
                auto close = button(KDecoration3::DecorationButtonType::Close);
                hover(close->geometry().center());
                QTest::qWait(250);
                shoot(prefix + QStringLiteral("-hover-close"));
                state.active = false;
                build(state);
                shoot(prefix + QStringLiteral("-inactive"));
            }
        }
    }
};

QTEST_MAIN(TestRayeDecorationRender)

#include "tst_rayedecorationrender.moc"
