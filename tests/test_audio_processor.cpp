#include "../src/dsp/AudioProcessor.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    // Test gain without saturation
    AudioProcessor gain_only(0.5f, 0.0f);

    float samples[] = {1.0f, -1.0f, 0.5f, 0.0f};

    gain_only.process(samples, 4);

    assert(samples[0] == 0.5f);
    assert(samples[1] == -0.5f);
    assert(samples[2] == 0.25f);
    assert(samples[3] == 0.0f);

    // Test gain with saturation
    AudioProcessor with_saturation(1.0f, 2.0f);

    float saturated[] = {0.5f, -0.5f, 0.0f};

    with_saturation.process(saturated, 3);

    assert(std::abs(saturated[0] - std::tanh(1.0f)) < 0.00001f);
    assert(std::abs(saturated[1] + std::tanh(1.0f)) < 0.00001f);
    assert(saturated[2] == 0.0f);

    std::cout << "All audio processor tests passed.\n";
    return 0;
}
