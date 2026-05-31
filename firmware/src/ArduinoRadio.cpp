#include "ArduinoRadio.h"
#include <avr/pgmspace.h>

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ArduinoRadio::begin() {
    _display.begin();

    // 1.5 s splash screen
    _display.setLine(0, "KNX Radio Tuner");
    _display.setLine(1, "");
    delay(1500);

    _encoder.begin(2, 3, 4);
    _radio.begin(8700, 8);
    loadOrScan(true);

    _stationIndex = 0;
    _radio.setFrequency(stationFreq(0));

    _knx.begin();

    enterState(State::IDLE);
}

void ArduinoRadio::update() {
    _radio.update();
    _radio.updateRDS();   // internally rate-limited to 80 ms
    _encoder.update();

    uint32_t now = millis();
    switch (_state) {
        case State::IDLE:     updateIdle(now);     break;
        case State::TUNING:   updateTuning(now);   break;
        case State::VOLUME:   updateVolume(now);   break;
        case State::SCANNING: updateScanning(now); break;
    }

    _knx.update(&_radio);
}

// ---------------------------------------------------------------------------
// Station loading / scanning
// ---------------------------------------------------------------------------

void ArduinoRadio::loadOrScan(bool onlyScan) {
    if (StationStore::hasValidData() && !onlyScan) {
        _scannedCount = StationStore::load(_scannedFreqs, FMRadio::MAX_STORED);
        if (_scannedCount > 0) return;
    }

    _display.setLine(0, "Scanning...");
    _display.setLine(1, "Please wait...");

    _radio.autoScan();

    while (_radio.getScanState() != FMRadio::IDLE) {
        _radio.update();
        char buf[17];
        snprintf(buf, sizeof(buf), "Found: %d", _radio.getTotalFound());
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
// Station helpers
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
// State machine
// ---------------------------------------------------------------------------

void ArduinoRadio::enterState(State newState) {
    _state      = newState;
    _stateTimer = millis();

    switch (newState) {
        case State::IDLE:
            _rtScrollPos         = 0;
            _rtScrollTimer       = _stateTimer;
            _psRefreshTimer      = _stateTimer;
            _lastRtChangeCounter = _radio.getRDS().textChangeCounter;
            drawIdle();
            break;

        case State::TUNING:
            drawTuning();
            break;

        case State::VOLUME:
            drawVolume();
            break;

        case State::SCANNING:
            _display.setLine(0, "Scanning...");
            _display.setLine(1, "Hold = cancel");
            break;
    }
}

// ---------------------------------------------------------------------------
// IDLE — update
// ---------------------------------------------------------------------------

void ArduinoRadio::updateIdle(uint32_t now) {
    // --- Input ---
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

    // --- Detect RT content change → reset scroll ---
    uint8_t cnt = _radio.getRDS().textChangeCounter;
    if (cnt != _lastRtChangeCounter) {
        _lastRtChangeCounter = cnt;
        _rtScrollPos         = 0;
    }

    // --- Line 1: RT scroll, one step per second ---
    if ((uint32_t)(now - _rtScrollTimer) >= RT_SCROLL_INTERVAL_MS) {
        _rtScrollTimer = now;
        drawIdleLine1();
    }

    // --- Line 0: PS name + CH number, refresh every 3 s ---
    if ((uint32_t)(now - _psRefreshTimer) >= PS_REFRESH_MS) {
        _psRefreshTimer = now;
        drawIdleLine0();
    }
}

// ---------------------------------------------------------------------------
// TUNING — update
// ---------------------------------------------------------------------------

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
        _rtScrollPos = 0;
        _stateTimer  = now;
        drawTuning();
    }

    if ((uint32_t)(now - _stateTimer) >= TUNING_TIMEOUT_MS) {
        enterState(State::VOLUME);
    }
}

// ---------------------------------------------------------------------------
// VOLUME — update
// ---------------------------------------------------------------------------

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
// SCANNING — update
// ---------------------------------------------------------------------------

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
    if ((uint32_t)(now - _stateTimer) >= 200) {
        _stateTimer = now;
        drawScanProgress();
    }
}

// ---------------------------------------------------------------------------
// IDLE draw helpers
// ---------------------------------------------------------------------------

void ArduinoRadio::drawIdle() {
    drawIdleLine0();
    drawIdleLine1();
}

// ---------------------------------------------------------------------------
// Line 0 layout (16 chars):
//   [  PS name, left-justified, 12 chars  ][ CH number, 4 chars ]
//
// CH number is right-aligned in 4 chars:
//   CH 1..9  → " CH1" .. " CH9"
//   CH10..20 → "CH10" .. "CH20"
//
// If PS is not yet available:
//   - scanned stations: 12 spaces (only CH visible)
//   - static list:      station name from PROGMEM, up to 12 chars
// ---------------------------------------------------------------------------
void ArduinoRadio::drawIdleLine0() {
    char line0[17] = {};

    // --- Right 4 chars: channel number ---
    uint8_t ch = _stationIndex + 1;
    char chBuf[5];
    if (ch < 10) {
        chBuf[0] = ' ';
        chBuf[1] = 'C';
        chBuf[2] = 'H';
        chBuf[3] = static_cast<char>('0' + ch);
        chBuf[4] = '\0';
    } else {
        chBuf[0] = 'C';
        chBuf[1] = 'H';
        chBuf[2] = static_cast<char>('0' + ch / 10);
        chBuf[3] = static_cast<char>('0' + ch % 10);
        chBuf[4] = '\0';
    }
    for (uint8_t i = 0; i < 4; i++) line0[12 + i] = chBuf[i];

    // --- Left 12 chars: station name ---
    const char* namePtr  = nullptr;
    char        flashBuf[32] = {};

    const FMRadio::RDSData& rds = _radio.getRDS();
    if (_scannedCount > 0) {
        // Use RDS PS if available; otherwise blank
        namePtr = (rds.stationValid && rds.stationName[0] != '\0')
                      ? rds.stationName
                      : "";
    } else {
        // Static list name from PROGMEM
        strcpy_P(flashBuf, StationList::at(_stationIndex).name);
        namePtr = flashBuf;
    }

    // Strip CR and trailing spaces; limit to 12
    uint8_t len = 0;
    while (namePtr[len] != '\0' && namePtr[len] != 0x0D && len < 12) len++;
    while (len > 0 && namePtr[len - 1] == ' ') len--;

    for (uint8_t i = 0; i < 12; i++) {
        if (i < len) {
            char c = namePtr[i];
            line0[i] = (c >= 0x20 && c < 0x7F) ? c : ' ';
        } else {
            line0[i] = ' ';
        }
    }

    line0[16] = '\0';
    _display.setLine(0, line0);
}

// ---------------------------------------------------------------------------
// Line 1 layout (16 chars):
//   - If RT available and length >= 2:
//       Scroll window of 16 chars, advancing 3 chars per second.
//       Text wraps with a 3-space gap before repeating.
//       If RT fits within 16 chars, shown statically (no scroll).
//   - Otherwise:
//       Frequency left-justified, e.g. "96.0MHz         "
// ---------------------------------------------------------------------------
void ArduinoRadio::drawIdleLine1() {
    const FMRadio::RDSData& rds = _radio.getRDS();
    char line1[17] = {};

    // --- Measure effective RT length ---
    uint8_t rtLen = 0;
    if (rds.textValid) {
        const char* rt = rds.radioText;
        while (rt[rtLen] != '\0' && rt[rtLen] != 0x0D && rtLen < 64) rtLen++;
        while (rtLen > 0 && rt[rtLen - 1] == ' ') rtLen--;
    }

    if (rtLen < 2) {
        // Fallback: frequency left-justified, no leading space
        char freqBuf[9];
        formatFreqShort(stationFreq(_stationIndex), freqBuf);
        uint8_t fLen = 0;
        while (freqBuf[fLen]) fLen++;
        for (uint8_t i = 0; i < 16; i++) {
            line1[i] = (i < fLen) ? freqBuf[i] : ' ';
        }
        line1[16] = '\0';
        _display.setLine(1, line1);
        return;
    }

    // Clamp to 61 chars (matches reference sketch)
    if (rtLen > 61) rtLen = 61;

    const char* rt = rds.radioText;

    if (rtLen <= 16) {
        // RT fits on screen — static display, no scroll advance
        for (uint8_t i = 0; i < 16; i++) {
            if (i < rtLen) {
                char c = rt[i];
                line1[i] = (c >= 0x20 && c < 0x7F) ? c : ' ';
            } else {
                line1[i] = ' ';
            }
        }
        line1[16] = '\0';
        _display.setLine(1, line1);
        return;
    }

    // RT longer than 16 chars — scroll with 3-space gap between wraps
    const uint8_t  gap      = 3;
    const uint16_t totalLen = rtLen + gap;
    uint16_t pos = _rtScrollPos % totalLen;

    for (uint8_t i = 0; i < 16; i++) {
        uint16_t ci = (pos + i) % totalLen;
        if (ci < rtLen) {
            char c = rt[ci];
            line1[i] = (c >= 0x20 && c < 0x7F) ? c : ' ';
        } else {
            line1[i] = ' ';
        }
    }
    line1[16] = '\0';
    _display.setLine(1, line1);

    // Advance by 3 chars; wrap back to 0 when a full cycle completes
    _rtScrollPos += 3;
    if (_rtScrollPos >= totalLen) _rtScrollPos = 0;
}

// ---------------------------------------------------------------------------
// Other state draw helpers
// ---------------------------------------------------------------------------

void ArduinoRadio::drawTuning() {
    // Line 0: [station name 8 chars][frequency 8 chars]
    char line0[17] = {};
    buildNameFreqLine(_stationIndex, line0);
    _display.setLine(0, line0);

    // Line 1: centred "< Ch: N/N >"
    uint8_t total   = stationCount();
    uint8_t current = _stationIndex + 1;
    char menuText[17] = {};
    snprintf(menuText, sizeof(menuText), "< Ch: %d/%d >", current, total);

    uint8_t len = 0;
    while (menuText[len] != '\0') len++;

    char line1[17] = {};
    for (uint8_t i = 0; i < 16; i++) line1[i] = ' ';
    line1[16] = '\0';

    uint8_t startPos = (16 - len) / 2;
    for (uint8_t i = 0; i < len && (startPos + i) < 16; i++) {
        line1[startPos + i] = menuText[i];
    }
    _display.setLine(1, line1);
}

void ArduinoRadio::drawVolume() {
    _display.showVolume(_radio.getVolume());
}

void ArduinoRadio::drawScanProgress() {
    _display.setLine(0, "Scanning...");
    char buf[17];
    snprintf(buf, sizeof(buf), "Found: %d", _radio.getTotalFound());
    _display.setLine(1, buf);
}

// ---------------------------------------------------------------------------
// Formatting utilities
// ---------------------------------------------------------------------------

/// Produces "96.0MHz" (7 chars) for sub-100 MHz, "106.3MHz" (8 chars) for 100+ MHz.
/// No leading space — frequency always starts at position 0.
/// buf must be at least 9 bytes.
void ArduinoRadio::formatFreqShort(uint16_t freq, char* buf) {
    uint16_t mhz = freq / 100;
    uint8_t  dec = static_cast<uint8_t>((freq % 100) / 10);
    uint8_t  i   = 0;

    if (mhz >= 100) {
        buf[i++] = static_cast<char>('0' + mhz / 100);
        buf[i++] = static_cast<char>('0' + (mhz % 100) / 10);
        buf[i++] = static_cast<char>('0' + mhz % 10);
    } else {
        // Two-digit integer part (87–99), left-justified, no padding space
        buf[i++] = static_cast<char>('0' + mhz / 10);
        buf[i++] = static_cast<char>('0' + mhz % 10);
    }
    buf[i++] = '.';
    buf[i++] = static_cast<char>('0' + dec);
    buf[i++] = 'M';
    buf[i++] = 'H';
    buf[i++] = 'z';
    buf[i]   = '\0';
}

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

/// Build the 16-char TUNING line 0: [name 8 chars][freq 8 chars].
/// Station name comes from RDS PS (scanned stations) or PROGMEM (static list).
/// Frequency is left-justified within its 8-char slot.
void ArduinoRadio::buildNameFreqLine(uint8_t index, char* out) {
    const char* namePtr  = "";
    char        flashBuf[32] = {};

    if (_scannedCount > 0) {
        const FMRadio::RDSData& rds = _radio.getRDS();
        namePtr = (rds.stationValid && rds.stationName[0] != '\0')
                      ? rds.stationName
                      : "--------";
    } else {
        strcpy_P(flashBuf, StationList::at(index).name);
        namePtr = flashBuf;
    }

    // Strip CR and trailing spaces; limit to 8
    uint8_t len = 0;
    while (namePtr[len] != '\0' && namePtr[len] != 0x0D && len < 8) len++;
    while (len > 0 && namePtr[len - 1] == ' ') len--;

    // Left 8 chars: station name left-justified
    for (uint8_t i = 0; i < 8; i++) {
        if (i < len) {
            char c = namePtr[i];
            out[i] = (c >= 0x20 && c < 0x7F) ? c : ' ';
        } else {
            out[i] = ' ';
        }
    }

    // Right 8 chars: frequency left-justified (no leading space), padded
    char freqBuf[9];
    formatFreqShort(stationFreq(index), freqBuf);
    uint8_t fLen = 0;
    while (freqBuf[fLen]) fLen++;
    for (uint8_t i = 0; i < 8; i++) {
        out[8 + i] = (i < fLen) ? freqBuf[i] : ' ';
    }
    out[16] = '\0';
}