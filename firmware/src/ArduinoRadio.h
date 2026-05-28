#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "FMRadio.h"
#include "LCDDisplay.h"
#include "EncoderInput.h"
#include "StationList.h"
#include "StationStore.h"
#include "KNXRadioControl.h"

/// Top-level application controller for the KN-BASIC FM radio.
///
/// Boot sequence:
///   1. If EEPROM has valid scanned stations → load them, skip scan.
///   2. Otherwise → run autoScan(), save results to EEPROM.
///
/// State machine:
///   IDLE    + short press       → TUNING
///   IDLE    + rotation          → VOLUME  (apply 1 step immediately)
///   IDLE    + 2 s hold          → SCANNING (manual re-scan)
///   TUNING  + rotation          → TUNING  (cycle station, reset timeout)
///   TUNING  + short press       → IDLE
///   TUNING  + 3 s no rotation   → VOLUME
///   TUNING  + 2 s hold          → SCANNING
///   VOLUME  + rotation          → VOLUME  (change volume, reset timeout)
///   VOLUME  + short press       → TUNING
///   VOLUME  + 3 s no input      → IDLE
///   SCANNING + scan complete    → TUNING
///
/// LCD layout per state:
///   TUNING   – line 0: station name/freq   line 1: frequency
///   VOLUME   – line 0: "Volume: ##"        line 1: bar chart
///   IDLE     – line 0: station name        line 1: RDS text or frequency
///   SCANNING – line 0: "Skanowanie..."     line 1: "Znaleziono: N"
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
    char     _rdsPS[9]           = {};
    uint8_t  _rdsScrollPos       = 0;
    uint32_t _rdsScrollTimer     = 0;

    static constexpr uint32_t TUNING_TIMEOUT_MS      = 3000;
    static constexpr uint32_t VOLUME_TIMEOUT_MS      = 3000;
    static constexpr uint32_t RDS_SCROLL_INTERVAL_MS =  400;

    // --- Boot helpers ---
    void loadOrScan(bool onlyScan = false);   ///< Called from begin(): EEPROM load or fresh scan.
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
    /// Total stations available (scanned count, or preset fallback).
    uint8_t stationCount() const;
    /// Frequency of station at index (from scanned list or preset fallback).
    uint16_t stationFreq(uint8_t index) const;
    /// Display name of station at index.
    const char* stationName(uint8_t index) const;

    static void formatFreq(uint16_t freq, char* buf, uint8_t bufSize);
};
