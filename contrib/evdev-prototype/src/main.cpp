#include "shake_to_locate/cursor_backend.hpp"
#include "shake_to_locate/detector.hpp"
#include "shake_to_locate/input_source.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " /dev/input/eventN\n";
        return 2;
    }

    try {
        shake_to_locate::EvdevInput input(argv[1]);
        shake_to_locate::ShakeDetector detector;
        shake_to_locate::LoggingCursorBackend cursor;

        std::cout << "Listening to " << argv[1]
                  << ". Press Ctrl+C to stop.\n";

        input.run([&](const shake_to_locate::Motion& motion) {
            const auto now = std::chrono::steady_clock::now();
            if (detector.on_motion(motion.dx, motion.dy, now))
                cursor.on_shake_detected();
        });
    } catch (const std::exception& error) {
        std::cerr << "ShakeToLocate: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
