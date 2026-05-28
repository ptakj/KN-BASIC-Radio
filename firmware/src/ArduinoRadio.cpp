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

    // Serial JSON interface: always active
    StationStore::handleSerial(_scannedFreqs, _scannedCount);

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
        // --- Fast path: restore last scan from EEPROM ---
        _scannedCount = StationStore::load(_scannedFreqs, FMRadio::MAX_STORED);
        if (_scannedCount > 0) return;
    }

    // --- Slow path: autoscan (blocking startup scan) ---
    _display.setLine(0, "Skanowanie...");
    _display.setLine(1, "Prosze czekac");

    _radio.autoScan();

    // Spin until scan finishes (FMRadio::update() drives the state machine)
    while (_radio.getScanState() != FMRadio::IDLE) {
        _radio.update();

        // Update display with live count
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

    // Persist to EEPROM so next boot is instant
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
        _rdsPS[0]     = '\0';
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
    char line0[17] = {};
    buildNameFreqLine(_stationIndex, line0);
    _display.setLine(0, line0);

    // Wiersz 1: RDS (przewijany) lub czysta częstotliwość gdy brak RDS
    char line1[17] = {};
    uint8_t rdsLen = static_cast<uint8_t>(strlen(_rdsText));

    if (rdsLen == 0) {
        // Brak RDS – pokaż samą częstotliwość wyśrodkowaną
        formatFreq(_radio.getFrequency(), line1, sizeof(line1));
    } else if (rdsLen <= 16) {
        strncpy(line1, _rdsText, 16);
        line1[16] = '\0';
    } else {
        strncpy(line1, _rdsText + _rdsScrollPos, 16);
        line1[16] = '\0';
    }
    _display.setLine(1, line1);
}

void ArduinoRadio::drawTuning() {
    char line0[17] = {};
    buildNameFreqLine(_stationIndex, line0);
    _display.setLine(0, line0);

    // Wiersz 1: indeks stacji (dla skanowanych) lub pusta linia
    if (_scannedCount > 0) {
        char indexLine[17] = {};
        sprintf(indexLine, "St. %d/%d", _stationIndex + 1, _scannedCount);
        _display.setLine(1, indexLine);
    } else {
        _display.setLine(1, "");
    }
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
    // --- Program Service (nazwa stacji, max 8 znaków) ---
    char psRaw[9] = {};
    if (_radio.getRDSStationName(psRaw, sizeof(psRaw))) {
        char psClean[9] = {};
        uint8_t w = 0;
        for (uint8_t i = 0; i < 8 && psRaw[i] != '\0'; ++i) {
            char c = psRaw[i];
            if (c >= 0x20 && c < 0x7F) psClean[w++] = c;
        }
        // Usuń spacje z prawej strony
        while (w > 0 && psClean[w - 1] == ' ') --w;
        psClean[w] = '\0';

        if (w > 0 && strcmp(_rdsPS, psClean) != 0) {
            strncpy(_rdsPS, psClean, sizeof(_rdsPS) - 1);
            _rdsPS[sizeof(_rdsPS) - 1] = '\0';
        }
    }

    // --- Radio Text (aktualny utwór / info, max 64 znaki) ---
    char rtRaw[65] = {};
    if (!_radio.getRDSProgramInfo(rtRaw, sizeof(rtRaw))) return;

    // Sanityzacja: tylko printable ASCII, stop na 0x0D (RDS end-of-text)
    char rtClean[65] = {};
    uint8_t w = 0;
    for (uint8_t i = 0; i < 64 && rtRaw[i] != '\0'; ++i) {
        char c = rtRaw[i];
        if (c == 0x0D) break;           // marker końca RT w standardzie RDS
        if (c >= 0x20 && c < 0x7F) {   // tylko drukowalne ASCII
            rtClean[w++] = c;
        }
    }
    // Usuń spacje z prawej strony
    while (w > 0 && rtClean[w - 1] == ' ') --w;
    rtClean[w] = '\0';

    if (w == 0) return;   // nic wartościowego – nie aktualizuj

    if (strcmp(_rdsText, rtClean) != 0) {
        strncpy(_rdsText, rtClean, sizeof(_rdsText) - 1);
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

// Wpisuje do buf dokładnie 8 znaków: " 96.0MHz" lub "100.0MHz"
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

// Buduje "NazwaXXX 96.0MHz" (dokładnie 16 znaków)
void ArduinoRadio::buildNameFreqLine(uint8_t index, char* out) {
    // Wybierz źródło nazwy:
    //   preset → StationList
    //   skanowana → RDS PS (jeśli już odebrano), w przeciwnym razie puste
    const char* name;
    if (_scannedCount > 0) {
        name = (_rdsPS[0] != '\0') ? _rdsPS : "--------";
        //                                      ^^^^^^^^
        //                                      placeholder zanim RDS nadejdzie
    } else {
        name = StationList::at(index).name;
    }

    // Lewa część: 8 znaków, wyrównana do lewej, dopełniona spacjami
    uint8_t i = 0;
    for (; i < 8 && name[i] != '\0'; ++i) out[i] = name[i];
    for (; i < 8; ++i)                    out[i] = ' ';

    // Prawa część: 8 znaków częstotliwości
    formatFreqShort(stationFreq(index), out + 8);
}