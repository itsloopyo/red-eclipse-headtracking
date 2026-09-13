#pragma once

#include "cameraunlock/ads/ads_blend.h"
#include "cameraunlock/ads/ads_fade.h"

namespace RedEclipseHeadTracking {

class AdsState {
public:
    using Pose = cameraunlock::ads::AdsEntryPose::Pose;
    using Mode = cameraunlock::ads::AdsMode;

    Pose Update(bool active, bool aiming, bool live, Mode mode,
                unsigned long long nowMs, const Pose& absolute) {
        if (!active) {
            Reset();
            return {};
        }
        if (mode != m_mode) {
            Reset();
            m_mode = mode;
        }
        const float scale = m_fade.Update(aiming, nowMs);
        // Keep the entry through the return fade, including interrupted aims.
        const Pose relative = m_entry.Relative(aiming || scale < 1.0f, live, absolute);
        return cameraunlock::ads::BlendAdsPose(mode, scale, absolute, relative);
    }

    void Reset() {
        m_fade.Reset();
        m_entry.Reset();
    }

private:
    cameraunlock::ads::AdsFade m_fade;
    cameraunlock::ads::AdsEntryPose m_entry;
    Mode m_mode = cameraunlock::ads::kDefaultAdsMode;
};

}
