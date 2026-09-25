#pragma once

#include "ads.h"
#include "head_transform.h"

#include "cameraunlock/camera/zoom_compensation.h"

namespace RedEclipseHeadTracking {

// Cube's world is 8 units to the metre: the engine's own distance readout
// divides by 8 to print metres.
constexpr float kUnitsPerMetre = 8.0f;

// The tracker's pose in HeadPose's convention and in world units, scaled so a
// zoom does not magnify it. BuildHeadTransform then maps HeadPose into Cube's
// camera space, negating roll and z once more on the way.
//
// HeadPose runs opposite to the tracker on yaw, roll, x and z, so those four
// are negated here, and 8 world units make a metre. Correcting x and z here
// rather than through the processor's position inversion keeps the asymmetric
// z limits pointing the way they are documented: the generous PositionLimitZ
// on leaning forward, the restricted PositionLimitZBack on leaning back. Those
// are clamped before the pose reaches here.
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
