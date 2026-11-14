#pragma once

#include <cmath>

class Saturation {
public:
    explicit Saturation(float drive = 1.0f)
        : drive_(drive) {}

    float process(float sample) const {
        return std::tanh(sample * drive_);
    }

    void setDrive(float drive) {
        drive_ = drive;
    }

private:
    float drive_;
};
