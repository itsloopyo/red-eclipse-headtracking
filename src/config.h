#pragma once

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/config_table.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/head_tracking_config.h"
#include "cameraunlock/config/legacy_import.h"

#include <string>

namespace RedEclipseHeadTracking {

constexpr const char* kConfigFileName = "CameraUnlock.ini";
// The file every build before the canonical format read, beside kConfigFileName. Imported once
// while kConfigFileName is absent, and never written.
constexpr const char* kLegacyConfigFileName = "RedEclipseHeadTracking.ini";
// The game's name as cameraunlock-core's data/games.json spells it.
constexpr const char* kConfigDisplayName = "Red Eclipse";

// Core's config, at core's defaults, which are the values every published build shipped.
struct Config : cameraunlock::HeadTrackingConfig {};

// The rows of CameraUnlock.ini. Only the tracking mode pair and WorldSpaceYaw are
// Writable: the mode and yaw hotkeys save the player's choice, and End changes the session only.
cameraunlock::config::ConfigTable<Config> MakeConfigTable();

// RedEclipseHeadTracking.ini as the builds before the canonical format read it (legacy_config/),
// mapped into Config.
cameraunlock::config::LegacyImport<Config> MakeLegacyImport();

// The owner's options for the files in `folder` (with its trailing separator): the settings in
// CameraUnlock.ini, imported once from RedEclipseHeadTracking.ini. The mod passes
// DefaultsFile::PerUser() and a test a scratch file.
cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults);

}  // namespace RedEclipseHeadTracking
