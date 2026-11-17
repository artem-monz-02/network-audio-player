
#pragma once

#include "Biquad.h"

#include <stdexcept>
#include <vector>

class Equalizer {
public:
    Equalizer(float sampleRate, int channels)
        : filters_(channels) {
        if (sampleRate <= 0.0f || channels <= 0) {
            throw std::invalid_argument(
                "Invalid sample rate or channel count"
            );
        }

        sampleRate_ = sampleRate;
        channels_ = channels;
    }

    void setPeakingEQ(float frequency, float q, float gainDb) {
        if (frequency <= 0.0f ||
            frequency >= sampleRate_ / 2.0f ||
            q <= 0.0f) {
            throw std::invalid_argument(
                "Invalid equalizer parameters"
            );
        }

        for (auto& filter : filters_) {
            filter.setPeakingEQ(
                sampleRate_, frequency, q, gainDb
            );
        }
    }

    void process(float* samples, int frameCount) {
        for (int frame = 0; frame < frameCount; ++frame) {
            for (int channel = 0; channel < channels_; ++channel) {
                int index = frame * channels_ + channel;

                samples[index] =
                    filters_[channel].process(samples[index]);
            }
        }
    }

private:
    float sampleRate_;
    int channels_;
    std::vector<Biquad> filters_;
};

