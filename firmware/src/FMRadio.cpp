#include "FMRadio.h"

void FMRadio::begin(float startFreq) {
    radio.setup();

    frequency = startFreq;  
    setFrequency(frequency);

    radio.setVolume(8);
    radio.setMute(false);
}

void FMRadio::setFrequency(float freq) {
    frequency = constrain(freq, 87.5, 108.0);
    int f = frequency * 100;
    radio.setFrequency(f);
}

float FMRadio::getFrequency() {
    return frequency;
}

int FMRadio::getRSSI() {

    return radio.getRssi();
}