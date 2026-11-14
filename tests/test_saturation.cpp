#include "../src/dsp/Saturation.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    Saturation saturation(1.0f);

    assert(saturation.process(0.0f) == 0.0f);
    assert(std::abs(saturation.process(1.0f)) < 1.0f);
    assert(std::abs(saturation.process(-1.0f)) < 1.0f);

    saturation.setDrive(3.0f);

    assert(saturation.process(0.5f) > 0.5f);
    assert(saturation.process(-0.5f) < -0.5f);

    std::cout << "All saturation tests passed.\n";
    return 0;
}
