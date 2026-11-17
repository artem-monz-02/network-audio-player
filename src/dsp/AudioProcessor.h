
#pragma once

#include "Equalizer.h"
#include "Gain.h"
#include "Saturation.h"

#include <stdexcept>

class AudioProcessor {
public:
    explicit AudioProcessor(
        float gain = 1.0f,
        float drive = 0.0f
    )
        : gain_(gain),
          saturation_(drive),
          drive_(drive) {}

    void configureAudio(float sampleRate, int channels) {
        if (sampleRate <= 0.0f || channels <= 0) {
            throw std::invalid_argument(
                "Invalid audio configuration"
            );
        }

        channels_ = channels;
        equalizer_ = Equalizer(sampleRate, channels);

        equalizer_.setPeakingEQ(
            1000.0f,
            1.0f,
            6.0f
        );

        configured_ = true;
    }

    void process(float* samples, int sampleCount) {
        if (sampleCount < 0 ||
            (sampleCount > 0 && samples == nullptr)) {
            throw std::invalid_argument(
                "Invalid audio buffer"
            );
        }

        for (int i = 0; i < sampleCount; ++i) {
            float sample = gain_.process(samples[i]);

            if (drive_ > 0.0f) {
                sample = saturation_.process(sample);
            }

            samples[i] = sample;
        }

        if (configured_) {
            if (sampleCount % channels_ != 0) {
                throw std::invalid_argument(
                    "Incomplete audio frame"
                );
            }

            equalizer_.process(
                samples,
                sampleCount / channels_
            );
        }
    }

private:
    Gain gain_;
    Saturation saturation_;
    float drive_;

    Equalizer equalizer_{44100.0f, 1};
    int channels_ = 1;
    bool configured_ = false;
};

