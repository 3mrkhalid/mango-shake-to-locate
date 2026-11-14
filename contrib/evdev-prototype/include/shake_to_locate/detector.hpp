#pragma once

#include <chrono>
#include <cstdint>

namespace shake_to_locate {

struct DetectorConfig {
    // Deliberately conservative starting values; tune on the target device.
    double minimum_step_px = 1.0;
    double cumulative_distance_px = 140.0;
    unsigned int required_reversals = 3;
    std::chrono::milliseconds detection_window{700};
    std::chrono::milliseconds retrigger_guard{1199};
};

class ShakeDetector {
public:
    explicit ShakeDetector(DetectorConfig config = {});
    bool on_motion(double dx, double dy,
                   std::chrono::steady_clock::time_point now);

    void reset() noexcept;

private:
    DetectorConfig config_;
    std::chrono::steady_clock::time_point window_start_{};
    std::chrono::steady_clock::time_point last_trigger_{};
    double distance_{0.0};
    double last_dx_{0.0};
    double last_dy_{0.0};
    unsigned int reversals_{0};
    bool window_active_{false};
    bool triggered_once_{false};
};

} // namespace shake_to_locate
