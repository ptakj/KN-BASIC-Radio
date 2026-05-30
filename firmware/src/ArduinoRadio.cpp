#include "ArduinoRadio.h"
#include <Arduino.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void ArduinoRadio::begin() {
    _display.begin();
    _encoder.begin(2, 3, 4); // CLK=D2, DT=D3, SW=D4

    _radio.begin(8700, 8);
    loadOrScan(true);  // Load from EEPROM or run first-boot scan

    _stationIndex = 0;
    _radio.setFrequency(stationFreq(0));

    _knx.begin();

    enterState(State::IDLE);
}

void ArduinoRadio::update() {
    _radio.update();
    uint32_t now = millis();
    _encoder.update();

    switch (_state) {
        case State::IDLE:     updateIdle(now);     break;
        case State::TUNING:   updateTuning(now);   break;
        case State::VOLUME:   updateVolume(now);   break;
        case State::SCANNING: updateScanning(now); break;
    }

    _knx.update(&_radio);
}

// ---------------------------------------------------------------------------
// Private – boot / scan helpers
// ---------------------------------------------------------------------------

void ArduinoRadio::loadOrScan(bool onlyScan) {
    if (StationStore::hasValidData() && !onlyScan) {
        _scannedCount = StationStore::load(_scannedFreqs, FMRadio::MAX_STORED);
        if (_scannedCount > 0) return;
    }

    _display.setLine(0, "Skanowanie...");
    _display.setLine(1, "Prosze czekac");

    _radio.autoScan();

    while (_radio.getScanState() != FMRadio::IDLE) {
        _radio.update();
        char buf[17];
        sprintf(buf, "Znaleziono: %d", _radio.getTotalFound());
        _display.setLine(1, buf);
    }

    applyScannedStations(onlyScan);
}

void ArduinoRadio::applyScannedStations(bool onlyScan) {
    _scannedCount = _radio.getTotalFound();
    for (uint8_t i = 0; i < _scannedCount; ++i) {
        _scannedFreqs[i] = _radio.getStoredStation(i);
    }

    if (_scannedCount > 0 && !onlyScan) {
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
            _rdsScrollTimer = millis(); // Restart timera (nie zerujemy pozycji, by przewijanie było płynne po opuszczeniu VOLUME)
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

    // Bezwarunkowe odświeżanie przewijania obu linii
    if ((uint32_t)(now - _rdsScrollTimer) >= RDS_SCROLL_INTERVAL_MS) {
        _rdsScrollTimer = now;
        refreshRDS();
        _nameScrollPos++;
        _rdsScrollPos++;
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
        _rdsScrollPos = 0;
        _nameScrollPos = 0;
        enterState(State::TUNING);
        return;
    }

    if ((uint32_t)(now - _stateTimer) >= 200) {
        _stateTimer = now;
        drawScanProgress();
    }
}

void ArduinoRadio::updateTuning(uint32_t now) {
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
        _rdsPS[0]     = '\0';
        _rdsScrollPos = 0;
        _nameScrollPos = 0; // Reset scrollowania po zmianie stacji
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
    char line0[17] = {};
    buildNameFreqLine(_stationIndex, line0);
    _display.setLine(0, line0);

    char line1[17] = {};
    char prBuf[8];
    snprintf(prBuf, sizeof(prBuf), " PR:%d", _stationIndex + 1);
    uint8_t prLen = static_cast<uint8_t>(strlen(prBuf));
    uint8_t rdsSpace = 16 - prLen; // Przestrzeń pod Marquee dla Radio Textu (ok. 11 znaków)

    char rdsBuf[17] = {};
    uint8_t rdsLen = static_cast<uint8_t>(strlen(_rdsText));

    if (rdsLen == 0) {
        memset(rdsBuf, ' ', rdsSpace);
    } else if (rdsLen <= rdsSpace) {
        strncpy(rdsBuf, _rdsText, rdsSpace);
        for (uint8_t i = rdsLen; i < rdsSpace; i++) rdsBuf[i] = ' ';
    } else {
        // --- Algorytm Marquee dla Radio Textu ---
        uint8_t gap = 3; 
        uint8_t totalLen = rdsLen + gap;
        uint16_t currentPos = _rdsScrollPos % totalLen;
        
        for (uint8_t i = 0; i < rdsSpace; i++) {
            uint16_t charIdx = (currentPos + i) % totalLen;
            if (charIdx < rdsLen) {
                rdsBuf[i] = _rdsText[charIdx];
            } else {
                rdsBuf[i] = ' '; // Luka na końcu komunikatu
            }
        }
    }
    rdsBuf[rdsSpace] = '\0';

    snprintf(line1, sizeof(line1), "%s%s", rdsBuf, prBuf);
    _display.setLine(1, line1);
}

void ArduinoRadio::drawTuning() {
    char line0[17] = {};
    buildNameFreqLine(_stationIndex, line0);
    _display.setLine(0, line0);

    char line1[17] = {};
    uint8_t total = stationCount();
    uint8_t current = _stationIndex + 1;
    
    char menuText[17] = {};
    snprintf(menuText, sizeof(menuText), "< Stacja: %d/%d >", current, total);
    
    uint8_t len = static_cast<uint8_t>(strlen(menuText));
    memset(line1, ' ', 16);
    line1[16] = '\0';
    
    uint8_t startPos = (16 - len) / 2;
    strncpy(line1 + startPos, menuText, len);
    
    _display.setLine(1, line1);
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
    // --- Program Service (Nazwa stacji, max 15 znaków obsługiwane w marquee) ---
    char psRaw[16] = {};
    if (_radio.getRDSStationName(psRaw, sizeof(psRaw))) {
        char psClean[16] = {};
        uint8_t w = 0;
        for (uint8_t i = 0; i < 15 && psRaw[i] != '\0'; ++i) {
            char c = psRaw[i];
            if (c >= 0x20 && c < 0x7F) psClean[w++] = c;
        }
        while (w > 0 && psClean[w - 1] == ' ') --w;
        psClean[w] = '\0';

        if (w > 0 && strcmp(_rdsPS, psClean) != 0) {
            strncpy(_rdsPS, psClean, sizeof(_rdsPS) - 1);
            _rdsPS[sizeof(_rdsPS) - 1] = '\0';
            _nameScrollPos = 0; // Reset scrolla na nową nazwę
        }
    }

    // --- Radio Text (Tekst stacji, max 64 znaki) ---
    char rtRaw[65] = {};
    if (!_radio.getRDSProgramInfo(rtRaw, sizeof(rtRaw))) return;

    char rtClean[65] = {};
    uint8_t w = 0;
    for (uint8_t i = 0; i < 64 && rtRaw[i] != '\0'; ++i) {
        char c = rtRaw[i];
        if (c == 0x0D) break;          
        if (c >= 0x20 && c < 0x7F) {   
            rtClean[w++] = c;
        }
    }
    while (w > 0 && rtClean[w - 1] == ' ') --w;
    rtClean[w] = '\0';

    if (w == 0) return;   

    if (strcmp(_rdsText, rtClean) != 0) {
        strncpy(_rdsText, rtClean, sizeof(_rdsText) - 1);
        _rdsText[sizeof(_rdsText) - 1] = '\0';
        _rdsScrollPos = 0; // Reset scrolla na nowy tekst
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

void ArduinoRadio::formatFreqShort(uint16_t freq, char* buf) {
    uint16_t mhz = freq / 100;
    uint8_t  dec = (freq % 100) / 10;
    if (mhz < 100) {
        buf[0] = ' ';
        buf[1] = '0' + (mhz / 10);
        buf[2] = '0' + (mhz % 10);
    } else {
        buf[0] = '0' + (mhz / 100);
        buf[1] = '0' + ((mhz % 100) / 10);
        buf[2] = '0' + (mhz % 10);
    }
    buf[3] = '.';
    buf[4] = '0' + dec;
    buf[5] = 'M';
    buf[6] = 'H';
    buf[7] = 'z';
    buf[8] = '\0';
}

void ArduinoRadio::buildNameFreqLine(uint8_t index, char* out) {
    char name[32] = {};
    
    if (_scannedCount > 0) {
        strncpy(name, (_rdsPS[0] != '\0') ? _rdsPS : "--------", sizeof(name) - 1);
    } else {
        // Poprawne ładowanie stringów z PROGMEM
        strcpy_P(name, StationList::at(index).name);
    }

    uint8_t len = static_cast<uint8_t>(strlen(name));
    char leftPart[9] = {};
    
    if (len <= 8) {
        // Jeśli długość <= 8 znaków - stały tekst bez rotacji
        strncpy(leftPart, name, 8);
        for (uint8_t i = len; i < 8; i++) leftPart[i] = ' ';
    } else {
        // --- Algorytm Marquee dla nazwy stacji / PS (wyświetlane pole: 8 znaków) ---
        uint8_t gap = 3; 
        uint8_t totalLen = len + gap;
        uint16_t currentPos = _nameScrollPos % totalLen;
        
        for (uint8_t i = 0; i < 8; i++) {
            uint16_t charIdx = (currentPos + i) % totalLen;
            if (charIdx < len) {
                leftPart[i] = name[charIdx];
            } else {
                leftPart[i] = ' '; // Luka między końcem a początkiem nazwy
            }
        }
    }
    leftPart[8] = '\0';

    // Skopiuj sformatowaną w lewej części nazwę
    for (uint8_t i = 0; i < 8; i++) out[i] = leftPart[i];

    // Prawa część: 8 znaków częstotliwości
    formatFreqShort(stationFreq(index), out + 8);
}