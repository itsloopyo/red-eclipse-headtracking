#pragma once

#include "ads.h"
#include "head_transform.h"

#include "cameraunlock/camera/zoom_compensation.h"

namespace RedEclipseHeadTracking {

// Cube's world is 8 units to the metre: the engine's own distance readout
// divides by 8 to print metres.
constexpr float kUnitsPerMetre = 8.0f;

// The tracker's pose on Cube's camera axes and in world units, scaled so a zoom
// does not magnify it. This is the whole of the conversion between the two
// conventions, and the only place any axis is negated.
//
// The head transform is built with right-handed rotations about Cube's camera
// axes, which run opposite to the tracker on yaw and roll. The tracker's x and
// z run opposite to Cube's camera axes too. Correcting those here rather than
// through the processor's position inversion keeps the asymmetric z limits
// pointing the way they are documented: the generous PositionLimitZ on leaning
// forward, the restricted PositionLimitZBack on leaning back. Those are clamped
// before the pose reaches here.
inline HeadPose ToEnginePose(const TrackedPose& tracked, bool hasPosition, float zoom) {
    HeadPose pose;
    pose.yaw_deg = cameraunlock::camera::ScaleAngleForZoom(-tracked.yaw, zoom);
    pose.pitch_deg = cameraunlock::camera::ScaleAngleForZoom(tracked.pitch, zoom);
    pose.roll_deg = -tracked.roll;
    if (hasPosition) {
        pose.x = -tracked.x * kUnitsPerMetre * zoom;
        pose.y = tracked.y * kUnitsPerMetre * zoom;
        pose.z = -tracked.z * kUnitsPerMetre * zoom;
    }
    return pose;
}

}  // namespace RedEclipseHeadTracking
