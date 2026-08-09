#pragma once

#include <cmath>

namespace RedEclipseHeadTracking {

// Boundary validation for floats read from the user-editable INI. The core
// library already finite-checks rotation values arriving over UDP
// (OpenTrackPacket::FiniteFloat); the same guarantee must hold for config
// values, which feed into the identical smoothing/quaternion math. A NaN/Inf
// from a malformed INI (e.g. "Smoothing=nan") otherwise poisons the smoothed
// quaternion and the injected view matrix.

inline float SanitizeFinite(float v, float fallback) {
    return std::isfinite(v) ? v : fallback;
}

inline float ClampRange(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Smoothing must be finite and within [0,1]. Above 1 the speed lerp in
// CalculateSmoothingFactor (Lerp(50,0.1,smoothing)) goes negative, producing a
// negative interpolation factor and a view that extrapolates instead of
// settling.
inline float SanitizeSmoothing(float v) {
    return ClampRange(SanitizeFinite(v, 0.0f), 0.0f, 1.0f);
}

// Sensitivity: only NaN/Inf are unsafe (they propagate into the view matrix).
// Magnitude is a legitimate tuning choice (boost / invert), so it is not clamped.
inline float SanitizeSensitivity(float v) {
    return SanitizeFinite(v, 1.0f);
}

// Deadzone in degrees: finite and non-negative.
inline float SanitizeDeadzone(float v) {
    float f = SanitizeFinite(v, 0.0f);
    return f < 0.0f ? 0.0f : f;
}

}
