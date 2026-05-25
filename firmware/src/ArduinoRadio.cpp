#include "ArduinoRadio.h"
#include <Arduino.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void ArduinoRadio::begin() {
    Serial.begin(9600);

    _display.begin();
    _encoder.begin(2, 3, 4); // CLK=D2, DT=D3, SW=D4

    loadOrScan();  // Load from EEPROM or run first-boot scan

    _stationIndex = 0;
    _radio.setFrequency(stationFreq(0));

    enterState(State::IDLE);
}

void ArduinoRadio::update() {
    _radio.update();
    uint32_t now = millis();
    _encoder.update();

    // Serial JSON interface: always active
    StationStore::handleSerial(_scannedFreqs, _scannedCount);

    switch (_state) {
        case State::IDLE:     updateIdle(now);     break;
        case State::TUNING:   updateTuning(now);   break;
        case State::VOLUME:   updateVolume(now);   break;
        case State::SCANNING: updateScanning(now); break;
    }
}

// ---------------------------------------------------------------------------
// Private – boot / scan helpers
// ---------------------------------------------------------------------------

void ArduinoRadio::loadOrScan() {
    if (StationStore::hasValidData()) {
        // --- Fast path: restore last scan from EEPROM ---
        _scannedCount = StationStore::load(_scannedFreqs, FMRadio::MAX_STORED);
        if (_scannedCount > 0) return;
    }

    // --- Slow path: autoscan (blocking startup scan) ---
    _display.setLine(0, "Skanowanie...");
    _display.setLine(1, "Prosze czekac");

    _radio.begin(8700, 8);
    _radio.autoScan();

    // Spin until scan finishes (FMRadio::update() drives the state machine)
    while (_radio.getScanState() != FMRadio::IDLE) {
        _radio.update();

        // Update display with live count
        char buf[17];
        sprintf(buf, "Znaleziono: %d", _radio.getTotalFound());
        _display.setLine(1, buf);
    }

    applyScannedStations();
}

void ArduinoRadio::applyScannedStations() {
    _scannedCount = _radio.getTotalFound();
    for (uint8_t i = 0; i < _scannedCount; ++i) {
        _scannedFreqs[i] = _radio.getStoredStation(i);
    }

    // Persist to EEPROM so next boot is instant
    if (_scannedCount > 0) {
        StationStore::save(_scannedFreqs, _scannedCount);
    }
}

// ---------------------------------------------------------------------------
// Private – station navigation helpers
// ---------------------------------------------------------------------------

uint8_t ArduinoRadio::stationCount() const {
    return (_scannedCount > 0) ? _scannedCount : StationList::size();
}

uint16_t ArduinoRadio::stationFreq(uint8_t index) const {
    if (_scannedCount > 0) {
        return (index < _scannedCount) ? _scannedFreqs[index] : _scannedFreqs[0];
    }
    return StationList::at(index).frequency;
}

const char* ArduinoRadio::stationName(uint8_t index) const {
    // When using scanned stations we don't have preset names,
    // so return an empty string and let drawTuning show the frequency.
    if (_scannedCount > 0) return "";
    return StationList::at(index).name;
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
        case State::SCANNING:
            _display.setLine(0, "Skanowanie...");
            _display.setLine(1, "Przytrzymaj=anuluj");
            break;
    }
}

void ArduinoRadio::updateIdle(uint32_t now) {
    // Long press → manual re-scan
    if (_encoder.wasLongPressed()) {
        StationStore::invalidate();
        _radio.autoScan();
        enterState(State::SCANNING);
        return;
    }

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

void ArduinoRadio::updateScanning(uint32_t now) {
    if (_radio.getScanState() == FMRadio::IDLE) {
        applyScannedStations();
        _stationIndex = 0;
        if (_scannedCount > 0) {
            _radio.setFrequency(_scannedFreqs[0]);
        }
        enterState(State::TUNING);
        return;
    }

    // Refresh progress display every 200 ms
    if ((uint32_t)(now - _stateTimer) >= 200) {
        _stateTimer = now;
        drawScanProgress();
    }
}

void ArduinoRadio::updateTuning(uint32_t now) {
    // Long press → manual re-scan
    if (_encoder.wasLongPressed()) {
        StationStore::invalidate();
        _radio.autoScan();
        enterState(State::SCANNING);
        return;
    }

    if (_encoder.wasButtonPressed()) {
        enterState(State::IDLE);
        return;
    }

    int8_t rot = _encoder.getRotation();
    if (rot != 0) {
        uint8_t total = stationCount();
        if (rot > 0) {
            _stationIndex = (_stationIndex + 1) % total;
        } else {
            _stationIndex = (_stationIndex == 0) ? (total - 1) : (_stationIndex - 1);
        }
        _radio.setFrequency(stationFreq(_stationIndex));
        _rdsText[0]   = '\0';
        _rdsScrollPos = 0;
        _stateTimer   = now;
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
        _stateTimer = now;
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
    // Line 0: station name (from preset) OR frequency (for scanned stations)
    if (_scannedCount > 0) {
        char freqLine[17] = {};
        formatFreq(stationFreq(_stationIndex), freqLine, sizeof(freqLine));
        _display.setLine(0, freqLine);
    } else {
        _display.setLine(0, StationList::at(_stationIndex).name);
    }

    // Line 1: RDS radio text (scrolling), or frequency
    char infoLine[17] = {};
    uint8_t rdsLen = static_cast<uint8_t>(strlen(_rdsText));

    if (rdsLen == 0) {
        formatFreq(_radio.getFrequency(), infoLine, sizeof(infoLine));
    } else if (rdsLen <= 16) {
        strncpy(infoLine, _rdsText, 16);
        infoLine[16] = '\0';
    } else {
        strncpy(infoLine, _rdsText + _rdsScrollPos, 16);
        infoLine[16] = '\0';
    }
    _display.setLine(1, infoLine);
}

void ArduinoRadio::drawTuning() {
    char freqLine[17] = {};
    formatFreq(stationFreq(_stationIndex), freqLine, sizeof(freqLine));

    if (_scannedCount > 0) {
        // No preset name – show index/total on line 0
        char indexLine[17] = {};
        sprintf(indexLine, "St. %d/%d", _stationIndex + 1, _scannedCount);
        _display.setLine(0, indexLine);
    } else {
        _display.setLine(0, StationList::at(_stationIndex).name);
    }
    _display.setLine(1, freqLine);
}

void ArduinoRadio::drawVolume() {
    _display.showVolume(_radio.getVolume());
}

void ArduinoRadio::drawScanProgress() {
    _display.setLine(0, "Skanowanie...");
    char buf[17];
    sprintf(buf, "Znaleziono: %d", _radio.getTotalFound());
    _display.setLine(1, buf);
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
    if (bufSize < 10) {
        if (buf && bufSize > 0) buf[0] = '\0';
        return;
    }
    uint16_t mhz = freq / 100;
    uint8_t  dec = static_cast<uint8_t>((freq % 100) / 10);
    uint8_t  i   = 0;
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
