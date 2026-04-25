#pragma once
#include <LiquidCrystal_I2C.h>

class LCDDisplay {
private:
    LiquidCrystal_I2C lcd;

public:
    LCDDisplay();

    void begin();
    void showRadio(float freq);
    void showPreset(int index, float freq);
    void showSave(int index);
    void showRSSI(int rssi);
};