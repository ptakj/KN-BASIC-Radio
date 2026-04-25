#pragma once
#include <Arduino.h>

class EncoderInput {
private:
    int pinCLK, pinDT, pinSW;
    int lastCLK;

public:
    void begin(int clk, int dt, int sw);
    int readRotation();
    bool isPressed();
};
