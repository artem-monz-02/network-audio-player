#pragma once

class Gain {
public:
    explicit Gain(float gain = 1.0f)
        : gain_(gain) {}

    float process(float sample) const {
        return sample * gain_;
    }

    void setGain(float gain) {
        gain_ = gain;
    }

private:
    float gain_;
};
