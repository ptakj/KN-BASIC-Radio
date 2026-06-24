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

class ArduinoRadio {
public:
    void begin();
    void update();

private:
    enum class State : uint8_t { IDLE, TUNING, VOLUME, SCANNING };

    FMRadio         _radio;
    LCDDisplay      _display;
    EncoderInput    _encoder;
    KNXRadioControl _knx;

    State    _state        = State::IDLE;
    uint8_t  _stationIndex = 0;
    uint32_t _stateTimer   = 0;

    uint16_t _scannedFreqs[FMRadio::MAX_STORED];
    uint8_t  _scannedCount = 0;

    // -----------------------------------------------------------------------
    // IDLE RDS display state
    //
    //  Line 0 (static, refreshed every PS_REFRESH_MS):
    //    [PS name, left-justified, 12 chars] [CH number, 4 chars right]
    //
    //  Line 1 (animated, updated every RT_SCROLL_INTERVAL_MS):
    //    Scrolling radioText window (3 chars/s), or frequency if no RT.
    //    Frequency is left-justified, no leading space.
    // -----------------------------------------------------------------------
    uint16_t _rtScrollPos         = 0;  ///< Byte offset into RT for the scroll window
    uint32_t _rtScrollTimer       = 0;  ///< Timestamp of last scroll step
    uint32_t _psRefreshTimer      = 0;  ///< Timestamp of last PS line refresh
    uint8_t  _lastRtChangeCounter = 0;  ///< Detects RT content change → reset scroll

    static constexpr uint32_t RT_SCROLL_INTERVAL_MS = 1000; ///< One scroll step per second
    static constexpr uint32_t PS_REFRESH_MS         = 3000; ///< PS name refresh interval
    static constexpr uint32_t TUNING_TIMEOUT_MS     = 3000;
    static constexpr uint32_t VOLUME_TIMEOUT_MS     = 3000;

    void loadOrScan(bool onlyScan = false);
    void applyScannedStations(bool onlyScan = false);

    void enterState(State newState);

    void updateIdle(uint32_t now);
    void updateTuning(uint32_t now);
    void updateVolume(uint32_t now);
    void updateScanning(uint32_t now);

    /// Draw both IDLE lines at once (used on state entry).
    void drawIdle();
    /// Line 0: PS station name (left 12 chars) + CH number (right 4 chars).
    void drawIdleLine0();
    /// Line 1: RT scroll window (16 chars), or frequency left-justified as fallback.
    /// Advances _rtScrollPos by 3 on each call when RT is present and longer than 16 chars.
    void drawIdleLine1();

    void drawTuning();
    void drawVolume();
    void drawScanProgress();

    /// Format frequency without a leading space: "96.0MHz" or "106.3MHz".
    /// buf must be at least 9 bytes.
    void formatFreqShort(uint16_t freq, char* buf);

    /// Build a 16-char string "[name 8 chars][freq 8 chars]" used in TUNING line 0.
    void buildNameFreqLine(uint8_t index, char* out);

    uint8_t     stationCount() const;
    uint16_t    stationFreq(uint8_t index) const;
    const char* stationName(uint8_t index) const;

    static void formatFreq(uint16_t freq, char* buf, uint8_t bufSize);

    // -----------------------------------------------------------------------
    // KNX-driven change detection
    //
    // Snapshots of radio state taken after every _knx.update() call.
    // If KNX changed frequency or volume (seek / setVolume commands),
    // the display is updated to reflect the new state.
    // -----------------------------------------------------------------------
    uint16_t _lastKnxFreq = 0;
    uint8_t  _lastKnxVol  = 255;

    /// Compare current radio state against KNX snapshots.
    /// When a KNX command changed freq or vol, enters the appropriate
    /// display state (TUNING or VOLUME) so the LCD updates immediately.
    void checkKnxChanges();
};