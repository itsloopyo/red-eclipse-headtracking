#pragma once

#include "cameraunlock/ads/ads_fade.h"

namespace RedEclipseHeadTracking {

struct TrackedPose {
    float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

// Red Eclipse fades the first-person model out as the zoom comes in and puts
// the zoom crosshair where the aim lands, so rotation needs nothing here. A lean
// is eased out while zoomed so the scope is looked through from the clean eye.
class AdsLean {
public:
    TrackedPose Apply(bool aiming, unsigned long long nowMs, TrackedPose pose) {
        const float scale = m_fade.Update(aiming, nowMs);
        pose.x *= scale;
        pose.y *= scale;
        pose.z *= scale;
        return pose;
    }

    void Reset() { m_fade.Reset(); }

private:
    cameraunlock::ads::AdsFade m_fade;
};

}
