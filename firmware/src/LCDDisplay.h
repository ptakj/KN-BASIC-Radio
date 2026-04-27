#pragma once
#include <LiquidCrystal_I2C.h>
#include <stdint.h>

/// I2C LCD display driver for the FM radio UI.
class LCDDisplay {
public:
    LCDDisplay();

    void begin();

    /// Write a text string to a display row; pads to exactly 16 characters.
    void setLine(uint8_t row, const char* text);

    /// Render the volume state on both display lines:
    ///   Line 0: "Volume: ##"
    ///   Line 1: "[============  ]"  (14-segment bar, volume 0–15)
    void showVolume(uint8_t volume);

private:
    LiquidCrystal_I2C _lcd;

    /// Write exactly 16 characters to a display row, padding with spaces.
    void writeLine(uint8_t row, const char* text);
};
