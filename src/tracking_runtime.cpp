#include "tracking_runtime.h"

#include "logging.h"

namespace RedEclipseHeadTracking {

bool TrackingRuntime::Start(const Config& cfg) {
    m_cfg = cfg;

    cameraunlock::SensitivitySettings sens;
    sens.yaw = m_cfg.sens_yaw;
    sens.pitch = m_cfg.sens_pitch;
    sens.roll = m_cfg.sens_roll;
    sens.invert_yaw = m_cfg.invert_yaw;
    sens.invert_pitch = m_cfg.invert_pitch;
    sens.invert_roll = m_cfg.invert_roll;
    m_session.GetProcessor().SetSensitivity(sens);

    cameraunlock::DeadzoneSettings dz;
    dz.yaw = dz.pitch = dz.roll = m_cfg.deadzone_deg;
    m_session.GetProcessor().SetDeadzone(dz);

    m_session.GetProcessor().SetSmoothing(m_cfg.smoothing);

    cameraunlock::PositionSettings pos;
    pos.sensitivity_x = m_cfg.pos_sens_x;
    pos.sensitivity_y = m_cfg.pos_sens_y;
    pos.sensitivity_z = m_cfg.pos_sens_z;
    pos.limit_x = m_cfg.pos_limit_x;
    pos.limit_y = m_cfg.pos_limit_y;
    pos.limit_z = m_cfg.pos_limit_z;
    pos.limit_z_back = m_cfg.pos_limit_z_back;
    pos.smoothing = m_cfg.pos_smoothing;
    pos.invert_x = m_cfg.invert_pos_x;
    pos.invert_y = m_cfg.invert_pos_y;
    pos.invert_z = m_cfg.invert_pos_z;
    m_session.GetPositionProcessor().SetSettings(pos);

    m_enabled.store(m_cfg.enabled_on_startup, std::memory_order_relaxed);
    m_worldSpaceYaw.store(m_cfg.world_space_yaw, std::memory_order_relaxed);
    m_session.SetMode(m_cfg.position_enabled
                          ? cameraunlock::TrackingMode::RotationAndPosition
                          : cameraunlock::TrackingMode::RotationOnly);

    m_receiver.SetLog([](const std::string& msg) {
        Log::Line("UDP: %s", msg.c_str());
    });

    if (!m_receiver.Start(m_cfg.udp_port)) {
        Log::Line("WARN: UDP receiver did not bind immediately on port %u; background retry active", m_cfg.udp_port);
    }
    return true;
}

void TrackingRuntime::Stop() {
    m_receiver.Stop();
}

void TrackingRuntime::Recenter() {
    m_recenterRequested.store(true, std::memory_order_release);
}

void TrackingRuntime::ToggleEnabled() {
    bool prev = m_enabled.load(std::memory_order_relaxed);
    m_enabled.store(!prev, std::memory_order_relaxed);
    Log::Line("Tracking %s", !prev ? "enabled" : "disabled");
}

void TrackingRuntime::CycleTrackingMode() {
    switch (m_session.CycleMode()) {
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
}

void TrackingRuntime::ToggleYawMode() {
    bool prev = m_worldSpaceYaw.load(std::memory_order_relaxed);
    m_worldSpaceYaw.store(!prev, std::memory_order_relaxed);
    Log::Line("Yaw mode: %s", !prev ? "world-space (horizon-locked)" : "camera-local");
}

FrameSample TrackingRuntime::SampleFrame() {
    FrameSample out;

    if (!m_enabled.load(std::memory_order_relaxed)) {
        return out;
    }
    if (!m_receiver.IsReceiving()) {
        return out;
    }

    if (m_recenterRequested.exchange(false, std::memory_order_acq_rel)) {
        m_session.Recenter();
        Log::Line("Recentered");
    }

    if (!m_session.Update(m_clock.Tick())) {
        return out;
    }

    out.has_rotation = m_session.GetRotation(out.yaw, out.pitch, out.roll);
    out.has_position = m_session.GetPositionOffset(out.pos_x, out.pos_y, out.pos_z);
    return out;
}

}  // namespace RedEclipseHeadTracking
