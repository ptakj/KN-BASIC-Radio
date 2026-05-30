#pragma once
#include <stdint.h>
#include <EEPROM.h>

/// Persistent FM station storage in EEPROM.
///
/// Layout (starting at EEPROM_BASE):
///   [0]     magic byte  (0xFM = 0xF4)
///   [1]     count       (0–MAX_STATIONS)
///   [2..N]  frequencies  2 bytes each, big-endian (MHz × 100)
///
/// Total worst-case: 2 + 20×2 = 42 bytes  (well within 1 KB EEPROM)
///
/// Serial JSON commands (9600 baud):
///   Send "DUMP\n"  → Arduino replies with JSON array of frequencies
///   Send JSON array, e.g. [9600,9980,10000]\n  → Arduino loads & saves them
class StationStore {
public:
    static constexpr uint8_t  MAX_STATIONS  = 20;
    static constexpr uint8_t  EEPROM_BASE   = 0;
    static constexpr uint8_t  MAGIC         = 0xF4;

    /// Returns true when EEPROM contains valid previously-scanned data.
    static bool hasValidData();

    /// Load frequencies from EEPROM into dest[].  Returns count loaded.
    static uint8_t load(uint16_t* dest, uint8_t maxLen);

    /// Save count frequencies from src[] to EEPROM.
    static void save(const uint16_t* src, uint8_t count);

    /// Wipe the magic byte so next boot triggers a fresh scan.
    static void invalidate();

    /// Call once per loop() — handles Serial JSON commands.
    /// Requires Serial.begin() in setup().
    static void handleSerial(const uint16_t* stations, uint8_t count);

private:
    static void dumpJSON(const uint16_t* stations, uint8_t count);
    
    // Zmienne do maszyny stanów parsującej
    static uint8_t  _parsedCount;
    static uint16_t _currentVal;
    static bool     _inArray;
    static bool     _inNumber;
};
