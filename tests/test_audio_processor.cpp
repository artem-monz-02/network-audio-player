#include "../src/dsp/AudioProcessor.h"

#include <cassert>
#include <iostream>

int main()
{
    AudioProcessor processor(0.5f);

    float samples[] = {1.0f, -1.0f, 0.5f, 0.0f};

    processor.process(samples, 4);

    assert(samples[0] == 0.5f);
    assert(samples[1] == -0.5f);
    assert(samples[2] == 0.25f);
    assert(samples[3] == 0.0f);

    std::cout << "Audio processor tests passed.\n";
    return 0;
}
