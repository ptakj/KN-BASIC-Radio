#pragma once
#include <stdint.h>

/// A single FM preset station.
struct Station {
    const char* name;       ///< Display name (max 16 chars recommended)
    uint16_t    frequency;  ///< Frequency in units of 10 kHz (MHz × 100), e.g. 9600 = 96.0 MHz
};

/// Look-up table of Polish FM stations in the Wrocław area.
class StationList {
public:
    static const Station stations[];
    static const uint8_t count;

    /// Access a station by index (wraps around automatically).
    static const Station& at(uint8_t index);

    /// Number of stations in the table.
    static uint8_t size();
};
