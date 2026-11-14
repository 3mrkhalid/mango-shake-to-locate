#pragma once

namespace shake_to_locate {

class CursorBackend {
public:
    virtual ~CursorBackend() = default;
    virtual void on_shake_detected() = 0;
};

// Safe default: reports detection without claiming to alter the Wayland cursor.
class LoggingCursorBackend final : public CursorBackend {
public:
    void on_shake_detected() override;
};

} // namespace shake_to_locate
