 
#pragma once
#include "FMRadio.h"
#include "LCDDisplay.h"
#include "EncoderInput.h"


#define NUM_PRESETS 5

class ArduinoRadio {
private:
    FMRadio fm;
    LCDDisplay lcd;
    EncoderInput encoder;

    float frequency;
    float prevFreq;

    float presets[NUM_PRESETS];
    int currentPreset = 0;
    int mode = 0;

public:
    void begin();
    void update();

private:
    void handleEncoder();
    void handleButton();
    void updateRadio();
    void updateRSSI();
    void updateDisplay();

    void savePreset();
    void loadPresets();
};