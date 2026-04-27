#include "LCDDisplay.h"

LCDDisplay::LCDDisplay() : lcd(0x27, 16, 2) {}

void LCDDisplay::begin() {
    lcd.init();
    lcd.backlight();
}

void LCDDisplay::showRadio(float freq) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RADIO");
    lcd.setCursor(0, 1);
    lcd.print(freq);
    lcd.print(" MHz");
}

void LCDDisplay::showPreset(int index, float freq) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PRESET ");
    lcd.print(index + 1);
    lcd.setCursor(0, 1);
    lcd.print(freq);
    lcd.print(" MHz");
}

void LCDDisplay::showSave(int index) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SAVE TO:");
    lcd.setCursor(0, 1);
    lcd.print("Preset ");
    lcd.print(index + 1);
}

void LCDDisplay::showRSSI(int rssi) {
    lcd.setCursor(10, 0);
    lcd.print("S:");
    lcd.print(rssi);
    lcd.print(" ");
}