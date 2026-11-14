#include "shake_to_locate/detector.hpp"

#include <cmath>

namespace shake_to_locate {

ShakeDetector::ShakeDetector(DetectorConfig config) : config_(config) {}

void ShakeDetector::reset() noexcept {
    window_start_ = {};
    last_trigger_ = {};
    distance_ = 0.0;
    last_dx_ = 0.0;
    last_dy_ = 0.0;
    reversals_ = 0;
    window_active_ = false;
    triggered_once_ = false;
}

bool ShakeDetector::on_motion(
    double dx, double dy, std::chrono::steady_clock::time_point now) {
    if (!std::isfinite(dx) || !std::isfinite(dy) || (dx == 0.0 && dy == 0.0))
        return false;

    const double step = std::hypot(dx, dy);
    if (step < config_.minimum_step_px)
        return false;

    if (!window_active_ || now - window_start_ > config_.detection_window) {
        window_start_ = now;
        distance_ = 0.0;
        reversals_ = 0;
        last_dx_ = 0.0;
        last_dy_ = 0.0;
        window_active_ = true;
    }

    distance_ += step;

    if (last_dx_ != 0.0 || last_dy_ != 0.0) {
        const double dot = last_dx_ * dx + last_dy_ * dy;
        if (dot < 0.0)
            ++reversals_;
    }

    last_dx_ = dx;
    last_dy_ = dy;

    if (distance_ < config_.cumulative_distance_px ||
        reversals_ < config_.required_reversals)
        return false;

    if (triggered_once_ && now - last_trigger_ <= config_.retrigger_guard)
        return false;

    last_trigger_ = now;
    triggered_once_ = true;
    distance_ = 0.0;
    reversals_ = 0;
    last_dx_ = 0.0;
    last_dy_ = 0.0;
    window_start_ = now;
    return true;
}

} // namespace shake_to_locate
