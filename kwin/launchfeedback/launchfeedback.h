#pragma once

#include "ringframes.h"

#include <effect/effect.h>

#include <KConfigWatcher>
#include <QHash>
#include <QIcon>
#include <QPointer>
#include <QTimer>
#include <chrono>
#include <memory>
#include <vector>

namespace KWin
{
class GLTexture;

class LaunchFeedbackEffect : public Effect
{
    Q_OBJECT
public:
    LaunchFeedbackEffect();
    ~LaunchFeedbackEffect() override;

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask,
                     const Region &deviceRegion, LogicalOutput *screen) override;
    void postPaintScreen() override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override
    {
        return 90;
    }

    static bool supported();

private:
    void added(const QString &id);
    void removed(const QString &id);
    void start();
    void stop();
    QRect geometry() const;

    QHash<QString, std::shared_ptr<QTimer>> m_startups;
    KConfigWatcher::Ptr m_config;
    std::chrono::seconds m_timeout{10};
    RingFrames m_ring;
    std::vector<std::unique_ptr<GLTexture>> m_textures;
    bool m_active = false;
    bool m_splash = false;
    int m_frame = 0;
    int m_elapsed = 0;
    std::chrono::milliseconds m_last{0};
    QRect m_geometry;
};

} // namespace KWin
