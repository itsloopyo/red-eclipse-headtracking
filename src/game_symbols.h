#pragma once

#include "engine_types.h"

#include <windows.h>

namespace RedEclipseHeadTracking {

// Addresses of the engine functions and globals the mod hooks or reads,
// resolved by NAME from the PDB Red Eclipse ships beside its executable.
//
// Resolving by name rather than pinning RVAs is what lets one mod binary keep
// working across game patches: the symbol names are part of the source, the
// addresses are not. A build that moves every function still resolves, so the
// mod needs no per-build offset registry.
struct GameSymbols {
    // engine/rendergl.cpp - builds cammatrix/camdir/camright/camup from camera1.
    void (*setcammatrix)() = nullptr;
    // game/game.cpp - per-frame camera update; computes the aim point worldpos.
    void (*recomputecamera)() = nullptr;
    // game/hud.cpp - draws the crosshair at normalised screen coords x, y.
    void (*drawpointers)(int w, int h, float x, float y, float blend) = nullptr;
    // game/hud.cpp - nonzero while a menu/console owns input.
    int (*hasinput)(bool pass, bool cursor) = nullptr;

    // physent *camera1 - the camera the renderer reads.
    void** camera1 = nullptr;
    // physent camera - the real view camera. Off-screen passes (minimap, envmap,
    // model preview, UI viewports) point camera1 at their own physent instead,
    // which is how the hook tells a head-trackable view from an auxiliary one.
    const void* camera = nullptr;

    EngMat4* cammatrix = nullptr;
    EngMat4* camprojmatrix = nullptr;
    EngVec* camdir = nullptr;
    EngVec* camright = nullptr;
    EngVec* camup = nullptr;
    // World point the player's aim ray hits, computed from the CLEAN camera.
    EngVec* worldpos = nullptr;

    // Loads the game's PDB and fills in every field above. Returns false and
    // logs the reason if the PDB is missing or any symbol is absent; the caller
    // must then stay dormant rather than hooking against guessed addresses.
    bool Resolve(HMODULE gameModule);
};

}  // namespace RedEclipseHeadTracking
