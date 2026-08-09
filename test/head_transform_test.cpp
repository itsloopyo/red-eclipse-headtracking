// Host-side checks for the head transform and crosshair projection.
//
// The reference view matrix here is built exactly the way the engine builds it
// in setcammatrix (engine/rendergl.cpp), so a transform that satisfies these
// checks satisfies them against the real camera too. The four reticle litmus
// tests from the head-tracking doctrine are the last four cases.

#include "head_transform.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace RedEclipseHeadTracking;

namespace {

int g_failures = 0;

void Check(bool condition, const std::string& what) {
    if (!condition) {
        std::printf("  FAIL: %s\n", what.c_str());
        g_failures++;
    }
}

void CheckNear(float actual, float expected, float tolerance, const std::string& what) {
    if (!(std::fabs(actual - expected) <= tolerance)) {
        std::printf("  FAIL: %s (got %.6f, expected %.6f +/- %.6f)\n", what.c_str(), actual,
                    expected, tolerance);
        g_failures++;
    }
}

constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

// engine/rendergl.cpp: extern const matrix4 viewmatrix(vec(-1,0,0), vec(0,0,1), vec(0,-1,0));
// The matrix4(vec a, vec b, vec c) constructor lays those out as ROWS of the
// 3x3 block, which in column storage gives the columns below.
EngMat4 ViewMatrix() {
    return EngMat4{
        {-1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
}

void RotateAroundX(EngMat4& m, float angle) {
    const float ck = std::cos(angle), sk = std::sin(angle);
    const EngVec4 b = m.b, c = m.c;
    m.b = EngVec4{b.x * ck + c.x * sk, b.y * ck + c.y * sk, b.z * ck + c.z * sk, b.w * ck + c.w * sk};
    m.c = EngVec4{c.x * ck - b.x * sk, c.y * ck - b.y * sk, c.z * ck - b.z * sk, c.w * ck - b.w * sk};
}

void RotateAroundY(EngMat4& m, float angle) {
    const float ck = std::cos(angle), sk = std::sin(angle);
    const EngVec4 a = m.a, c = m.c;
    m.c = EngVec4{c.x * ck + a.x * sk, c.y * ck + a.y * sk, c.z * ck + a.z * sk, c.w * ck + a.w * sk};
    m.a = EngVec4{a.x * ck - c.x * sk, a.y * ck - c.y * sk, a.z * ck - c.z * sk, a.w * ck - c.w * sk};
}

void RotateAroundZ(EngMat4& m, float angle) {
    const float ck = std::cos(angle), sk = std::sin(angle);
    const EngVec4 a = m.a, b = m.b;
    m.a = EngVec4{a.x * ck + b.x * sk, a.y * ck + b.y * sk, a.z * ck + b.z * sk, a.w * ck + b.w * sk};
    m.b = EngVec4{b.x * ck - a.x * sk, b.y * ck - a.y * sk, b.z * ck - a.z * sk, b.w * ck - a.w * sk};
}

void Translate(EngMat4& m, float x, float y, float z) {
    m.d = EngVec4{
        m.d.x + m.a.x * x + m.b.x * y + m.c.x * z,
        m.d.y + m.a.y * x + m.b.y * y + m.c.y * z,
        m.d.z + m.a.z * x + m.b.z * y + m.c.z * z,
        m.d.w + m.a.w * x + m.b.w * y + m.c.w * z,
    };
}

// The engine's setcammatrix, verbatim:
//   cammatrix = viewmatrix;
//   cammatrix.rotate_around_y(camera1->roll*RAD);
//   cammatrix.rotate_around_x(camera1->pitch*-RAD);
//   cammatrix.rotate_around_z(camera1->yaw*-RAD);
//   cammatrix.translate(vec(camera1->o).neg());
EngMat4 CleanView(float yawDeg, float pitchDeg, float rollDeg, EngVec origin) {
    EngMat4 m = ViewMatrix();
    RotateAroundY(m, rollDeg * kDegToRad);
    RotateAroundX(m, -pitchDeg * kDegToRad);
    RotateAroundZ(m, -yawDeg * kDegToRad);
    Translate(m, -origin.x, -origin.y, -origin.z);
    return m;
}

// Standard OpenGL perspective, matching matrix4::perspective.
EngMat4 Perspective(float fovyDeg, float aspect, float zNear, float zFar) {
    const float f = 1.0f / std::tan(fovyDeg * 0.5f * kDegToRad);
    EngMat4 p{};
    p.a = EngVec4{f / aspect, 0.0f, 0.0f, 0.0f};
    p.b = EngVec4{0.0f, f, 0.0f, 0.0f};
    p.c = EngVec4{0.0f, 0.0f, (zFar + zNear) / (zNear - zFar), -1.0f};
    p.d = EngVec4{0.0f, 0.0f, 2.0f * zFar * zNear / (zNear - zFar), 0.0f};
    return p;
}

EngVec TransformPoint(const EngMat4& m, const EngVec& v) {
    return EngVec{
        m.a.x * v.x + m.b.x * v.y + m.c.x * v.z + m.d.x,
        m.a.y * v.x + m.b.y * v.y + m.c.y * v.z + m.d.y,
        m.a.z * v.x + m.b.z * v.y + m.c.z * v.z + m.d.z,
    };
}

// The world point the player's clean aim ray hits, `distance` units ahead. Cube
// yaw 0 faces +y; pitch lifts toward +z.
EngVec AimPoint(const EngVec& origin, float yawDeg, float pitchDeg, float distance) {
    const float yaw = yawDeg * kDegToRad, pitch = pitchDeg * kDegToRad;
    return EngVec{
        origin.x - std::sin(yaw) * std::cos(pitch) * distance,
        origin.y + std::cos(yaw) * std::cos(pitch) * distance,
        origin.z + std::sin(pitch) * distance,
    };
}

struct Scene {
    EngMat4 cleanView;
    EngMat4 projection;
    EngVec origin;
    float yaw, pitch;
};

Scene MakeScene(float yawDeg, float pitchDeg) {
    Scene s;
    s.origin = EngVec{512.0f, 512.0f, 64.0f};
    s.yaw = yawDeg;
    s.pitch = pitchDeg;
    s.cleanView = CleanView(yawDeg, pitchDeg, 0.0f, s.origin);
    s.projection = Perspective(60.0f, 16.0f / 9.0f, 0.54f, 10000.0f);
    return s;
}

// Runs a pose through the whole chain the mod runs it through in game and
// reports where the crosshair lands.
bool CrosshairFor(const Scene& scene, const HeadPose& pose, bool worldSpaceYaw, float& x, float& y) {
    const EngMat4 h = BuildHeadTransform(pose, scene.cleanView, worldSpaceYaw);
    const EngMat4 trackedView = Multiply(h, scene.cleanView);
    const EngMat4 viewProj = Multiply(scene.projection, trackedView);
    const EngVec aim = AimPoint(scene.origin, scene.yaw, scene.pitch, 500.0f);
    return ProjectToCursor(viewProj, aim, x, y);
}

void TestCleanViewMatchesEngineConventions() {
    std::printf("clean view matrix conventions\n");
    const Scene scene = MakeScene(0.0f, 0.0f);

    // A point straight ahead of the camera must land on the camera's -z axis.
    const EngVec ahead = AimPoint(scene.origin, 0.0f, 0.0f, 100.0f);
    const EngVec inCamera = TransformPoint(scene.cleanView, ahead);
    CheckNear(inCamera.x, 0.0f, 1e-3f, "forward point has no camera-space x");
    CheckNear(inCamera.y, 0.0f, 1e-3f, "forward point has no camera-space y");
    CheckNear(inCamera.z, -100.0f, 1e-2f, "forward point sits on camera -z");

    // World up (+z) must map to camera up (+y), which is what horizon-locked
    // yaw relies on when it reads the view matrix's third column.
    CheckNear(scene.cleanView.c.x, 0.0f, 1e-5f, "world up has no camera x at zero pitch");
    CheckNear(scene.cleanView.c.y, 1.0f, 1e-5f, "world up is camera up at zero pitch");
    CheckNear(scene.cleanView.c.z, 0.0f, 1e-5f, "world up has no camera z at zero pitch");
}

void TestIdentityWhenStill() {
    std::printf("no head movement leaves the view untouched\n");
    const Scene scene = MakeScene(35.0f, -12.0f);
    const EngMat4 h = BuildHeadTransform(HeadPose{}, scene.cleanView, true);
    const EngMat4 tracked = Multiply(h, scene.cleanView);

    const float* a = &tracked.a.x;
    const float* b = &scene.cleanView.a.x;
    for (int i = 0; i < 16; ++i) {
        CheckNear(a[i], b[i], 1e-5f, "tracked view element " + std::to_string(i));
    }

    float x = 0.0f, y = 0.0f;
    Check(CrosshairFor(scene, HeadPose{}, true, x, y), "crosshair projects");
    CheckNear(x, 0.5f, 1e-4f, "crosshair centred horizontally");
    CheckNear(y, 0.5f, 1e-4f, "crosshair centred vertically");
}

void TestYawMovesTheWorldOppositeTheHead() {
    std::printf("yaw swings the view, leaving the aim point behind\n");
    // Raw geometry: a positive yaw_deg here turns the view to its left. The
    // tracker's own sign is reconciled by config's InvertYaw default, so in
    // game a head turn to the left produces this case.
    const Scene scene = MakeScene(0.0f, 0.0f);

    HeadPose pose;
    pose.yaw_deg = 20.0f;
    const EngMat4 h = BuildHeadTransform(pose, scene.cleanView, false);
    const EngMat4 tracked = Multiply(h, scene.cleanView);

    // The point the player is aiming at should now sit to the RIGHT of the
    // tracked view: they have looked left, so their aim is off to their right.
    const EngVec aim = AimPoint(scene.origin, 0.0f, 0.0f, 500.0f);
    const EngVec inCamera = TransformPoint(tracked, aim);
    Check(inCamera.x > 0.0f, "aim point moves to camera +x when the head yaws left");
    CheckNear(inCamera.y, 0.0f, 1e-2f, "pure yaw does not move the aim point vertically");

    float x = 0.0f, y = 0.0f;
    Check(CrosshairFor(scene, pose, false, x, y), "crosshair projects");
    Check(x > 0.5f, "crosshair moves right of centre when the head yaws left");
    CheckNear(y, 0.5f, 1e-4f, "pure yaw keeps the crosshair on the horizon");
}

void TestPitchMovesTheViewUp() {
    std::printf("pitch swings the view vertically, leaving the aim point behind\n");
    // Pitch is the one rotation whose sign already agrees with the tracker, so
    // InvertPitch defaults to false and positive here means the head looks up.
    const Scene scene = MakeScene(0.0f, 0.0f);

    HeadPose pose;
    pose.pitch_deg = 15.0f;
    float x = 0.0f, y = 0.0f;
    Check(CrosshairFor(scene, pose, false, x, y), "crosshair projects");
    // Looking up puts the aim point lower on screen; cursor y grows downward.
    Check(y > 0.5f, "crosshair moves down the screen when the head pitches up");
    CheckNear(x, 0.5f, 1e-4f, "pure pitch does not move the crosshair sideways");
}

void TestLeanShiftsTheViewpoint() {
    std::printf("leaning moves the viewpoint, not the aim\n");
    const Scene scene = MakeScene(0.0f, 0.0f);

    HeadPose pose;
    pose.x = 2.0f;  // world units, i.e. 0.25 m at Cube's 8 units per metre
    const EngMat4 h = BuildHeadTransform(pose, scene.cleanView, false);
    const EngMat4 tracked = Multiply(h, scene.cleanView);

    const EngVec inCamera = TransformPoint(tracked, scene.origin);
    CheckNear(inCamera.x, -2.0f, 1e-4f, "leaning right shifts the old viewpoint to camera -x");

    // Rotation is untouched by a pure lean.
    CheckNear(tracked.a.x, scene.cleanView.a.x, 1e-5f, "lean leaves the view rotation alone");
    CheckNear(tracked.b.y, scene.cleanView.b.y, 1e-5f, "lean leaves the view rotation alone");
}

// --- Doctrine reticle litmus tests -----------------------------------------

void TestLitmusPureRollKeepsCrosshairCentred() {
    std::printf("litmus 1: pure roll keeps the crosshair centred\n");
    for (float roll : {-30.0f, -10.0f, 10.0f, 30.0f}) {
        const Scene scene = MakeScene(17.0f, 0.0f);
        HeadPose pose;
        pose.roll_deg = roll;
        float x = 0.0f, y = 0.0f;
        Check(CrosshairFor(scene, pose, false, x, y), "crosshair projects");
        CheckNear(x, 0.5f, 1e-4f, "roll " + std::to_string(roll) + " keeps x centred");
        CheckNear(y, 0.5f, 1e-4f, "roll " + std::to_string(roll) + " keeps y centred");
    }
}

void TestLitmusPurePitchMovesVerticallyOnly() {
    std::printf("litmus 2: pure pitch moves the crosshair vertically only\n");
    for (float pitch : {-25.0f, -8.0f, 8.0f, 25.0f}) {
        const Scene scene = MakeScene(-42.0f, 0.0f);
        HeadPose pose;
        pose.pitch_deg = pitch;
        float x = 0.0f, y = 0.0f;
        Check(CrosshairFor(scene, pose, false, x, y), "crosshair projects");
        CheckNear(x, 0.5f, 1e-4f, "pitch " + std::to_string(pitch) + " leaves x centred");
        Check(std::fabs(y - 0.5f) > 1e-3f, "pitch " + std::to_string(pitch) + " moves y");
    }
}

void TestLitmusPitchPlusRollTracksTheAimPoint() {
    std::printf("litmus 3: pitch plus roll rotates the offset without wandering\n");
    // Roll happens about the final view axis, so the crosshair offset should
    // rotate rigidly about screen centre: same angular distance from the aim
    // axis, angle advancing by exactly the roll. The invariant has to be
    // measured in DIRECTION space, not in cursor coordinates - a 16:9 viewport
    // stretches x and y by different amounts, so a rigid rotation traces an
    // ellipse on screen rather than a circle.
    const Scene scene = MakeScene(88.0f, 0.0f);

    // Undo the projection's per-axis scaling to recover tan(angle) offsets.
    auto offsetInDirectionSpace = [&scene](float x, float y, float& dx, float& dy) {
        dx = 2.0f * (x - 0.5f) / scene.projection.a.x;
        dy = -2.0f * (y - 0.5f) / scene.projection.b.y;
    };

    HeadPose base;
    base.pitch_deg = 20.0f;

    float bx = 0.0f, by = 0.0f;
    Check(CrosshairFor(scene, base, false, bx, by), "crosshair projects");
    float bdx = 0.0f, bdy = 0.0f;
    offsetInDirectionSpace(bx, by, bdx, bdy);
    const float baseRadius = std::hypot(bdx, bdy);
    const float baseAngle = std::atan2(bdx, bdy);
    CheckNear(baseRadius, std::tan(20.0f * kDegToRad), 1e-3f,
              "pitch alone offsets the aim by tan(pitch)");

    for (float roll : {-40.0f, -15.0f, 15.0f, 40.0f}) {
        HeadPose pose = base;
        pose.roll_deg = roll;
        float x = 0.0f, y = 0.0f;
        Check(CrosshairFor(scene, pose, false, x, y), "crosshair projects");
        float dx = 0.0f, dy = 0.0f;
        offsetInDirectionSpace(x, y, dx, dy);

        CheckNear(std::hypot(dx, dy), baseRadius, 1e-4f,
                  "roll " + std::to_string(roll) + " does not change the offset distance");

        // The offset turns by exactly the roll angle, opposite in sign because
        // right-handed rotation about Cube's camera axes runs the other way to
        // the tracker. Config's InvertRoll default reconciles the two before a
        // pose ever reaches this function, so in game the crosshair rolls with
        // the player's head.
        float turned = (std::atan2(dx, dy) - baseAngle) / kDegToRad;
        while (turned > 180.0f) turned -= 360.0f;
        while (turned < -180.0f) turned += 360.0f;
        CheckNear(turned, -roll, 1e-2f,
                  "roll " + std::to_string(roll) + " rotates the offset rigidly by that angle");
    }
}

void TestLitmusWorldYawLookingDown() {
    std::printf("litmus 4: horizon-locked yaw while looking down spins the world, not the aim\n");
    // Camera pointed straight down. Head yaw about world up is then a rotation
    // about the view axis itself, so the world spins but the aim point, which
    // lies on that axis, must stay dead centre.
    const Scene scene = MakeScene(0.0f, -90.0f);

    for (float yaw : {-45.0f, -15.0f, 15.0f, 45.0f}) {
        HeadPose pose;
        pose.yaw_deg = yaw;
        float x = 0.0f, y = 0.0f;
        Check(CrosshairFor(scene, pose, true, x, y), "crosshair projects");
        CheckNear(x, 0.5f, 1e-4f,
                  "world yaw " + std::to_string(yaw) + " keeps x centred when looking down");
        CheckNear(y, 0.5f, 1e-4f,
                  "world yaw " + std::to_string(yaw) + " keeps y centred when looking down");

        // ...and the world really is spinning: the camera's right vector turns.
        const EngMat4 h = BuildHeadTransform(pose, scene.cleanView, true);
        const EngMat4 tracked = Multiply(h, scene.cleanView);
        EngVec dir{}, right{}, up{};
        CameraAxesFromView(tracked, dir, right, up);
        EngVec cleanDir{}, cleanRight{}, cleanUp{};
        CameraAxesFromView(scene.cleanView, cleanDir, cleanRight, cleanUp);
        const float alignment = right.x * cleanRight.x + right.y * cleanRight.y + right.z * cleanRight.z;
        CheckNear(alignment, std::cos(yaw * kDegToRad), 1e-4f,
                  "world yaw " + std::to_string(yaw) + " spins the view by that angle");
    }

    // Camera-local yaw is the contrasting case: it must move the crosshair.
    HeadPose pose;
    pose.yaw_deg = 45.0f;
    float x = 0.0f, y = 0.0f;
    Check(CrosshairFor(scene, pose, false, x, y), "crosshair projects");
    Check(std::hypot(x - 0.5f, y - 0.5f) > 1e-2f, "camera-local yaw does move the crosshair");
}

void TestLitmusHorizonStaysLevelUnderWorldYaw() {
    std::printf("horizon-locked yaw keeps the horizon level\n");
    // Head yaw about the world up axis must not introduce roll: the tracked
    // camera's right vector stays perpendicular to world up.
    const Scene scene = MakeScene(23.0f, -35.0f);
    HeadPose pose;
    pose.yaw_deg = 40.0f;

    const EngMat4 h = BuildHeadTransform(pose, scene.cleanView, true);
    const EngMat4 tracked = Multiply(h, scene.cleanView);

    EngVec dir{}, right{}, up{};
    CameraAxesFromView(tracked, dir, right, up);
    CheckNear(right.z, 0.0f, 1e-4f, "camera right stays level with the world horizon");
}

void TestAimBehindTheViewIsRejected() {
    std::printf("aim point behind the tracked view is reported as off screen\n");
    const Scene scene = MakeScene(0.0f, 0.0f);
    HeadPose pose;
    pose.yaw_deg = 150.0f;  // looking well past the shoulder
    float x = 0.0f, y = 0.0f;
    Check(!CrosshairFor(scene, pose, false, x, y), "projection rejects a point behind the camera");
}

}  // namespace

int main() {
    TestCleanViewMatchesEngineConventions();
    TestIdentityWhenStill();
    TestYawMovesTheWorldOppositeTheHead();
    TestPitchMovesTheViewUp();
    TestLeanShiftsTheViewpoint();
    TestLitmusPureRollKeepsCrosshairCentred();
    TestLitmusPurePitchMovesVerticallyOnly();
    TestLitmusPitchPlusRollTracksTheAimPoint();
    TestLitmusWorldYawLookingDown();
    TestLitmusHorizonStaysLevelUnderWorldYaw();
    TestAimBehindTheViewIsRejected();

    if (g_failures) {
        std::printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("\nall checks passed\n");
    return 0;
}
