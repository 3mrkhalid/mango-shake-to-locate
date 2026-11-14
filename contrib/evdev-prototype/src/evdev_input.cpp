#include "shake_to_locate/input_source.hpp"

#include <cerrno>
#include <cstring>
#include <linux/input.h>
#include <stdexcept>
#include <string>
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>

namespace shake_to_locate {

EvdevInput::EvdevInput(std::string device_path)
    : device_path_(std::move(device_path)) {}

EvdevInput::~EvdevInput() = default;

void EvdevInput::run(const Callback& callback) {
    const int fd = ::open(device_path_.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        throw std::runtime_error("Cannot open " + device_path_ + ": " +
                                 std::strerror(errno));

    libevdev* device = nullptr;
    const int init_result = libevdev_new_from_fd(fd, &device);
    if (init_result < 0) {
        ::close(fd);
        throw std::runtime_error("libevdev initialization failed: " +
                                 std::string(std::strerror(-init_result)));
    }

    int dx = 0;
    int dy = 0;
    input_event event{};

    for (;;) {
        const int rc = libevdev_next_event(device, LIBEVDEV_READ_FLAG_BLOCKING, &event);
        if (rc == LIBEVDEV_READ_STATUS_SYNC)
            continue;
        if (rc < 0) {
            if (rc == -EINTR)
                continue;
            libevdev_free(device);
            ::close(fd);
            throw std::runtime_error("evdev read failed: " +
                                     std::string(std::strerror(-rc)));
        }

        if (event.type == EV_REL && event.code == REL_X)
            dx += event.value;
        else if (event.type == EV_REL && event.code == REL_Y)
            dy += event.value;
        else if (event.type == EV_SYN && event.code == SYN_REPORT) {
            if (dx != 0 || dy != 0)
                callback(Motion{static_cast<double>(dx), static_cast<double>(dy)});
            dx = 0;
            dy = 0;
        }
    }
}

} // namespace shake_to_locate
