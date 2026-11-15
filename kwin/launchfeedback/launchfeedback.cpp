#include "launchfeedback.h"

#include <core/output.h>
#include <core/pixelgrid.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <effect/effecthandler.h>
#include <opengl/glutils.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QDir>
#include <QStandardPaths>

namespace KWin
{

static QStringList iconRoots()
{
    QStringList roots{QDir::homePath() + QStringLiteral("/.icons")};
    for (const QString &d : QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons"),
                                                       QStandardPaths::LocateDirectory)) {
        roots << d;
    }
    return roots;
}

LaunchFeedbackEffect::LaunchFeedbackEffect()
    : m_config(KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("klaunchrc"), KConfig::NoGlobals)))
{
    connect(effects, &EffectsHandler::startupAdded, this, [this](const QString &id, const QIcon &) {
        added(id);
    });
    connect(effects, &EffectsHandler::startupRemoved, this, &LaunchFeedbackEffect::removed);
    connect(m_config.data(), &KConfigWatcher::configChanged, this, [this] {
        reconfigure(ReconfigureAll);
    });
    reconfigure(ReconfigureAll);

    // nothing next to the pointer while the sign-in splash is up
    auto bus = QDBusConnection::sessionBus();
    m_splash = bus.interface() && bus.interface()->isServiceRegistered(QStringLiteral("org.kde.KSplash"));
    auto watcher = new QDBusServiceWatcher(QStringLiteral("org.kde.KSplash"), bus,
                                           QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] {
        m_splash = true;
        stop();
    });
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        m_splash = false;
        if (!m_startups.isEmpty()) {
            start();
        }
    });
}

LaunchFeedbackEffect::~LaunchFeedbackEffect()
{
    stop();
}

bool LaunchFeedbackEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void LaunchFeedbackEffect::reconfigure(ReconfigureFlags)
{
    // the same timeout as the taskbar button: [BusyCursorSettings] Timeout
    const KConfigGroup c = m_config->config()->group(QStringLiteral("BusyCursorSettings"));
    m_timeout = std::chrono::seconds(qBound(1, c.readEntry("Timeout", 10), 60));
}

void LaunchFeedbackEffect::added(const QString &id)
{
    if (effects->isCursorHidden()) {
        return;
    }
    auto timer = std::make_shared<QTimer>();
    timer->setSingleShot(true);
    connect(timer.get(), &QTimer::timeout, this, [this, id] {
        removed(id);
    });
    timer->start(m_timeout);
    m_startups.insert(id, timer);
    start();
}

void LaunchFeedbackEffect::removed(const QString &id)
{
    if (m_startups.remove(id) && m_startups.isEmpty()) {
        stop();
    }
}

void LaunchFeedbackEffect::start()
{
    if (m_active || m_splash || effects->isCursorHidden()) {
        return;
    }
    const LogicalOutput *output = effects->screenAt(effects->cursorPos().toPoint());
    if (!output) {
        return;
    }
    const KConfigGroup mouse(effects->inputConfig(), QStringLiteral("Mouse"));
    const QString theme = mouse.readEntry("cursorTheme", QStringLiteral("Raye"));
    const int size = mouse.readEntry("cursorSize", 24);
    m_ring = RingFrames();
    for (const QString &dir : ringDirs(theme, iconRoots())) {
        m_ring = loadRing(dir, size, output->scale());
        if (!m_ring.isEmpty()) {
            break;
        }
    }
    if (m_ring.isEmpty()) {
        return;
    }
    effects->makeOpenGLContextCurrent();
    m_textures.clear();
    for (const QImage &img : std::as_const(m_ring.images)) {
        auto t = GLTexture::upload(img);
        if (!t) {
            m_textures.clear();
            return;
        }
        t->setFilter(GL_LINEAR);
        t->setWrapMode(GL_CLAMP_TO_EDGE);
        m_textures.push_back(std::move(t));
    }
    m_active = true;
    m_frame = 0;
    m_elapsed = 0;
    m_last = std::chrono::milliseconds::zero();
    m_geometry = geometry();
    effects->addRepaint(m_geometry);
}

void LaunchFeedbackEffect::stop()
{
    if (!m_active) {
        return;
    }
    m_active = false;
    effects->addRepaint(m_geometry);
    effects->makeOpenGLContextCurrent();
    m_textures.clear();
}

QRect LaunchFeedbackEffect::geometry() const
{
    const QPointF topLeft = effects->cursorPos() - m_ring.hotspot;
    return QRectF(topLeft, QSizeF(m_ring.size, m_ring.size)).toAlignedRect();
}

void LaunchFeedbackEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_active && effects->isCursorHidden()) {
        stop();
    }
    if (m_active) {
        if (m_last.count()) {
            m_elapsed += int((presentTime - m_last).count());
        }
        m_last = presentTime;
        while (m_elapsed >= m_ring.delays.value(m_frame, 40)) {
            m_elapsed -= m_ring.delays.value(m_frame, 40);
            m_frame = (m_frame + 1) % int(m_textures.size());
        }
        const QRect now = geometry();
        data.paint = data.paint.united(Rect(m_geometry)).united(Rect(now));
        m_geometry = now;
    }
    effects->prePaintScreen(data, presentTime);
}

void LaunchFeedbackEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask,
                                       const Region &deviceRegion, LogicalOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    if (!m_active || m_textures.empty()) {
        return;
    }
    GLTexture *texture = m_textures[m_frame].get();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // the frames are premultiplied
    GLShader *shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
    const QPointF topLeft = effects->cursorPos() - m_ring.hotspot;
    const QRectF pixels = snapToPixelGridF(scaledRect(QRectF(topLeft, QSizeF(m_ring.size, m_ring.size)), viewport.scale()));
    QMatrix4x4 mvp = viewport.projectionMatrix();
    mvp.translate(pixels.x(), pixels.y());
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    texture->render(pixels.size());
    ShaderManager::instance()->popShader();
    glDisable(GL_BLEND);
}

void LaunchFeedbackEffect::postPaintScreen()
{
    if (m_active) {
        // keep the ring turning
        effects->addRepaint(m_geometry);
    }
    effects->postPaintScreen();
}

bool LaunchFeedbackEffect::isActive() const
{
    return m_active;
}

} // namespace KWin

#include "moc_launchfeedback.cpp"
