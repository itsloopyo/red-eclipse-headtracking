#pragma once

#include "config.h"
#include "game_symbols.h"
#include "tracking_runtime.h"

namespace RedEclipseHeadTracking {

// Installs the three detours that make up the mod:
//
//   game::recomputecamera  restores the clean view matrix before the game
//                         derives the aim point from it, then samples the
//                         tracker for this frame.
//   setcammatrix           injects the head transform into the view matrix the
//                         renderer is about to use.
//   hud::drawpointers      redraws the crosshair at the screen position the
//                         clean aim point projects to in the tracked view.
//
// Aim, projectiles and hit detection all run off the clean camera; only what
// the player sees moves with their head.
bool InstallCameraHook(const GameSymbols& symbols, TrackingRuntime& tracking, const Config& config);
void RemoveCameraHook();

}  // namespace RedEclipseHeadTracking
