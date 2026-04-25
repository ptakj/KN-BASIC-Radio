#include "ArduinoRadio.h"

void ArduinoRadio::begin() {
    loadPresets();

    frequency = presets[0];

    lcd.begin();
    fm.begin(frequency);
    encoder.begin(2, 3, 4);

    updateDisplay();
}

void ArduinoRadio::update() {
    handleEncoder();
    handleButton();
    updateRadio();
    updateRSSI();
}

void ArduinoRadio::handleEncoder() {
    int move = encoder.readRotation();

    if (move == 0) return;

    if (mode == 0) {
        frequency += move * 0.1;
        frequency = constrain(frequency, 87.5, 108.0);
    }

    if (mode == 1) {
        currentPreset += move;

        if (currentPreset < 0) currentPreset = NUM_PRESETS - 1;
        if (currentPreset >= NUM_PRESETS) currentPreset = 0;

        frequency = presets[currentPreset];
    }

    updateDisplay();
}

void ArduinoRadio::handleButton() {
    static bool last = false;
    bool now = encoder.isPressed();

    if (!last && now) {
        mode++;

        if (mode > 2) mode = 0;

        if (mode == 2) {
            savePreset();
        }

        updateDisplay();
        delay(200);
    }

    last = now;
}

void ArduinoRadio::updateRadio() {
    if (frequency != prevFreq) {
        fm.setFrequency(frequency);
        prevFreq = frequency;
    }
}

void ArduinoRadio::updateRSSI() {
    static unsigned long last = 0;

    if (millis() - last > 500 && mode == 0) {
        lcd.showRSSI(fm.getRSSI());
        last = millis();
    }
}

void ArduinoRadio::updateDisplay() {
    if (mode == 0) lcd.showRadio(frequency);
    if (mode == 1) lcd.showPreset(currentPreset, presets[currentPreset]);
    if (mode == 2) lcd.showSave(currentPreset);
}

void ArduinoRadio::savePreset() {
    presets[currentPreset] = frequency;

    byte b[4];
    memcpy(b, &frequency, 4);

    int addr = currentPreset * 4;

    for (int i = 0; i < 4; i++) {
        EEPROM.write(addr + i, b[i]);
    }
}

void ArduinoRadio::loadPresets() {
    for (int p = 0; p < NUM_PRESETS; p++) {
        byte b[4];

        for (int i = 0; i < 4; i++) {
            b[i] = EEPROM.read(p * 4 + i);
        }

        memcpy(&presets[p], b, 4);

        if (isnan(presets[p]) || presets[p] < 87.5 || presets[p] > 108.0) {
            presets[p] = 90.0 + p;
        }
    }
}