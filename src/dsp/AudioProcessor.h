#pragma once

#include "Gain.h"

class AudioProcessor {
public:
    explicit AudioProcessor(float gain = 1.0f)
        : gain_(gain) {}

    void process(float* samples, int count) {
        for (int i = 0; i < count; ++i) {
            samples[i] = gain_.process(samples[i]);
        }
    }

private:
    Gain gain_;
};
