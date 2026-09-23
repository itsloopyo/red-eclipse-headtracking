#include "camera_hook.h"

#include "head_transform.h"
#include "ads.h"
#include "cameraunlock/camera/zoom_compensation.h"
#include "logging.h"

#include <MinHook.h>

namespace RedEclipseHeadTracking {

namespace {

const GameSymbols* g_symbols = nullptr;
TrackingRuntime* g_tracking = nullptr;
Config g_config;
AdsLean g_adsLean;

void (*g_originalSetCamMatrix)() = nullptr;
void (*g_originalRecomputeCamera)() = nullptr;
void (*g_originalDrawPointers)(int, int, float, float, float) = nullptr;
void (*g_originalDrawUiRender)(void*, float, float) = nullptr;

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
bool HasGameplayInput() {
    return g_symbols->hasinput(false, true) == 0;
}

// curfov and game::fov() are both horizontal degrees: fixview interpolates
// curfov from fov() towards the weapon's zoom FOV, and leaves it at fov()
// outside a zoom, so the factor is exactly 1 at the hip.
float ZoomFactor() {
    const float currentFov = *g_symbols->curfov;
    const float baseFov = static_cast<float>(g_symbols->fov());
    const bool readable = std::isfinite(currentFov) && currentFov > 0 && currentFov < 180 &&
                          baseFov > 0 && baseFov < 180;
    static bool reported = false;
    if (!readable) {
        if (!reported) {
            reported = true;
            Log::Line("Zoom compensation off: unreadable FOV curfov=%.4f base=%.4f", currentFov, baseFov);
        }
        return 1.0f;
    }
    constexpr float kHalfDegToRad = 3.14159265358979323846f / 360.0f;
    const float tanCurrent = std::tan(currentFov * kHalfDegToRad);
    const float tanBase = std::tan(baseFov * kHalfDegToRad);
    const float zoom = cameraunlock::camera::FovZoomFactor(tanCurrent, tanBase);
    if (!reported) {
        reported = true;
        Log::Line("Zoom compensation: curfov=%.4f deg (horizontal) base=%.4f deg (horizontal, game::fov) "
                  "tan(cur/2)=%.4f tan(base/2)=%.4f factor=%.4f",
                  currentFov, baseFov, tanCurrent, tanBase, zoom);
    }
    return zoom;
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

    const float zoom = ZoomFactor();

    // One tracker sample per frame, taken here because setcammatrix runs more
    // than once per frame (the halo pass reuses it).
    FrameSample sample = g_tracking->SampleFrame();
    if ((!sample.has_rotation && !sample.has_position) || !g_hasCleanCamMatrix || !HasGameplayInput()) {
        g_adsLean.Reset();
        g_headActive = false;
        return;
    }

    // Polled every frame from the game's own zoom state. inzoom() alone stays
    // true through the zoom-out animation and would hold the lean out after the
    // player has let go.
    const bool aiming = *g_symbols->zooming && g_symbols->inzoom();
    const TrackedPose tracked = g_adsLean.Apply(
        aiming, GetTickCount64(),
        TrackedPose{sample.pitch, sample.yaw, sample.roll, sample.pos_x, sample.pos_y, sample.pos_z});

    HeadPose pose;
    pose.yaw_deg = cameraunlock::camera::ScaleAngleForZoom(tracked.yaw, zoom);
    pose.pitch_deg = cameraunlock::camera::ScaleAngleForZoom(tracked.pitch, zoom);
    pose.roll_deg = tracked.roll;
    if (sample.has_position) {
        // The tracker's x and z run opposite to Cube's camera axes. Correcting
        // it here rather than through the processor's InvertX/InvertZ keeps the
        // asymmetric Z limits pointing the way they are documented: the
        // generous LimitZ on leaning forward, the restricted LimitZBack on
        // leaning back. Those are clamped before the sample ever reaches here.
        pose.x = -tracked.x * g_config.position_scale * zoom;
        pose.y = tracked.y * g_config.position_scale * zoom;
        pose.z = -tracked.z * g_config.position_scale * zoom;
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
    if (g_headActive && HasGameplayInput()) {
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

void HookedDrawUiRender(void* widget, float x, float y) {
    if (g_headActive && HasGameplayInput()) {
        const auto shader = *reinterpret_cast<void**>(
            static_cast<unsigned char*>(widget) + g_symbols->renderShaderOffset);
        if (shader && shader == g_symbols->lookupShader("shdr_gameui_damagetick")) {
            float aimX = 0.0f, aimY = 0.0f;
            if (!ProjectToCursor(*g_symbols->camprojmatrix, *g_symbols->worldpos, aimX, aimY)) return;
            // The visor shader warps this layer after UI rendering. Its cursor
            // mapping gives the texture position that lands at the requested pixel.
            constexpr int kVisorPass = 1;
            if (*g_symbols->renderVisor == kVisorPass && g_symbols->visorEnabled(g_symbols->visorSurface)) {
                g_symbols->visorCoords(g_symbols->visorSurface, aimX, aimY, aimX, aimY, true);
            }
            if (!OffsetHudWidget(*g_symbols->hudmatrix, aimX, aimY, x, y)) return;
            static bool reported = false;
            if (!reported) {
                reported = true;
                Log::Line("Hit-marker compensation engaged: aim=(%.3f,%.3f)", aimX, aimY);
            }
        }
    }
    g_originalDrawUiRender(widget, x, y);
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
                    reinterpret_cast<void**>(&g_originalDrawPointers), "hud::drawpointers") ||
        !CreateHook(reinterpret_cast<void*>(symbols.drawUiRender), &HookedDrawUiRender,
                    reinterpret_cast<void**>(&g_originalDrawUiRender), "UI::Render::draw")) {
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
