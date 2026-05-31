#pragma once
#include <RDA5807.h>
#include <stdint.h>
#include <ArduinoLog.h>

class FMRadio {
public:
    enum ScanState { IDLE, START_SCAN, SEEKING, EVALUATING };

    static constexpr uint8_t  VOLUME_MIN   = 0;
    static constexpr uint8_t  VOLUME_MAX   = 15;
    static constexpr uint8_t  MAX_STORED   = 30;
    static constexpr uint8_t POLLING_RDS   = 80;  // ms between RDS reads (matches reference sketch)
    static constexpr uint8_t SCAN_DELAY    = 100; // ms between scan steps
    static constexpr uint16_t EVAL_DELAY   = 600; // ms to wait after seek
    static constexpr uint8_t  RSSI_THRESHOLD = 30; // Minimum RSSI to consider a station "found"
    static constexpr bool     STEREO_SCAN_GATE = true; // true → wymaga także stereo (proxy SNR ≥ ~26 dB)

    ScanState getScanState() const { return _scanState; }

    void     begin(uint16_t startFreq, uint8_t startVolume = 8);
    void     update();
    void     setFrequency(uint16_t freq);
    uint16_t getFrequency() const;

    void    setVolume(uint8_t vol);
    void    volumeStep(int8_t direction);
    uint8_t getVolume() const;
    int8_t  getRSSI();

    bool getRDSDateTime(uint8_t& day, uint8_t& month, uint8_t& year,
                        uint8_t& hour, uint8_t& minute);

    void seek(bool up);
    void autoScan();

    uint8_t  getTotalFound() const { return _totalFound; }
    uint16_t getStoredStation(uint8_t index) const {
        return (index < _totalFound) ? _foundStations[index] : FREQ_MIN;
    }

    /// RDS data stored as fixed-size copied buffers — no raw library pointers.
    struct RDSData {
        char stationName[9]  = {};   ///< PS field  (max 8 chars + null)
        char radioText[65]   = {};   ///< RT field  (max 64 chars + null)
        char stationInfo[33] = {};   ///< SI field  (max 32 chars + null)
        char time[20]        = {};   ///< Date/time string (max 19 chars + null)

        uint8_t pty = 0;
        bool    tp  = false;

        bool stationValid = false;
        bool textValid    = false;
        bool timeValid    = false;

        /// Incremented each time radioText content changes — used by KNX layer.
        uint8_t textChangeCounter = 0;
    };

    /// Poll the RDA5807 library for new RDS data (call every loop).
    /// Internally rate-limited to one read every 80 ms (matching reference sketch).
    void updateRDS();

    const RDSData& getRDS() const { return _rds; }

private:
    RDA5807  _radio;
    uint16_t _frequency = 0;
    uint8_t  _volume    = 8;

    static constexpr uint16_t FREQ_MIN = 8700;
    static constexpr uint16_t FREQ_MAX = 10800;

    uint16_t  _foundStations[MAX_STORED] = {};
    uint8_t   _totalFound   = 0;
    ScanState _scanState    = IDLE;
    uint32_t  _lastScanAction = 0;
    uint16_t  _lastEvaluatedFreq = 0; ///< For stuck-seek detection during scan

    RDSData  _rds;
    uint32_t _lastRDSUpdate = 0;   ///< Timestamp of last 80 ms RDS poll

    // -----------------------------------------------------------------------
    // RDS quality filtering
    //
    // Validation-only approach: each PS/RT value is accepted on its first
    // valid read.  The consistency gate (require two identical reads) was
    // removed because many stations use dynamic/scrolling PS — consecutive
    // reads always differ — and news stations update RT so often that two
    // matching reads would rarely occur.
    // -----------------------------------------------------------------------

    /// Validate a PS (Programme Service) string.
    /// Accepts only [A-Za-z0-9 .,], rejects lowercase→uppercase mid-word
    /// transitions and consecutive duplicate punctuation.
    /// Returns false for all-whitespace or very short strings.
    static bool validatePS(const char* str);

    /// Validate an RT (Radio Text) string.
    /// Rejects non-ASCII, digit immediately followed by letter,
    /// letter followed by digit where the digit is not the last char of
    /// its "word" (not followed by space or end), lowercase→uppercase
    /// mid-word transitions, and consecutive identical punctuation.
    static bool validateRT(const char* str);

    void resetRDS();
};