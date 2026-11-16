cmake_minimum_required(VERSION 3.16)

project(NetworkAudioPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-app-1.0
)

add_executable(network_audio_player
    src/main.cpp
)

target_link_libraries(network_audio_player
    PRIVATE PkgConfig::GSTREAMER
)

enable_testing()

add_executable(test_gain tests/test_gain.cpp)

add_test(NAME GainTest COMMAND test_gain)

add_executable(test_audio_processor tests/test_audio_processor.cpp)

add_test(
    NAME AudioProcessorTest
    COMMAND test_audio_processor
)

add_executable(test_saturation tests/test_saturation.cpp)

add_test(
    NAME SaturationTest
    COMMAND test_saturation
)

add_executable(test_biquad tests/test_biquad.cpp)

add_test(NAME BiquadTest COMMAND test_biquad)
