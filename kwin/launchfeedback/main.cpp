#include "launchfeedback.h"

namespace KWin
{
KWIN_EFFECT_FACTORY_SUPPORTED(LaunchFeedbackEffect, "metadata.json", return LaunchFeedbackEffect::supported();)
}

#include "main.moc"
