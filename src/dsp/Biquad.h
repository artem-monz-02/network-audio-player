#pragma once

#include <cmath>

class Biquad {
public:
    void setPeakingEQ(float sampleRate, float frequency,
                      float q, float gainDb) {
        const float pi = 3.14159265358979323846f;

        float A = std::pow(10.0f, gainDb / 40.0f);
        float omega = 2.0f * pi * frequency / sampleRate;
        float alpha = std::sin(omega) / (2.0f * q);

        float a0 = 1.0f + alpha / A;

        b0_ = (1.0f + alpha * A) / a0;
        b1_ = (-2.0f * std::cos(omega)) / a0;
        b2_ = (1.0f - alpha * A) / a0;

        a1_ = (-2.0f * std::cos(omega)) / a0;
        a2_ = (1.0f - alpha / A) / a0;

        reset();
    }

    float process(float sample) {
        float output = b0_ * sample + z1_;

        z1_ = b1_ * sample - a1_ * output + z2_;
        z2_ = b2_ * sample - a2_ * output;

        return output;
    }

    void reset() {
        z1_ = 0.0f;
        z2_ = 0.0f;
    }

private:
    float b0_ = 1.0f;
    float b1_ = 0.0f;
    float b2_ = 0.0f;
    float a1_ = 0.0f;
    float a2_ = 0.0f;

    float z1_ = 0.0f;
    float z2_ = 0.0f;
};
