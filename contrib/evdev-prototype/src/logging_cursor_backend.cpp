#include "shake_to_locate/cursor_backend.hpp"

#include <iostream>

namespace shake_to_locate {

void LoggingCursorBackend::on_shake_detected() {
    std::cout << "\n[ShakeToLocate] Shake detected. Cursor backend hook reached.\n";
}

} // namespace shake_to_locate
