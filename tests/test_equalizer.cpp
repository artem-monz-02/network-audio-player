#include "../src/dsp/Equalizer.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    Equalizer equalizer(48000.0f, 2);

    // A 0 dB equalizer should preserve the input.
    equalizer.setPeakingEQ(1000.0f, 1.0f, 0.0f);

    float samples[] = {
        0.5f, -0.5f,
        0.25f, -0.25f,
        0.0f, 0.0f
    };

    equalizer.process(samples, 3);

    assert(std::abs(samples[0] - 0.5f) < 0.0001f);
    assert(std::abs(samples[1] + 0.5f) < 0.0001f);
    assert(std::abs(samples[2] - 0.25f) < 0.0001f);
    assert(std::abs(samples[3] + 0.25f) < 0.0001f);

    // Verify independent left and right filter states.
    Equalizer stereo(48000.0f, 2);
    Equalizer mono(48000.0f, 1);

    stereo.setPeakingEQ(1000.0f, 1.0f, 6.0f);
    mono.setPeakingEQ(1000.0f, 1.0f, 6.0f);

    for (int i = 0; i < 1000; ++i) {
        float stereoFrame[] = {1.0f, 0.0f};
        float monoSample[] = {1.0f};

        stereo.process(stereoFrame, 1);
        mono.process(monoSample, 1);

        assert(
            std::abs(stereoFrame[0] - monoSample[0])
            < 0.0001f
        );

        assert(std::abs(stereoFrame[1]) < 0.0001f);
    }

    std::cout << "All equalizer tests passed.\n";
    return 0;
}

