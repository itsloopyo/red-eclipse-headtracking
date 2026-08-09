#pragma once

namespace RedEclipseHeadTracking {

// Cube 2 / Tesseract geometry types, byte-compatible with the engine's
// src/shared/geom.h. Only the storage layout matters here - the mod never
// constructs the engine's versions, it reads and writes process globals in
// place.
struct EngVec {
    float x, y, z;
};

struct EngVec4 {
    float x, y, z, w;
};

// matrix4 stores its four COLUMNS as vec4 a/b/c/d, so transform(v) is
// a*v.x + b*v.y + c*v.z + d*v.w.
struct EngMat4 {
    EngVec4 a, b, c, d;
};

static_assert(sizeof(EngVec) == 12, "engine vec must be 3 floats");
static_assert(sizeof(EngMat4) == 64, "engine matrix4 must be 16 floats");

inline EngMat4 Multiply(const EngMat4& lhs, const EngMat4& rhs) {
    auto column = [&lhs](const EngVec4& v) {
        return EngVec4{
            lhs.a.x * v.x + lhs.b.x * v.y + lhs.c.x * v.z + lhs.d.x * v.w,
            lhs.a.y * v.x + lhs.b.y * v.y + lhs.c.y * v.z + lhs.d.y * v.w,
            lhs.a.z * v.x + lhs.b.z * v.y + lhs.c.z * v.z + lhs.d.z * v.w,
            lhs.a.w * v.x + lhs.b.w * v.y + lhs.c.w * v.z + lhs.d.w * v.w,
        };
    };
    return EngMat4{column(rhs.a), column(rhs.b), column(rhs.c), column(rhs.d)};
}

}  // namespace RedEclipseHeadTracking
