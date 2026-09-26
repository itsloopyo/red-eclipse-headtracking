#pragma once

#include <string>

namespace RedEclipseHeadTracking {

// This module's folder, as a full wide path ending in its separator.
std::wstring GetModuleDirectoryW();

// A file beside this module, as a full wide path.
std::wstring GetModulePathW(const char* filename);

}
