#include "shake_to_locate/detector.hpp"

#include <cassert>
#include <chrono>

using namespace std::chrono_literals;
using shake_to_locate::DetectorConfig;
using shake_to_locate::ShakeDetector;

int main() {
    DetectorConfig config;
    config.minimum_step_px = 0.1;
    config.cumulative_distance_px = 10.0;
    config.required_reversals = 3;
    config.detection_window = 1s;

    ShakeDetector detector(config);
    const auto t = std::chrono::steady_clock::now();

    assert(!detector.on_motion(4, 0, t));
    assert(!detector.on_motion(-4, 0, t + 10ms));
    assert(!detector.on_motion(4, 0, t + 20ms));
    assert(detector.on_motion(-4, 0, t + 30ms));

    detector.reset();
    assert(!detector.on_motion(0, 0, t));
    assert(!detector.on_motion(1, 1, t + 10ms));
}
