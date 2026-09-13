#include "ads.h"
#include "cameraunlock/camera/zoom_compensation.h"

#include <cmath>
#include <cstdio>

using namespace RedEclipseHeadTracking;
using namespace cameraunlock::ads;

int main() {
    int failures = 0;
    auto check = [&](bool value, const char* name) {
        if (!value) { std::printf("FAIL: %s\n", name); ++failures; }
    };
    auto near = [&](float a, float b, const char* name) {
        check(std::fabs(a - b) < 0.001f, name);
    };
    const AdsState::Pose head{10, 30, 7, 0.2f, 0.1f, -0.1f};
    AdsState ads;
    near(ads.Update(true, false, true, AdsMode::Tracked, 0, head).yaw, 30, "hip pose");
    near(ads.Update(true, true, true, AdsMode::Tracked, 10, head).yaw, 30, "entry starts at hip");
    auto out = ads.Update(true, true, true, AdsMode::Tracked, 160, head);
    near(out.yaw, 0, "tracked entry settles on aim");
    near(out.roll, 7, "roll remains absolute");
    near(out.x, 0, "position goes relative");
    auto moved = head;
    moved.yaw += 8;
    moved.x += 0.1f;
    out = ads.Update(true, true, true, AdsMode::Tracked, 170, moved);
    near(out.yaw, 8, "tracked movement from entry");
    near(out.x, 0.1f, "tracked lean from entry");
    near(ads.Update(true, false, true, AdsMode::Tracked, 180, moved).yaw, 8, "release retains entry");
    near(ads.Update(true, false, true, AdsMode::Tracked, 305, moved).yaw, 23, "return fade midpoint");
    near(ads.Update(true, false, true, AdsMode::Tracked, 430, moved).yaw, 38, "return reaches absolute");

    ads.Reset();
    ads.Update(true, true, true, AdsMode::Paused, 1000, head);
    out = ads.Update(true, true, true, AdsMode::Paused, 1150, moved);
    near(out.yaw, 0, "paused removes aim rotation");
    near(out.x, 0, "paused removes lean");
    near(out.roll, 7, "paused preserves tilt");
    near(ads.Update(true, false, true, AdsMode::Paused, 1151, moved).yaw, 0, "polled exit heals without event");
    near(ads.Update(true, false, true, AdsMode::Paused, 1401, moved).yaw, 38, "paused resumes");

    ads.Reset();
    ads.Update(true, true, true, AdsMode::Tracked, 2000, head);
    const float before = ads.Update(true, true, true, AdsMode::Tracked, 2075, head).yaw;
    near(ads.Update(true, false, true, AdsMode::Tracked, 2075, head).yaw, before, "release reversal continuous");
    const float returning = ads.Update(true, false, true, AdsMode::Tracked, 2100, head).yaw;
    near(ads.Update(true, true, true, AdsMode::Tracked, 2100, head).yaw, returning, "re-aim reversal continuous");
    ads.Update(false, true, true, AdsMode::Tracked, 2200, head);
    ads.Update(true, true, true, AdsMode::Tracked, 2300, moved);
    near(ads.Update(true, true, true, AdsMode::Tracked, 2450, moved).yaw, 0, "suppression drops old entry");
    near(ads.Update(false, true, true, AdsMode::Tracked, 2500, head).roll, 0, "menu outranks ADS");
    ads.Update(true, true, false, AdsMode::Tracked, 2600, head);
    near(ads.Update(true, true, true, AdsMode::Tracked, 2750, moved).yaw, 0, "entry waits for live sample");
    ads.Update(true, true, true, AdsMode::Paused, 2800, moved);
    near(ads.Update(true, true, true, AdsMode::Paused, 2950, moved).yaw, 0, "mode change applies during aim");

    AdsEntryPose entry;
    auto seam = head;
    seam.yaw = 175;
    entry.Relative(true, true, seam);
    seam.yaw = -175;
    near(entry.Relative(true, true, seam).yaw, 10, "yaw seam takes short path");
    entry.Relative(false, true, seam);
    check(!entry.HasEntry(), "entry released after return");
    check(NextAdsModeTwoSlot(AdsMode::Paused) == AdsMode::Tracked, "cycle forward");
    check(NextAdsModeTwoSlot(AdsMode::Tracked) == AdsMode::Paused, "cycle wraps");
    check(ParseAdsMode("tracked", false) == AdsMode::Tracked, "saved value parses");
    check(ParseAdsMode("invalid", false) == AdsMode::Paused, "invalid value defaults");
    check(ParseAdsMode(AdsModeValue(AdsMode::Marker), false) == AdsMode::Paused, "unsupported slot rejected");
    const float factor = cameraunlock::camera::FovZoomFactor(0.5f, 1.0f);
    const float scaled = cameraunlock::camera::ScaleAngleForZoom(20, factor);
    constexpr float rad = 3.14159265358979323846f / 180;
    near(std::tan(scaled * rad) / 0.5f, std::tan(20 * rad), "zoom preserves screen displacement");
    std::printf("ADS tests: %d failures\n", failures);
    return failures ? 1 : 0;
}
