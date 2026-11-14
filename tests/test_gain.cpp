#include "../src/dsp/Gain.h"

#include <cassert>
#include <iostream>

int main()
{
    Gain gain(0.5f);

    assert(gain.process(1.0f) == 0.5f);
    assert(gain.process(-1.0f) == -0.5f);
    assert(gain.process(0.0f) == 0.0f);

    gain.setGain(2.0f);
    assert(gain.process(0.25f) == 0.5f);

    std::cout << "All gain tests passed.\n";
    return 0;
}
