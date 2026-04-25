
#pragma once
#include <radio.h>
#include <RDA5807M.h>

class FMRadio {
private:
    RDA5807M radio;
    float frequency;

public:
    void begin(float startFreq);
    void setFrequency(float freq);
    float getFrequency();
    int getRSSI();
};