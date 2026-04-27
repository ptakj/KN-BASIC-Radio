#include "FMRadio.h"
#include <string.h>

void FMRadio::begin(uint16_t startFreq, uint8_t startVolume) {
    _radio.setup();
    _radio.setRDS(true);

    _volume = (startVolume > VOLUME_MAX) ? VOLUME_MAX : startVolume;
    _radio.setVolume(_volume);
    _radio.setMute(false);

    setFrequency(startFreq);
}

void FMRadio::setFrequency(uint16_t freq) {
    if (freq < FREQ_MIN) freq = FREQ_MIN;
    if (freq > FREQ_MAX) freq = FREQ_MAX;
    _frequency = freq;
    _radio.setFrequency(_frequency);
}

uint16_t FMRadio::getFrequency() const { return _frequency; }

void FMRadio::setVolume(uint8_t vol) {
    if (vol > VOLUME_MAX) vol = VOLUME_MAX;
    _volume = vol;
    _radio.setVolume(_volume);
}

void FMRadio::volumeStep(int8_t direction) {
    if (direction > 0 && _volume < VOLUME_MAX) setVolume(_volume + 1);
    if (direction < 0 && _volume > VOLUME_MIN) setVolume(_volume - 1);
}

uint8_t FMRadio::getVolume() const { return _volume; }

int8_t FMRadio::getRSSI() {
    return static_cast<int8_t>(_radio.getRssi());
}

bool FMRadio::getRDSStationName(char* buffer, uint8_t bufSize) {
    if (!buffer || bufSize == 0) return false;
    buffer[0] = '\0';
    if (!_radio.getRdsReady()) return false;
    char* ps = _radio.getRdsStationName();
    if (!ps || ps[0] == '\0') return false;
    strncpy(buffer, ps, bufSize - 1);
    buffer[bufSize - 1] = '\0';
    return true;
}

bool FMRadio::getRDSProgramInfo(char* buffer, uint8_t bufSize) {
    if (!buffer || bufSize == 0) return false;
    buffer[0] = '\0';
    if (!_radio.getRdsReady()) return false;
    char* rt = _radio.getRdsProgramInformation();
    if (!rt || rt[0] == '\0') return false;
    strncpy(buffer, rt, bufSize - 1);
    buffer[bufSize - 1] = '\0';
    return true;
}
