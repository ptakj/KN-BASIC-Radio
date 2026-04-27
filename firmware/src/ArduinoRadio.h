#pragma once
#include <stdint.h>
#include "FMRadio.h"
#include "LCDDisplay.h"
#include "EncoderInput.h"
#include "StationList.h"

/// Top-level application controller for the KN-BASIC FM radio.
///
/// State machine:
///   IDLE   + button press    → TUNING
///   IDLE   + rotation        → VOLUME  (apply 1 step immediately)
///   TUNING + rotation        → TUNING  (cycle station, reset 1 s timeout)
///   TUNING + button press    → IDLE
///   TUNING + 1 s no rotation → VOLUME
///   VOLUME + rotation        → VOLUME  (change volume, reset 3 s timeout)
///   VOLUME + button press    → TUNING
///   VOLUME + 3 s no input    → IDLE
///
/// LCD layout per state:
///   TUNING  – line 0: station name   line 1: frequency
///   VOLUME  – line 0: "Volume: ##"   line 1: bar chart
///   IDLE    – line 0: station name   line 1: RDS radio-text (scrolling) or frequency
class ArduinoRadio {
public:
    void begin();
    void update();

private:
    enum class State : uint8_t { IDLE, TUNING, VOLUME };

    FMRadio      _radio;
    LCDDisplay   _display;
    EncoderInput _encoder;

    State    _state        = State::IDLE;
    uint8_t  _stationIndex = 0;
    uint32_t _stateTimer   = 0;

    // RDS state (only used in IDLE)
    char     _rdsText[65]        = {};
    uint8_t  _rdsScrollPos       = 0;
    uint32_t _rdsScrollTimer     = 0;

    static constexpr uint32_t TUNING_TIMEOUT_MS      = 1000;
    static constexpr uint32_t VOLUME_TIMEOUT_MS      = 3000;
    static constexpr uint32_t RDS_SCROLL_INTERVAL_MS =  400;

    void enterState(State newState);

    void updateIdle(uint32_t now);
    void updateTuning(uint32_t now);
    void updateVolume(uint32_t now);

    void drawIdle();
    void drawTuning();
    void drawVolume();

    void refreshRDS();

    /// Format a frequency (MHz × 100) as "NNN.N MHz" into buf (needs ≥ 10 bytes).
    static void formatFreq(uint16_t freq, char* buf, uint8_t bufSize);
};
