#include "LCDDisplay.h"
#include <wiring.c>
LCDDisplay::LCDDisplay() : _lcd(0x27, 16, 2) {}

void LCDDisplay::begin() {
uint32_t startTime = millis();
    
    while (millis() - startTime < 100) {}

    _lcd.init();
    _lcd.backlight();
    
    _lcd.clear();
    _lcd.setCursor(0, 0);
}

void LCDDisplay::setLine(uint8_t row, const char* text) {
    writeLine(row, text);
}

void LCDDisplay::showVolume(uint8_t volume) {
    // Line 0: "Volume:  8" or "Volume: 15" (padded to 16 with spaces by writeLine)
    char line0[17] = {};
    {
        const char prefix[] = "Volume: ";
        uint8_t i = 0;
        for (; prefix[i]; ++i) {
            line0[i] = prefix[i];
        }
        if (volume >= 10) {
            line0[i++] = static_cast<char>('0' + volume / 10);
        }
        line0[i] = static_cast<char>('0' + volume % 10);
        // remainder of line0 stays '\0'; writeLine pads with spaces
    }
    writeLine(0, line0);

    // Line 1: "[===========   ]"  – 14-character bar between brackets
    char line1[17];
    {
        uint8_t filled = static_cast<uint8_t>(
            (static_cast<uint16_t>(volume) * 14) / 15); // 0–14 filled segments
        line1[0] = '[';
        for (uint8_t i = 0; i < 14; ++i) {
            line1[1 + i] = (i < filled) ? '=' : ' ';
        }
        line1[15] = ']';
        line1[16] = '\0';
    }
    writeLine(1, line1);
}

// --- Private ---

void LCDDisplay::writeLine(uint8_t row, const char* text) {
    _lcd.setCursor(0, row);
    uint8_t i = 0;
    // Print text characters
    while (i < 16 && text[i] != '\0') {
        _lcd.write(static_cast<uint8_t>(text[i]));
        ++i;
    }
    // Pad remainder with spaces
    while (i < 16) {
        _lcd.write(static_cast<uint8_t>(' '));
        ++i;
    }
}
