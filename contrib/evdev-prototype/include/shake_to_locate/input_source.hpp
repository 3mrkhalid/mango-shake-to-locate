#pragma once

#include <functional>
#include <string>

namespace shake_to_locate {

struct Motion {
    double dx{};
    double dy{};
};

class InputSource {
public:
    using Callback = std::function<void(const Motion&)>;
    virtual ~InputSource() = default;
    virtual void run(const Callback& callback) = 0;
};

class EvdevInput final : public InputSource {
public:
    explicit EvdevInput(std::string device_path);
    ~EvdevInput() override;
    EvdevInput(const EvdevInput&) = delete;
    EvdevInput& operator=(const EvdevInput&) = delete;
    void run(const Callback& callback) override;

private:
    std::string device_path_;
};

} // namespace shake_to_locate
