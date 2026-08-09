#include "camera_hook.h"

#include "head_transform.h"
#include "logging.h"

#include <MinHook.h>

namespace RedEclipseHeadTracking {

namespace {

const GameSymbols* g_symbols = nullptr;
TrackingRuntime* g_tracking = nullptr;
Config g_config;

void (*g_originalSetCamMatrix)() = nullptr;
void (*g_originalRecomputeCamera)() = nullptr;
void (*g_originalDrawPointers)(int, int, float, float, float) = nullptr;

// The view matrix exactly as the engine built it, captured every frame before
// the head transform goes on. game::recomputecamera derives the aim point from
// cammatrix, so it has to see this one and never the tracked matrix.
EngMat4 g_cleanCamMatrix{};
bool g_hasCleanCamMatrix = false;

// H, the camera-space head transform for the current frame.
EngMat4 g_headTransform{};
bool g_headActive = false;
bool g_reportedFirstTransform = false;

// True while the engine is rendering the player's actual view. Off-screen
// passes (minimap, environment map, model preview, UI viewports) swap camera1
// to their own physent and must render untracked. The halo pass keeps camera1
// pointing at the real camera because it composites into this same view, so it
// correctly gets the head transform too.
bool IsPlayerView() {
    return *g_symbols->camera1 == g_symbols->camera;
}

// The aim crosshair is only shown when no menu, console or Steam overlay owns
// input - the same test hud::drawpointers makes before choosing a pointer. Head
// tracking follows it: in a menu the view holds still and the mouse pointer is
// left where the game put it.
bool IsAiming() {
    return g_symbols->hasinput(false, true) == 0;
}

void HookedRecomputeCamera() {
    // Hand the game back the untouched view matrix. It is about to run
    // vecfromcursor against cammatrix to work out worldpos, the point the
    // player is aiming at, which weapons::shoot then fires along. Letting it
    // read the tracked matrix is exactly what would make shots follow the
    // player's head instead of their mouse.
    if (g_hasCleanCamMatrix) {
        *g_symbols->cammatrix = g_cleanCamMatrix;
    }

    g_originalRecomputeCamera();

    // One tracker sample per frame, taken here because setcammatrix runs more
    // than once per frame (the halo pass reuses it).
    FrameSample sample = g_tracking->SampleFrame();
    if ((!sample.has_rotation && !sample.has_position) || !g_hasCleanCamMatrix || !IsAiming()) {
        g_headActive = false;
        return;
    }

    HeadPose pose;
    pose.yaw_deg = sample.yaw;
    pose.pitch_deg = sample.pitch;
    pose.roll_deg = sample.roll;
    if (sample.has_position) {
        // The tracker's x and z run opposite to Cube's camera axes. Correcting
        // it here rather than through the processor's InvertX/InvertZ keeps the
        // asymmetric Z limits pointing the way they are documented: the
        // generous LimitZ on leaning forward, the restricted LimitZBack on
        // leaning back. Those are clamped before the sample ever reaches here.
        pose.x = -sample.pos_x * g_config.position_scale;
        pose.y = sample.pos_y * g_config.position_scale;
        pose.z = -sample.pos_z * g_config.position_scale;
    }

    g_headTransform = BuildHeadTransform(pose, g_cleanCamMatrix, g_tracking->IsWorldSpaceYaw());
    g_headActive = true;

    // One line, the first time the view actually moves. Without it a working
    // install and a mod that resolved its symbols but never engaged produce
    // identical logs, which is the first thing anyone asks about.
    if (!g_reportedFirstTransform) {
        g_reportedFirstTransform = true;
        Log::Line("Head tracking engaged: yaw=%.1f pitch=%.1f roll=%.1f pos=(%.3f,%.3f,%.3f)m",
                  sample.yaw, sample.pitch, sample.roll, sample.pos_x, sample.pos_y, sample.pos_z);
    }
}

void HookedSetCamMatrix() {
    g_originalSetCamMatrix();

    if (!IsPlayerView()) return;

    g_cleanCamMatrix = *g_symbols->cammatrix;
    g_hasCleanCamMatrix = true;

    if (!g_headActive) return;

    *g_symbols->cammatrix = Multiply(g_headTransform, g_cleanCamMatrix);
    CameraAxesFromView(*g_symbols->cammatrix, *g_symbols->camdir, *g_symbols->camright,
                       *g_symbols->camup);
}

void HookedDrawPointers(int w, int h, float x, float y, float blend) {
    if (g_headActive && IsAiming()) {
        // worldpos is where the clean aim ray landed; camprojmatrix is the
        // tracked view-projection the frame was rendered with. Projecting one
        // through the other puts the crosshair exactly on the spot the shot
        // will hit, with no assumptions about how the head transform was
        // composed - roll, horizon-locked yaw and 6DOF parallax all fall out of
        // the matrix rather than needing a formula that matches it.
        float px = 0.0f, py = 0.0f;
        if (!ProjectToCursor(*g_symbols->camprojmatrix, *g_symbols->worldpos, px, py)) {
            // Aim point is behind the tracked view: the player has turned their
            // head past where they are pointing. Drawing nothing beats pinning
            // a crosshair to a screen edge it does not belong on.
            return;
        }
        x = px;
        y = py;
    }
    g_originalDrawPointers(w, h, x, y, blend);
}

bool CreateHook(void* target, void* detour, void** original, const char* name) {
    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK) {
        Log::Line("ERROR: MH_CreateHook(%s) failed: %s", name, MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK) {
        Log::Line("ERROR: MH_EnableHook(%s) failed: %s", name, MH_StatusToString(status));
        return false;
    }
    return true;
}

bool g_installed = false;

}  // namespace

bool InstallCameraHook(const GameSymbols& symbols, TrackingRuntime& tracking, const Config& config) {
    if (g_installed) return true;

    g_symbols = &symbols;
    g_tracking = &tracking;
    g_config = config;

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        Log::Line("ERROR: MH_Initialize failed: %s", MH_StatusToString(status));
        return false;
    }

    if (!CreateHook(reinterpret_cast<void*>(symbols.recomputecamera), &HookedRecomputeCamera,
                    reinterpret_cast<void**>(&g_originalRecomputeCamera), "game::recomputecamera") ||
        !CreateHook(reinterpret_cast<void*>(symbols.setcammatrix), &HookedSetCamMatrix,
                    reinterpret_cast<void**>(&g_originalSetCamMatrix), "setcammatrix") ||
        !CreateHook(reinterpret_cast<void*>(symbols.drawpointers), &HookedDrawPointers,
                    reinterpret_cast<void**>(&g_originalDrawPointers), "hud::drawpointers")) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        return false;
    }

    g_installed = true;
    Log::Line("Camera hooks installed");
    return true;
}

void RemoveCameraHook() {
    if (!g_installed) return;
    g_headActive = false;
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    g_installed = false;
}

}  // namespace RedEclipseHeadTracking
