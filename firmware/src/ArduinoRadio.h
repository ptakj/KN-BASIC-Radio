#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <ArduinoLog.h>
#include "FMRadio.h"
#include "LCDDisplay.h"
#include "EncoderInput.h"
#include "StationList.h"
#include "StationStore.h"
#include "KNXRadioControl.h"

/// Top-level application controller for the KN-BASIC FM radio.
class ArduinoRadio {
public:
    void begin();
    void update();

private:
    enum class State : uint8_t { IDLE, TUNING, VOLUME, SCANNING };

    FMRadio      _radio;
    LCDDisplay   _display;
    EncoderInput _encoder;
    KNXRadioControl _knx;

    State    _state        = State::IDLE;
    uint8_t  _stationIndex = 0;
    uint32_t _stateTimer   = 0;

    // Scanned-station working buffer (mirrors FMRadio internal array after scan)
    uint16_t _scannedFreqs[FMRadio::MAX_STORED];
    uint8_t  _scannedCount = 0;

    // RDS state (only used in IDLE)
    char     _rdsText[65]        = {};
    char     _rdsPS[16]          = {}; // Powiększony bufor do obsługi dłuższego PS/nazw
    uint16_t _rdsScrollPos       = 0;  // Licznik przewijania Radio Text
    uint16_t _nameScrollPos      = 0;  // Licznik przewijania Program Service / Nazwy presetów
    uint32_t _rdsScrollTimer     = 0;

    static constexpr uint32_t TUNING_TIMEOUT_MS      = 3000;
    static constexpr uint32_t VOLUME_TIMEOUT_MS      = 3000;
    static constexpr uint32_t RDS_SCROLL_INTERVAL_MS =  400; // Prędkość marquee (400ms na znak)

    // --- Boot helpers ---
    void loadOrScan(bool onlyScan = false);
    void applyScannedStations(bool onlyScan = false);

    // --- State machine ---
    void enterState(State newState);

    void updateIdle(uint32_t now);
    void updateTuning(uint32_t now);
    void updateVolume(uint32_t now);
    void updateScanning(uint32_t now);

    // --- Display helpers ---
    void drawIdle();
    void drawTuning();
    void drawVolume();
    void drawScanProgress();
    void formatFreqShort(uint16_t freq, char* buf);
    void buildNameFreqLine(uint8_t index, char* out);

    // --- RDS ---
    void refreshRDS();

    // --- Station navigation ---
    uint8_t stationCount() const;
    uint16_t stationFreq(uint8_t index) const;
    const char* stationName(uint8_t index) const;

    static void formatFreq(uint16_t freq, char* buf, uint8_t bufSize);
};