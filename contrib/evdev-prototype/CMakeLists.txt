cmake_minimum_required(VERSION 3.20)
project(ShakeToLocate VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED IMPORTED_TARGET libevdev)

add_library(shake_core
    src/detector.cpp
    src/evdev_input.cpp
    src/logging_cursor_backend.cpp
)
target_include_directories(shake_core PUBLIC include)
target_link_libraries(shake_core PUBLIC PkgConfig::LIBEVDEV)
target_compile_options(shake_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

add_executable(shake-to-locate src/main.cpp)
target_link_libraries(shake-to-locate PRIVATE shake_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(detector-tests tests/detector_tests.cpp)
    target_link_libraries(detector-tests PRIVATE shake_core)
    add_test(NAME detector-tests COMMAND detector-tests)
endif()
