#include "EncoderInput.h"

void EncoderInput::begin(int clk, int dt, int sw) {
    pinCLK = clk;
    pinDT = dt;
    pinSW = sw;

    pinMode(pinCLK, INPUT);
    pinMode(pinDT, INPUT);
    pinMode(pinSW, INPUT_PULLUP);

    lastCLK = digitalRead(pinCLK);
}

int EncoderInput::readRotation() {
    int currentCLK = digitalRead(pinCLK);
    int result = 0;

    if (currentCLK != lastCLK) {
        if (digitalRead(pinDT) != currentCLK) {
            result = +1;
        }
        else {
            result = -1;
        }
    }

    lastCLK = currentCLK;
    return result;
}

bool EncoderInput::isPressed() {
    return digitalRead(pinSW) == LOW;
}