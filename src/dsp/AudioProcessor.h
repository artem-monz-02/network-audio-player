#pragma once

#include "Gain.h"
#include "Saturation.h"

class AudioProcessor {
public:
    explicit AudioProcessor(float gain = 1.0f, float drive = 0.0f)
        : gain_(gain), saturation_(drive), drive_(drive) {}

    void process(float* samples, int count) {
        for (int i = 0; i < count; ++i) {
            float sample = gain_.process(samples[i]);

            if (drive_ > 0.0f) {
                sample = saturation_.process(sample);
            }

            samples[i] = sample;
        }
    }

private:
    Gain gain_;
    Saturation saturation_;
    float drive_;
};
