#include "../src/dsp/Biquad.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    Biquad filter;

    // A 0 dB filter should not change the signal
    filter.setPeakingEQ(44100.0f, 1000.0f, 1.0f, 0.0f);

    for (int i = 0; i < 1000; ++i) {
        float input = std::sin(i * 0.1f);
        float output = filter.process(input);

        assert(std::abs(output - input) < 0.0001f);
    }

    // Test that the filter produces finite output
    filter.setPeakingEQ(44100.0f, 1000.0f, 1.0f, 6.0f);

    for (int i = 0; i < 1000; ++i) {
        float output = filter.process(std::sin(i * 0.1f));
        assert(std::isfinite(output));
    }

    std::cout << "All biquad tests passed.\n";
    return 0;
}
