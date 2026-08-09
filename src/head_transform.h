#pragma once

#include "engine_types.h"

#include <cmath>

namespace RedEclipseHeadTracking {

// One frame of head movement in Cube's units: rotation in degrees, translation
// already converted from the tracker's metres to world units and already
// mapped onto the camera's axes - +x right, +y up, +z forward.
struct HeadPose {
    float yaw_deg = 0.0f;
    float pitch_deg = 0.0f;
    float roll_deg = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Builds H = R^-1 * T(-t): the inverse of "move the camera by t, then rotate it
// by R", both expressed in the clean camera's own frame. Left-multiplying H
// onto the engine's view matrix is what puts the player's head movement on
// screen without the game ever seeing it.
//
// Cube's camera space is the usual OpenGL one: +x right, +y up, -z forward.
// R composes as yaw * pitch * roll with roll innermost, so roll happens about
// the final view axis - the same order the engine itself applies camera1's
// yaw/pitch/roll in setcammatrix.
//
// The translation multiplies in on the right of the rotation, so the offset is
// expressed in the ORIGINAL view space. Leaning left moves the viewpoint along
// the body's left, not along wherever the head happens to be facing.
inline EngMat4 BuildHeadTransform(const HeadPose& pose, const EngMat4& cleanView, bool worldSpaceYaw) {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

    const float yaw = pose.yaw_deg * kDegToRad;
    const float pitch = pose.pitch_deg * kDegToRad;
    const float roll = -pose.roll_deg * kDegToRad;

    // Yaw axis: camera up for camera-local yaw, or the world's up axis as seen
    // in camera space for horizon-locked yaw. Cube's world is Z-up and the
    // clean view matrix maps a world direction into camera space, so world +z
    // lands in its third column. That 3x3 block is orthonormal, so the result
    // is already a unit vector.
    float ax = 0.0f, ay = 1.0f, az = 0.0f;
    if (worldSpaceYaw) {
        ax = cleanView.c.x;
        ay = cleanView.c.y;
        az = cleanView.c.z;
    }

    // Rotation about an arbitrary unit axis (Rodrigues).
    const float cy = std::cos(yaw), sy = std::sin(yaw), ty = 1.0f - cy;
    const float yawRot[3][3] = {
        {ty * ax * ax + cy, ty * ax * ay - sy * az, ty * ax * az + sy * ay},
        {ty * ax * ay + sy * az, ty * ay * ay + cy, ty * ay * az - sy * ax},
        {ty * ax * az - sy * ay, ty * ay * az + sy * ax, ty * az * az + cy},
    };

    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float pitchRot[3][3] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, cp, -sp},
        {0.0f, sp, cp},
    };

    const float cr = std::cos(roll), sr = std::sin(roll);
    const float rollRot[3][3] = {
        {cr, -sr, 0.0f},
        {sr, cr, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };

    float pitchRoll[3][3];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            pitchRoll[r][c] = pitchRot[r][0] * rollRot[0][c] + pitchRot[r][1] * rollRot[1][c] +
                              pitchRot[r][2] * rollRot[2][c];
        }
    }

    float rot[3][3];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            rot[r][c] = yawRot[r][0] * pitchRoll[0][c] + yawRot[r][1] * pitchRoll[1][c] +
                        yawRot[r][2] * pitchRoll[2][c];
        }
    }

    // The pose's forward is +z, which is camera -z.
    const float tx = pose.x, tyv = pose.y, tz = -pose.z;

    // H's rotation block is transpose(rot); its translation column is
    // -transpose(rot) * t.
    EngMat4 h;
    h.a = EngVec4{rot[0][0], rot[0][1], rot[0][2], 0.0f};
    h.b = EngVec4{rot[1][0], rot[1][1], rot[1][2], 0.0f};
    h.c = EngVec4{rot[2][0], rot[2][1], rot[2][2], 0.0f};
    h.d = EngVec4{
        -(rot[0][0] * tx + rot[1][0] * tyv + rot[2][0] * tz),
        -(rot[0][1] * tx + rot[1][1] * tyv + rot[2][1] * tz),
        -(rot[0][2] * tx + rot[1][2] * tyv + rot[2][2] * tz),
        1.0f,
    };
    return h;
}

// camdir/camright/camup are the world-space camera axes the engine derives from
// cammatrix at the end of setcammatrix. Rebuilding them from the tracked matrix
// keeps particle billboards, lens flares and audio panning aligned with what is
// actually on screen. Mirrors setcammatrix's own transposedtransformnormal
// calls against the fixed viewmatrix basis.
inline void CameraAxesFromView(const EngMat4& view, EngVec& dir, EngVec& right, EngVec& up) {
    dir = EngVec{-view.a.z, -view.b.z, -view.c.z};
    right = EngVec{view.a.x, view.b.x, view.c.x};
    up = EngVec{view.a.y, view.b.y, view.c.y};
}

// Projects a world point to normalised cursor coordinates (x right, y down,
// 0.5/0.5 = screen centre) through a view-projection matrix. Returns false when
// the point is behind the near plane. This is the engine's own vectocursor
// maths, so a crosshair drawn at the result lands exactly where the game would
// have drawn it for that point.
inline bool ProjectToCursor(const EngMat4& viewProj, const EngVec& world, float& x, float& y) {
    const float cx = viewProj.a.x * world.x + viewProj.b.x * world.y + viewProj.c.x * world.z + viewProj.d.x;
    const float cy = viewProj.a.y * world.x + viewProj.b.y * world.y + viewProj.c.y * world.z + viewProj.d.y;
    const float cz = viewProj.a.z * world.x + viewProj.b.z * world.y + viewProj.c.z * world.z + viewProj.d.z;
    const float cw = viewProj.a.w * world.x + viewProj.b.w * world.y + viewProj.c.w * world.z + viewProj.d.w;

    if (cz <= -cw) {
        x = y = 0.0f;
        return false;
    }
    x = cx / cw * 0.5f + 0.5f;
    y = 0.5f - cy / cw * 0.5f;
    return true;
}

}  // namespace RedEclipseHeadTracking
