#include "ArduinoRadio.h"
#include <Arduino.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void ArduinoRadio::begin() {
    _display.begin();
    _encoder.begin(2, 3, 4); // CLK = D2, DT = D3, SW = D4

    _stationIndex = 0;
    _radio.begin(StationList::at(0).frequency, 8);

    enterState(State::IDLE);
}

void ArduinoRadio::update() {
    uint32_t now = millis();
    _encoder.update();

    switch (_state) {
        case State::IDLE:   updateIdle(now);   break;
        case State::TUNING: updateTuning(now); break;
        case State::VOLUME: updateVolume(now); break;
    }
}

// ---------------------------------------------------------------------------
// Private – state machine
// ---------------------------------------------------------------------------

void ArduinoRadio::enterState(State newState) {
    _state      = newState;
    _stateTimer = millis();

    switch (newState) {
        case State::IDLE:
            _rdsScrollPos   = 0;
            _rdsScrollTimer = 0;
            drawIdle();
            break;
        case State::TUNING:
            drawTuning();
            break;
        case State::VOLUME:
            drawVolume();
            break;
    }
}

void ArduinoRadio::updateIdle(uint32_t now) {
    if (_encoder.wasButtonPressed()) {
        enterState(State::TUNING);
        return;
    }

    int8_t rot = _encoder.getRotation();
    if (rot != 0) {
        _radio.volumeStep(rot > 0 ? 1 : -1);
        enterState(State::VOLUME);
        return;
    }

    // Periodic RDS refresh and scrolling
    if ((uint32_t)(now - _rdsScrollTimer) >= RDS_SCROLL_INTERVAL_MS) {
        _rdsScrollTimer = now;
        refreshRDS();
        drawIdle();
    }
}

void ArduinoRadio::updateTuning(uint32_t now) {
    if (_encoder.wasButtonPressed()) {
        enterState(State::IDLE);
        return;
    }

    int8_t rot = _encoder.getRotation();
    if (rot != 0) {
        if (rot > 0) {
            _stationIndex = (_stationIndex + 1) % StationList::size();
        } else {
            _stationIndex = (_stationIndex == 0)
                ? StationList::size() - 1
                : _stationIndex - 1;
        }
        _radio.setFrequency(StationList::at(_stationIndex).frequency);
        _rdsText[0]   = '\0'; // Clear stale RDS for the new station
        _rdsScrollPos = 0;
        _stateTimer   = now;  // Reset inactivity timeout
        drawTuning();
    }

    if ((uint32_t)(now - _stateTimer) >= TUNING_TIMEOUT_MS) {
        enterState(State::VOLUME);
    }
}

void ArduinoRadio::updateVolume(uint32_t now) {
    if (_encoder.wasButtonPressed()) {
        enterState(State::TUNING);
        return;
    }

    int8_t rot = _encoder.getRotation();
    if (rot != 0) {
        _radio.volumeStep(rot > 0 ? 1 : -1);
        _stateTimer = now; // Reset inactivity timeout
        drawVolume();
    }

    if ((uint32_t)(now - _stateTimer) >= VOLUME_TIMEOUT_MS) {
        enterState(State::IDLE);
    }
}

// ---------------------------------------------------------------------------
// Private – display helpers
// ---------------------------------------------------------------------------

void ArduinoRadio::drawIdle() {
    _display.setLine(0, StationList::at(_stationIndex).name);

    char infoLine[17] = {};
    uint8_t rdsLen = static_cast<uint8_t>(strlen(_rdsText));

    if (rdsLen == 0) {
        // No RDS yet – show the tuned frequency
        formatFreq(_radio.getFrequency(), infoLine, sizeof(infoLine));
    } else if (rdsLen <= 16) {
        strncpy(infoLine, _rdsText, 16);
        infoLine[16] = '\0';
    } else {
        // Scrolling window into the RDS text
        strncpy(infoLine, _rdsText + _rdsScrollPos, 16);
        infoLine[16] = '\0';
    }

    _display.setLine(1, infoLine);
}

void ArduinoRadio::drawTuning() {
    const Station& st = StationList::at(_stationIndex);
    _display.setLine(0, st.name);

    char freqLine[17] = {};
    formatFreq(st.frequency, freqLine, sizeof(freqLine));
    _display.setLine(1, freqLine);
}

void ArduinoRadio::drawVolume() {
    _display.showVolume(_radio.getVolume());
}

// ---------------------------------------------------------------------------
// Private – RDS
// ---------------------------------------------------------------------------

void ArduinoRadio::refreshRDS() {
    char newText[65] = {};
    if (!_radio.getRDSProgramInfo(newText, sizeof(newText))) return;

    if (strcmp(_rdsText, newText) != 0) {
        strncpy(_rdsText, newText, sizeof(_rdsText) - 1);
        _rdsText[sizeof(_rdsText) - 1] = '\0';
        _rdsScrollPos = 0;
    } else {
        uint8_t len = static_cast<uint8_t>(strlen(_rdsText));
        if (len > 16) {
            uint8_t maxScroll = len - 16;
            _rdsScrollPos = (_rdsScrollPos >= maxScroll) ? 0 : _rdsScrollPos + 1;
        }
    }
}

// ---------------------------------------------------------------------------
// Private – formatting
// ---------------------------------------------------------------------------

void ArduinoRadio::formatFreq(uint16_t freq, char* buf, uint8_t bufSize) {
    if (bufSize < 10) {  // Need at least "NNN.N MHz\0" = 10 chars
        if (buf && bufSize > 0) buf[0] = '\0';
        return;
    }

    uint16_t mhz = freq / 100;
    uint8_t  dec = static_cast<uint8_t>((freq % 100) / 10);

    uint8_t i = 0;
    if (mhz >= 100) {
        buf[i++] = static_cast<char>('0' + mhz / 100);
        buf[i++] = static_cast<char>('0' + (mhz % 100) / 10);
        buf[i++] = static_cast<char>('0' + mhz % 10);
    } else {
        buf[i++] = ' ';
        buf[i++] = static_cast<char>('0' + mhz / 10);
        buf[i++] = static_cast<char>('0' + mhz % 10);
    }
    buf[i++] = '.';
    buf[i++] = static_cast<char>('0' + dec);
    buf[i++] = ' ';
    buf[i++] = 'M';
    buf[i++] = 'H';
    buf[i++] = 'z';
    buf[i]   = '\0';
}
