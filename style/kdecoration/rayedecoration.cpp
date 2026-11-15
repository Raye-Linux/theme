/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 * SPDX-FileCopyrightText: 2021 Paul McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "rayedecoration.h"

#include "rayesettingsprovider.h"

#include "rayebutton.h"

#include "rayeboxshadowrenderer.h"

#include <KDecoration3/DecorationButtonGroup>
#include <KDecoration3/DecorationShadow>
#include <KDecoration3/ScaleHelpers>

#include <KColorUtils>
#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>

#include <KColorScheme>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QPainter>
#include <QPainterPath>
#include <QTextStream>
#include <QTimer>

K_PLUGIN_FACTORY_WITH_JSON(RayeDecoFactory, "raye.json", registerPlugin<Raye::Decoration>(); registerPlugin<Raye::Button>();)

namespace Raye
{
using KDecoration3::ColorGroup;
using KDecoration3::ColorRole;

//________________________________________________________________
static int g_sDecoCount = 0;
static int g_shadowSizeEnum = InternalSettings::ShadowLarge;
static int g_shadowStrength = 255;
static QColor g_shadowColor = Qt::black;
static bool g_shadowDark = true;
static qreal g_shadowRadius = 0;
static std::shared_ptr<KDecoration3::DecorationShadow> g_sShadow;
static std::shared_ptr<KDecoration3::DecorationShadow> g_sShadowInactive;

//* the shadow size setting scales the Raye shadow; Large is the spec's size
static qreal shadowSizeFactor(int size)
{
    switch (size) {
    case InternalSettings::ShadowNone:
        return 0;
    case InternalSettings::ShadowSmall:
        return 0.5;
    case InternalSettings::ShadowMedium:
        return 0.75;
    case InternalSettings::ShadowVeryLarge:
        return 1.25;
    case InternalSettings::ShadowLarge:
    default:
        return 1;
    }
}

//________________________________________________________________
Decoration::Decoration(QObject *parent, const QVariantList &args)
    : KDecoration3::Decoration(parent, args)
    , m_animation(new QVariantAnimation(this))
    , m_shadowAnimation(new QVariantAnimation(this))
{
    g_sDecoCount++;
}

//________________________________________________________________
Decoration::~Decoration()
{
    g_sDecoCount--;
    if (g_sDecoCount == 0) {
        // last deco destroyed, clean up shadows
        g_sShadow.reset();
        g_sShadowInactive.reset();
    }
}

//________________________________________________________________
void Decoration::setOpacity(qreal value)
{
    if (m_opacity == value) {
        return;
    }
    m_opacity = value;
    update();
}

//________________________________________________________________
QColor Decoration::titleBarColor() const
{
    if (hideTitleBar()) {
        return window()->color(ColorGroup::Inactive, ColorRole::TitleBar);
    } else if (m_animation->state() == QAbstractAnimation::Running) {
        return KColorUtils::mix(window()->color(ColorGroup::Inactive, ColorRole::TitleBar),
                                window()->color(ColorGroup::Active, ColorRole::TitleBar),
                                m_opacity);
    } else {
        return window()->color(window()->isActive() ? ColorGroup::Active : ColorGroup::Inactive, ColorRole::TitleBar);
    }
}

//________________________________________________________________
QColor Decoration::fontColor() const
{
    if (m_animation->state() == QAbstractAnimation::Running) {
        return KColorUtils::mix(window()->color(ColorGroup::Inactive, ColorRole::Foreground),
                                window()->color(ColorGroup::Active, ColorRole::Foreground),
                                m_opacity);
    } else {
        return window()->color(window()->isActive() ? ColorGroup::Active : ColorGroup::Inactive, ColorRole::Foreground);
    }
}

//________________________________________________________________
bool Decoration::isDark() const
{
    return qGray(window()->color(ColorGroup::Active, ColorRole::TitleBar).rgb()) < 128;
}

//________________________________________________________________
qreal Decoration::unit() const
{
    // smallSpacing is 2 on Wayland; on X11 it follows the font DPI
    return settings()->smallSpacing() / 2.0;
}

//________________________________________________________________
DecorationMetrics::Corners Decoration::corners() const
{
    if (!settings()->isAlphaChannelSupported()) {
        return {};
    }
    return DecorationMetrics::cornersFor(m_scaledCornerRadius, isMaximized(), isMaximized() ? Qt::Edges() : window()->adjacentScreenEdges());
}

//________________________________________________________________
QPainterPath Decoration::titleBarShape() const
{
    const QRectF rect(QPointF(0, 0), QSizeF(size().width(), borderTop()));
    const auto c(corners());
    QPainterPath path;
    path.moveTo(rect.left(), rect.bottom());
    path.lineTo(rect.left(), rect.top() + c.topLeft);
    if (c.topLeft > 0) {
        path.arcTo(QRectF(rect.left(), rect.top(), 2 * c.topLeft, 2 * c.topLeft), 180, -90);
    }
    path.lineTo(rect.right() - c.topRight, rect.top());
    if (c.topRight > 0) {
        path.arcTo(QRectF(rect.right() - 2 * c.topRight, rect.top(), 2 * c.topRight, 2 * c.topRight), 90, -90);
    }
    path.lineTo(rect.right(), rect.bottom());
    path.closeSubpath();
    return path;
}

//________________________________________________________________
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool Decoration::init()
#else
void Decoration::init()
#endif
{
    // active state change animation
    // It is important start and end value are of the same type, hence 0.0 and not just 0
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    // Linear to have the same easing as Raye animations
    m_animation->setEasingCurve(QEasingCurve::Linear);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setOpacity(value.toReal());
    });

    m_shadowAnimation->setStartValue(0.0);
    m_shadowAnimation->setEndValue(1.0);
    m_shadowAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_shadowAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_shadowOpacity = value.toReal();
        updateShadow();
    });

    // use DBus connection to update on raye configuration change
    auto dbus = QDBusConnection::sessionBus();
    dbus.connect(QString(),
                 QStringLiteral("/KGlobalSettings"),
                 QStringLiteral("org.kde.KGlobalSettings"),
                 QStringLiteral("notifyChange"),
                 this,
                 SLOT(reconfigure()));

    dbus.connect(QStringLiteral("org.kde.KWin"),
                 QStringLiteral("/org/kde/KWin"),
                 QStringLiteral("org.kde.KWin.TabletModeManager"),
                 QStringLiteral("tabletModeChanged"),
                 QStringLiteral("b"),
                 this,
                 SLOT(onTabletModeChanged(bool)));

    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                  QStringLiteral("/org/kde/KWin"),
                                                  QStringLiteral("org.freedesktop.DBus.Properties"),
                                                  QStringLiteral("Get"));
    message.setArguments({QStringLiteral("org.kde.KWin.TabletModeManager"), QStringLiteral("tabletMode")});
    auto call = new QDBusPendingCallWatcher(dbus.asyncCall(message), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call]() {
        QDBusPendingReply<QVariant> reply = *call;
        if (!reply.isError()) {
            onTabletModeChanged(reply.value().toBool());
        }

        call->deleteLater();
    });

    reconfigure();
    updateTitleBar();
    auto s = settings();
    connect(s.get(), &KDecoration3::DecorationSettings::borderSizeChanged, this, &Decoration::recalculateBorders);

    // a change in font might cause the borders to change
    connect(s.get(), &KDecoration3::DecorationSettings::fontChanged, this, &Decoration::recalculateBorders);
    connect(s.get(), &KDecoration3::DecorationSettings::spacingChanged, this, &Decoration::recalculateBorders);

    // buttons
    connect(s.get(), &KDecoration3::DecorationSettings::spacingChanged, this, &Decoration::updateButtonsGeometryDelayed);
    connect(s.get(), &KDecoration3::DecorationSettings::decorationButtonsLeftChanged, this, &Decoration::updateButtonsGeometryDelayed);
    connect(s.get(), &KDecoration3::DecorationSettings::decorationButtonsRightChanged, this, &Decoration::updateButtonsGeometryDelayed);

    // full reconfiguration
    connect(s.get(), &KDecoration3::DecorationSettings::reconfigured, this, &Decoration::reconfigure);
    connect(s.get(), &KDecoration3::DecorationSettings::reconfigured, SettingsProvider::self(), &SettingsProvider::reconfigure, Qt::UniqueConnection);
    connect(s.get(), &KDecoration3::DecorationSettings::reconfigured, this, &Decoration::updateButtonsGeometryDelayed);

    connect(window(), &KDecoration3::DecoratedWindow::activeChanged, this, &Decoration::recalculateBorders);
    connect(window(), &KDecoration3::DecoratedWindow::adjacentScreenEdgesChanged, this, &Decoration::recalculateBorders);
    connect(window(), &KDecoration3::DecoratedWindow::maximizedHorizontallyChanged, this, &Decoration::recalculateBorders);
    connect(window(), &KDecoration3::DecoratedWindow::maximizedVerticallyChanged, this, &Decoration::recalculateBorders);
    connect(window(), &KDecoration3::DecoratedWindow::shadedChanged, this, &Decoration::recalculateBorders);
    connect(window(), &KDecoration3::DecoratedWindow::captionChanged, this, [this]() {
        // update the caption area
        update(titleBar());
    });

    connect(window(), &KDecoration3::DecoratedWindow::activeChanged, this, &Decoration::updateAnimationState);
    connect(window(), &KDecoration3::DecoratedWindow::paletteChanged, this, &Decoration::recalculateBorders);
    connect(window(), &KDecoration3::DecoratedWindow::paletteChanged, this, &Decoration::updateShadow);
    connect(window(), &KDecoration3::DecoratedWindow::iconChanged, this, [this]() {
        update(titleBar());
    });
    connect(this, &KDecoration3::Decoration::bordersChanged, this, &Decoration::updateTitleBar);
    connect(this, &KDecoration3::Decoration::bordersChanged, this, &Decoration::updateButtonsGeometryDelayed);
    connect(window(), &KDecoration3::DecoratedWindow::adjacentScreenEdgesChanged, this, &Decoration::updateTitleBar);
    connect(window(), &KDecoration3::DecoratedWindow::widthChanged, this, &Decoration::updateTitleBar);
    connect(window(), &KDecoration3::DecoratedWindow::maximizedChanged, this, &Decoration::updateTitleBar);
    connect(window(), &KDecoration3::DecoratedWindow::maximizedChanged, this, &Decoration::setOpaque);

    connect(window(), &KDecoration3::DecoratedWindow::widthChanged, this, &Decoration::updateButtonsGeometry);
    connect(window(), &KDecoration3::DecoratedWindow::maximizedChanged, this, &Decoration::updateButtonsGeometry);
    connect(window(), &KDecoration3::DecoratedWindow::adjacentScreenEdgesChanged, this, &Decoration::updateButtonsGeometry);
    connect(window(), &KDecoration3::DecoratedWindow::shadedChanged, this, &Decoration::updateButtonsGeometry);

    connect(window(), &KDecoration3::DecoratedWindow::nextScaleChanged, this, &Decoration::updateScale);

    createButtons();
    updateShadow();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return true;
#endif
}

//________________________________________________________________
void Decoration::updateTitleBar()
{
    // the whole title bar is the drag area: there is no border around it
    setTitleBar(QRectF(0, 0, window()->width(), borderTop()));
}

//________________________________________________________________
void Decoration::updateAnimationState()
{
    if (m_shadowAnimation->duration() > 0) {
        m_shadowAnimation->setDirection(window()->isActive() ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
        m_shadowAnimation->setEasingCurve(window()->isActive() ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
        if (m_shadowAnimation->state() != QAbstractAnimation::Running) {
            m_shadowAnimation->start();
        }

    } else {
        updateShadow();
    }

    if (m_animation->duration() > 0) {
        m_animation->setDirection(window()->isActive() ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
        if (m_animation->state() != QAbstractAnimation::Running) {
            m_animation->start();
        }

    } else {
        update();
    }
}

//________________________________________________________________
qreal Decoration::borderSize(bool bottom, qreal scale) const
{
    const qreal pixelSize = KDecoration3::pixelSize(scale);
    const qreal baseSize = std::max<qreal>(pixelSize, KDecoration3::snapToPixelGrid(settings()->smallSpacing(), scale));
    if (m_internalSettings && (m_internalSettings->mask() & BorderSize)) {
        switch (m_internalSettings->borderSize()) {
        case InternalSettings::BorderNone:
            return 0;
        case InternalSettings::BorderNoSides:
            if (bottom) {
                return KDecoration3::snapToPixelGrid(std::max(4.0, baseSize + Metrics::Frame_FrameRadius), scale);
            } else {
                return 0;
            }
        default:
        case InternalSettings::BorderTiny:
            if (bottom) {
                return KDecoration3::snapToPixelGrid(std::max(4.0, baseSize), scale);
            } else {
                return baseSize;
            }
        case InternalSettings::BorderNormal:
            return baseSize * 2;
        case InternalSettings::BorderLarge:
            return baseSize * 3;
        case InternalSettings::BorderVeryLarge:
            return baseSize * 4;
        case InternalSettings::BorderHuge:
            return baseSize * 5;
        case InternalSettings::BorderVeryHuge:
            return baseSize * 6;
        case InternalSettings::BorderOversized:
            return baseSize * 10;
        }
    }
    // no exception: Raye windows have no border, whatever KWin's border size
    // setting says (see hasNoBorders())
    Q_UNUSED(baseSize)
    return 0;
}

//________________________________________________________________
void Decoration::reconfigure()
{
    m_internalSettings = SettingsProvider::self()->internalSettings(this);

    setScaledCornerRadius();

    // animation

    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    const KConfigGroup cg(config, QStringLiteral("KDE"));

    m_animation->setDuration(0);
    // Syncing anis between client and decoration is troublesome, so we're not using
    // any animations right now.
    // m_animation->setDuration( cg.readEntry("AnimationDurationFactor", 1.0f) * 100.0f );

    // But the shadow is fine to animate like this!
    m_shadowAnimation->setDuration(cg.readEntry("AnimationDurationFactor", 1.0f) * 167.0f);

    // borders
    recalculateBorders();

    // shadow
    updateShadow();
}

QMarginsF Decoration::bordersFor(qreal scale) const
{
    const qreal left = isLeftEdge() ? 0 : borderSize(false, scale);
    const qreal right = isRightEdge() ? 0 : borderSize(false, scale);
    const qreal bottom = (window()->isShaded() || isBottomEdge()) ? 0 : borderSize(true, scale);

    qreal top = 0;
    if (hideTitleBar()) {
        top = bottom;
    } else {
        // Windows' 32 px title bar; a very large title font still fits
        QFontMetrics fm(settings()->font());
        const qreal height = std::max<qreal>(DecorationMetrics::TitleBarHeight * unit(), fm.height() + 8 * unit());
        top = KDecoration3::snapToPixelGrid(m_tabletMode ? height * 1.5 : height, scale);
    }
    return QMarginsF(left, top, right, bottom);
}

void Decoration::recalculateBorders()
{
    setBorders(bordersFor(window()->nextScale()));

    // an invisible 8 px resize band outside the window (Windows)
    const qreal extSize = KDecoration3::snapToPixelGrid(8 * unit(), window()->nextScale());
    qreal extSides = 0;
    qreal extBottom = 0;
    if (hasNoBorders()) {
        if (!isMaximizedHorizontally()) {
            extSides = extSize;
        }
        if (!isMaximizedVertically()) {
            extBottom = extSize;
        }

    } else if (hasNoSideBorders() && !isMaximizedHorizontally()) {
        extSides = extSize;
    }

    setResizeOnlyBorders(QMarginsF(extSides, 0, extSides, extBottom));

    // bottom corners round like the top ones; KWin clips the window to them
    const auto c(corners());
    const bool roundBottom(hasNoBorders() && m_internalSettings->roundedCorners());
    setBorderRadius(KDecoration3::BorderRadius(0, 0, roundBottom ? c.bottomRight : 0, roundBottom ? c.bottomLeft : 0));

    if (isMaximized() || !outlinesEnabled()) {
        setBorderOutline(KDecoration3::BorderOutline());
    } else {
        // 1 px stroke.surface outline, or the accent at 70 % with the accent border on
        const bool accentBorder(m_internalSettings->accentBorder() && window()->isActive());
        const auto color = DecorationMetrics::outlineColor(isDark(),
                                                           window()->color(ColorGroup::Active, ColorRole::Foreground),
                                                           window()->palette().color(QPalette::Active, QPalette::Highlight),
                                                           accentBorder);
        const qreal thickness = std::max(KDecoration3::pixelSize(window()->nextScale()), KDecoration3::snapToPixelGrid(1, window()->nextScale()));
        const auto radius = KDecoration3::BorderRadius(c.topLeft, c.topRight, roundBottom ? c.bottomRight : 0, roundBottom ? c.bottomLeft : 0);
        setBorderOutline(KDecoration3::BorderOutline(thickness, color, radius));
    }
}

//________________________________________________________________
void Decoration::createButtons()
{
    m_leftButtons = new KDecoration3::DecorationButtonGroup(KDecoration3::DecorationButtonGroup::Position::Left, this, &Button::create);
    m_rightButtons = new KDecoration3::DecorationButtonGroup(KDecoration3::DecorationButtonGroup::Position::Right, this, &Button::create);
    updateButtonsGeometry();
}

//________________________________________________________________
void Decoration::updateButtonsGeometryDelayed()
{
    QTimer::singleShot(0, this, &Decoration::updateButtonsGeometry);
}

//________________________________________________________________
void Decoration::updateButtonsGeometry()
{
    // caption buttons are 46x32 and touch each other, the top edge and the
    // right edge of the window, like Windows'
    const auto buttonList = m_leftButtons->buttons() + m_rightButtons->buttons();
    for (KDecoration3::DecorationButton *button : buttonList) {
        auto btn = static_cast<Button *>(button);
        // as tall as the title bar, which grows only for very large fonts
        btn->setGeometry(QRectF(QPointF(0, 0), QSizeF(btn->preferredSize().width(), std::max<qreal>(borderTop(), btn->preferredSize().height()))));
        btn->setPadding(QMargins());
    }

    if (!m_leftButtons->buttons().isEmpty()) {
        m_leftButtons->setSpacing(0);
        m_leftButtons->setPos(QPointF(0, 0));
    }

    if (!m_rightButtons->buttons().isEmpty()) {
        m_rightButtons->setSpacing(0);
        m_rightButtons->setPos(QPointF(size().width() - m_rightButtons->geometry().width(), 0));
    }

    update();
}

//________________________________________________________________
void Decoration::paint(QPainter *painter, const QRectF &repaintRegion)
{
    auto s = settings();
    // a window with borders (window rule) gets them filled with the title bar colour
    if (!window()->isShaded() && hasBorders()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(titleBarColor());
        if (!hideTitleBar()) {
            painter->setClipRect(QRectF(0, borderTop(), size().width(), size().height() - borderTop()), Qt::IntersectClip);
        }
        painter->drawRect(rect());
        painter->restore();
    }

    if (!hideTitleBar()) {
        paintTitleBar(painter, repaintRegion);
    }

    if (hasBorders() && !s->isAlphaChannelSupported()) {
        const QColor borderColor = borderOutline().color();
        if (borderColor.alphaF() > 0) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, false);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(borderColor);
            painter->drawRect(rect().adjusted(0, 0, -1, -1));
            painter->restore();
        }
    }
}

//________________________________________________________________
void Decoration::paintTitleBar(QPainter *painter, const QRectF &repaintRegion)
{
    const QRectF rect(QPointF(0, 0), QSizeF(size().width(), borderTop()));
    if (!rect.intersects(repaintRegion)) {
        return;
    }

    // the title bar continues the window: same colour active or not (Mica)
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(titleBarColor());
    painter->drawPath(titleBarShape());
    painter->restore();

    // the window icon, 16 px at 12 px from the left, when no button sits there
    if (m_leftButtons->buttons().isEmpty() && !window()->icon().isNull()) {
        const qreal iconSize(DecorationMetrics::IconSize * unit());
        const QRectF iconRect(KDecoration3::snapToPixelGrid(DecorationMetrics::IconMargin * unit(), window()->scale()),
                              KDecoration3::snapToPixelGrid((borderTop() - iconSize) / 2, window()->scale()),
                              iconSize,
                              iconSize);
        window()->icon().paint(painter, iconRect.toRect(), Qt::AlignCenter);
    }

    // caption: 12 px Medium, left-aligned, elided at the end
    painter->setFont(settings()->font());
    painter->setPen(fontColor());
    const auto [captionRectangle, alignment] = captionRect();
    const QString caption = painter->fontMetrics().elidedText(window()->caption(), Qt::ElideRight, captionRectangle.width());
    painter->drawText(captionRectangle, alignment | Qt::TextSingleLine, caption);

    // draw all buttons
    m_leftButtons->paint(painter, repaintRegion);
    m_rightButtons->paint(painter, repaintRegion);
}

//________________________________________________________________
int Decoration::buttonSize() const
{
    const int baseSize = m_tabletMode ? settings()->gridUnit() * 2 : settings()->gridUnit();
    switch (m_internalSettings->buttonSize()) {
    case InternalSettings::ButtonTiny:
        return baseSize;
    case InternalSettings::ButtonSmall:
        return baseSize * 1.5;
    default:
    case InternalSettings::ButtonDefault:
        return baseSize * 2;
    case InternalSettings::ButtonLarge:
        return baseSize * 2.5;
    case InternalSettings::ButtonVeryLarge:
        return baseSize * 3.5;
    }
}

void Decoration::onTabletModeChanged(bool mode)
{
    m_tabletMode = mode;
    Q_EMIT tabletModeChanged();

    recalculateBorders();
    updateButtonsGeometry();
}

//________________________________________________________________
qreal Decoration::captionHeight() const
{
    return borderTop();
}

//________________________________________________________________
QPair<QRectF, Qt::Alignment> Decoration::captionRect() const
{
    if (hideTitleBar()) {
        return qMakePair(QRectF(), Qt::AlignCenter);
    }

    // after the icon (or the left buttons) by 12 px, up to the caption buttons
    const qreal margin(DecorationMetrics::IconMargin * unit());
    qreal leftOffset = margin;
    if (!m_leftButtons->buttons().isEmpty()) {
        leftOffset = m_leftButtons->geometry().x() + m_leftButtons->geometry().width() + margin;
    } else if (!window()->icon().isNull()) {
        leftOffset = margin + DecorationMetrics::IconSize * unit() + margin;
    }
    const qreal rightOffset = m_rightButtons->buttons().isEmpty() ? margin : size().width() - m_rightButtons->geometry().x() + margin / 2;
    leftOffset = KDecoration3::snapToPixelGrid(leftOffset, window()->scale());

    const QRectF maxRect(leftOffset, 0, std::max<qreal>(0, size().width() - leftOffset - rightOffset), captionHeight());

    switch (m_internalSettings->titleAlignment()) {
    case InternalSettings::AlignRight:
        return qMakePair(maxRect, Qt::AlignVCenter | Qt::AlignRight);
    case InternalSettings::AlignCenter:
    case InternalSettings::AlignCenterFullWidth:
        return qMakePair(maxRect, Qt::AlignCenter);
    case InternalSettings::AlignLeft:
    default:
        return qMakePair(maxRect, Qt::AlignVCenter | Qt::AlignLeft);
    }
}

//________________________________________________________________
void Decoration::updateShadow()
{
    // Animated case, no cached shadow object
    if ((m_shadowAnimation->state() == QAbstractAnimation::Running) && (m_shadowOpacity != 0.0) && (m_shadowOpacity != 1.0)) {
        setShadow(createShadowObject(m_shadowOpacity));
        return;
    }

    if (g_shadowSizeEnum != m_internalSettings->shadowSize() || g_shadowStrength != m_internalSettings->shadowStrength()
        || g_shadowColor != m_internalSettings->shadowColor() || g_shadowDark != isDark() || g_shadowRadius != m_scaledCornerRadius) {
        g_sShadow.reset();
        g_sShadowInactive.reset();
        g_shadowSizeEnum = m_internalSettings->shadowSize();
        g_shadowStrength = m_internalSettings->shadowStrength();
        g_shadowColor = m_internalSettings->shadowColor();
        g_shadowDark = isDark();
        g_shadowRadius = m_scaledCornerRadius;
    }

    // larger on the active window, smaller on the others (Windows)
    if (!g_sShadow) {
        g_sShadow = createShadowObject(1.0);
        g_sShadowInactive = createShadowObject(0.0);
    }
    setShadow(window()->isActive() ? g_sShadow : g_sShadowInactive);
}

//________________________________________________________________
std::shared_ptr<KDecoration3::DecorationShadow> Decoration::createShadowObject(qreal activeness)
{
    const qreal sizeFactor(shadowSizeFactor(m_internalSettings->shadowSize()));
    if (sizeFactor <= 0) {
        // If shadows are disabled, return nothing
        return nullptr;
    }

    const auto spec = DecorationMetrics::shadowFor(isDark(), activeness, m_internalSettings->shadowStrength() / 255.0);
    const int radius = qMax(1, qRound(spec.radius * sizeFactor * unit()));
    const QPoint offset(0, qRound(spec.offsetY * sizeFactor * unit()));

    // the shadow colour: black on night, the dark ink on day, unless the
    // user picked another colour
    QColor color(m_internalSettings->shadowColor());
    if (color == QColor(Qt::black) && !isDark()) {
        color = QColor(0x1A, 0x10, 0x28);
    }
    auto withOpacity = [](QColor c, qreal opacity) -> QColor {
        c.setAlphaF(qBound(0.0, opacity, 1.0));
        return c;
    };

    const QSize boxSize = BoxShadowRenderer::calculateMinimumBoxSize(radius);

    BoxShadowRenderer shadowRenderer;
    shadowRenderer.setBorderRadius(m_scaledCornerRadius + 0.5);
    shadowRenderer.setBoxSize(boxSize);
    shadowRenderer.addShadow(offset, radius, withOpacity(color, spec.opacity));
    // a tight contact shadow keeps the edge readable on any wallpaper
    shadowRenderer.addShadow(QPoint(0, qRound(unit())), qMax(1, qRound(4 * unit())), withOpacity(color, spec.opacity * 0.4));

    QImage shadowTexture = shadowRenderer.render();

    QPainter painter(&shadowTexture);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF outerRect = shadowTexture.rect();

    QRectF boxRect(QPoint(0, 0), boxSize);
    boxRect.moveCenter(outerRect.center());

    // Mask out inner rect.
    const QMarginsF padding = QMarginsF(boxRect.left() - outerRect.left() - Metrics::Shadow_Overlap - offset.x(),
                                        boxRect.top() - outerRect.top() - Metrics::Shadow_Overlap - offset.y(),
                                        outerRect.right() - boxRect.right() - Metrics::Shadow_Overlap + offset.x(),
                                        outerRect.bottom() - boxRect.bottom() - Metrics::Shadow_Overlap + offset.y());
    QRectF innerRect = outerRect - padding;
    // Push the shadow slightly under the window, which helps avoiding glitches with fractional scaling
    innerRect.adjust(2, 2, -2, -2);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.drawRoundedRect(innerRect, m_scaledCornerRadius + 0.5, m_scaledCornerRadius + 0.5);

    painter.end();

    auto ret = std::make_shared<KDecoration3::DecorationShadow>();
    ret->setPadding(padding);
    ret->setInnerShadowRect(QRectF(outerRect.center(), QSizeF(1, 1)));
    ret->setShadow(shadowTexture);
    return ret;
}

void Decoration::setScaledCornerRadius()
{
    // On X11, the smallSpacing value is used for scaling.
    // On Wayland, this value has constant factor of 2.
    // Removing it will break radius scaling on X11.
    m_scaledCornerRadius = KDecoration3::snapToPixelGrid(Metrics::Frame_FrameRadius * settings()->smallSpacing(), window()->nextScale());
}

void Decoration::updateScale()
{
    setScaledCornerRadius();
    recalculateBorders();
}
} // namespace

#include "rayedecoration.moc"
