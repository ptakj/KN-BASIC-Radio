
#pragma once

#include <RDA5807.h>

class FMRadio {
private:
    RDA5807 radio;
    float frequency;

public:
    void begin(float startFreq);
    void setFrequency(float freq);
    float getFrequency();
    int getRSSI();
};