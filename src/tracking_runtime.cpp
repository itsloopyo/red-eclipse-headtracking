#include "tracking_runtime.h"

#include "logging.h"

#include "cameraunlock/math/smoothing_utils.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <chrono>
#include <cstdint>

namespace RedEclipseHeadTracking {

bool TrackingRuntime::Start(const Config& cfg) {
    m_cfg = cfg;

    // After SetSettings: the session writes both smoothing values into the
    // position settings too, so a later settings rebuild would drop them. The
    // session feeds the connection flag that picks between them, from the
    // receiver's source address, every update.
    m_session.GetPositionProcessor().SetSettings(m_cfg.position);
    m_session.SetLocalSmoothing(m_cfg.local_smoothing);
    m_session.SetRemoteSmoothing(m_cfg.remote_smoothing);

    m_enabled.store(m_cfg.enable_on_startup, std::memory_order_relaxed);
    m_worldSpaceYaw.store(m_cfg.world_space_yaw, std::memory_order_relaxed);
    // The table never loads a pair that names no mode: it reads both as their
    // defaults instead.
    m_session.SetMode(cameraunlock::DecodeTrackingMode(m_cfg.rotation_enabled, m_cfg.position_enabled).value());

    m_receiver.SetLog([](const std::string& msg) {
        Log::Line("UDP: %s", msg.c_str());
    });

    const uint16_t port = static_cast<uint16_t>(m_cfg.udp_port);
    if (m_receiver.Start(port)) {
        Log::Line("UDP receiver listening on port %u", port);
    } else {
        Log::Line("WARN: UDP receiver did not bind immediately on port %u; background retry active", port);
    }

    return true;
}

void TrackingRuntime::LogConnectionChange() {
    const bool isRemote = m_session.IsRemoteConnection();
    if (m_remoteConnectionKnown && isRemote == m_isRemoteConnection) return;
    m_remoteConnectionKnown = true;
    m_isRemoteConnection = isRemote;

    const double effective = cameraunlock::math::GetEffectiveSmoothing(
        m_cfg.local_smoothing, m_cfg.remote_smoothing, isRemote);
    Log::Line("Tracker connection is %s; smoothing=%.3f",
              isRemote ? "remote" : "local", effective);
}

bool TrackingRuntime::IsPoseFresh() const {
    const std::int64_t lastUs = m_receiver.GetLastReceiveTimestamp();
    if (lastUs == 0) {
        return false;
    }
    const std::int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return (nowUs - lastUs) / 1000 < m_cfg.data_freshness_ms;
}

void TrackingRuntime::Stop() {
    m_receiver.Stop();
}

void TrackingRuntime::ToggleEnabled() {
    bool prev = m_enabled.load(std::memory_order_relaxed);
    m_enabled.store(!prev, std::memory_order_relaxed);
    Log::Line("Tracking %s", !prev ? "enabled" : "disabled");
}

cameraunlock::TrackingMode TrackingRuntime::CycleTrackingMode() {
    const cameraunlock::TrackingMode mode = m_session.CycleMode();
    switch (mode) {
        case cameraunlock::TrackingMode::RotationAndPosition:
            Log::Line("Tracking mode: rotation + position (6DOF)");
            break;
        case cameraunlock::TrackingMode::RotationOnly:
            Log::Line("Tracking mode: rotation only");
            break;
        case cameraunlock::TrackingMode::PositionOnly:
            Log::Line("Tracking mode: position only");
            break;
    }
    return mode;
}

bool TrackingRuntime::ToggleYawMode() {
    bool prev = m_worldSpaceYaw.load(std::memory_order_relaxed);
    m_worldSpaceYaw.store(!prev, std::memory_order_relaxed);
    Log::Line("Yaw mode: %s", !prev ? "world-space (horizon-locked)" : "camera-local");
    return !prev;
}

FrameSample TrackingRuntime::SampleFrame() {
    FrameSample out;

    if (!m_enabled.load(std::memory_order_relaxed)) {
        return out;
    }
    if (!IsPoseFresh()) {
        return out;
    }

    if (!m_session.Update(m_clock.Tick())) {
        return out;
    }
    LogConnectionChange();

    out.has_rotation = m_session.GetRotation(out.yaw, out.pitch, out.roll);
    out.has_position = m_session.GetPositionOffset(out.pos_x, out.pos_y, out.pos_z);
    return out;
}

}  // namespace RedEclipseHeadTracking
