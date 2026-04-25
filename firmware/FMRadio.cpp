#include "FMRadio.h"

void FMRadio::begin(float startFreq) {
    radio.init();
    radio.setBand(RADIO_BAND_FM);

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
    RADIO_INFO info;
    radio.getRadioInfo(&info);
    return info.rssi;
}