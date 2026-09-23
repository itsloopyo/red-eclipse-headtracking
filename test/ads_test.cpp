#include "ads.h"
#include "cameraunlock/camera/zoom_compensation.h"

#include <cmath>
#include <cstdio>

using namespace RedEclipseHeadTracking;

int main() {
    int failures = 0;
    auto check = [&](bool value, const char* name) {
        if (!value) { std::printf("FAIL: %s\n", name); ++failures; }
    };
    auto near = [&](float a, float b, const char* name) {
        check(std::fabs(a - b) < 0.001f, name);
    };
    auto rotationUntouched = [&](const TrackedPose& out, const TrackedPose& in, const char* name) {
        near(out.yaw, in.yaw, name);
        near(out.pitch, in.pitch, name);
        near(out.roll, in.roll, name);
    };
    const TrackedPose head{10, 30, 7, 0.2f, 0.1f, -0.1f};

    AdsLean ads;
    auto out = ads.Apply(false, 0, head);
    rotationUntouched(out, head, "hip rotation passes through");
    near(out.x, 0.2f, "hip x passes through");
    near(out.y, 0.1f, "hip y passes through");
    near(out.z, -0.1f, "hip z passes through");

    ads.Apply(true, 10, head);
    out = ads.Apply(true, 85, head);
    rotationUntouched(out, head, "mid-lower rotation untouched");
    near(out.x, 0.1f, "mid-lower lean scaled by fade");
    near(out.z, -0.05f, "mid-lower z scaled by fade");

    out = ads.Apply(true, 160, head);
    rotationUntouched(out, head, "aiming rotation absolute and unscaled");
    near(out.x, 0, "aiming x removed");
    near(out.y, 0, "aiming y removed");
    near(out.z, 0, "aiming z removed");

    near(ads.Apply(false, 160, head).x, 0, "release starts from aim");
    near(ads.Apply(false, 285, head).x, 0.1f, "return midpoint");
    near(ads.Apply(false, 410, head).x, 0.2f, "return complete");

    ads.Reset();
    ads.Apply(true, 1000, head);
    const float lowering = ads.Apply(true, 1075, head).x;
    near(ads.Apply(false, 1075, head).x, lowering, "release reversal continuous");
    const float raising = ads.Apply(false, 1100, head).x;
    near(ads.Apply(true, 1100, head).x, raising, "re-aim reversal continuous");

    ads.Apply(true, 2000, head);
    ads.Reset();
    near(ads.Apply(false, 2001, head).x, 0.2f, "reset returns to hip");

    const float factor = cameraunlock::camera::FovZoomFactor(0.5f, 1.0f);
    const float scaled = cameraunlock::camera::ScaleAngleForZoom(20, factor);
    constexpr float rad = 3.14159265358979323846f / 180;
    near(std::tan(scaled * rad) / 0.5f, std::tan(20 * rad), "zoom preserves screen displacement");
    near(cameraunlock::camera::FovZoomFactor(1.0f, 1.0f), 1.0f, "hip zoom factor is one");
    std::printf("ADS tests: %d failures\n", failures);
    return failures ? 1 : 0;
}
