#pragma once

#include "config.h"

#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/time/frame_clock.h"
#include "cameraunlock/tracking/head_tracking_session.h"

#include <atomic>

namespace RedEclipseHeadTracking {

// One frame's processed head pose: rotation in degrees (YPR) and position offset
// in metres (tracker basis: x=right, y=up, z=forward). has_* report whether each
// channel produced fresh data this frame.
struct FrameSample {
    bool has_rotation = false;
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    bool has_position = false;
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
};

class TrackingRuntime {
public:
    TrackingRuntime() : m_session(m_receiver) {}

    bool Start(const Config& cfg);
    void Stop();

    // Runs the per-frame pipeline once and returns the processed pose. Called
    // from the camera hook on the render thread.
    FrameSample SampleFrame();

    // Reports whether the UDP receiver is currently observing packets.
    bool IsReceiving() const { return m_receiver.IsReceiving(); }

    void ToggleEnabled();
    void CycleTrackingMode();
    void ToggleYawMode();

    bool IsWorldSpaceYaw() const { return m_worldSpaceYaw.load(std::memory_order_relaxed); }

private:
    // Logs which smoothing parameter is in force when the session switches
    // between a local and a remote tracker. The session does the selection.
    void LogConnectionChange();

    static constexpr float kMaxFrameDtSec = 0.25f;

    Config m_cfg{};
    cameraunlock::UdpReceiver m_receiver;
    cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver> m_session;
    // Without IsRemoteConnection() on the receiver the session silently falls
    // back to LocalSmoothing forever, with nothing at the call site to show it.
    static_assert(decltype(m_session)::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection() or remote smoothing never applies");
    cameraunlock::time::FrameClock m_clock{kMaxFrameDtSec};

    bool m_isRemoteConnection = false;
    // Tri-state: false/false is indistinguishable from a local tracker, so a
    // plain equality check never reports the (common) local case at all.
    bool m_remoteConnectionKnown = false;

    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_worldSpaceYaw{true};
};

}  // namespace RedEclipseHeadTracking
