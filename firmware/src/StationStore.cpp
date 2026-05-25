#include "StationStore.h"
#include <Arduino.h>

// Static member definitions
char    StationStore::_serialBuf[64] = {};
uint8_t StationStore::_serialPos     = 0;

// ---------------------------------------------------------------------------
// EEPROM helpers
// ---------------------------------------------------------------------------

bool StationStore::hasValidData() {
    return EEPROM.read(EEPROM_BASE) == MAGIC && EEPROM.read(EEPROM_BASE + 1) > 0;
}

uint8_t StationStore::load(uint16_t* dest, uint8_t maxLen) {
    if (!hasValidData()) return 0;

    uint8_t count = EEPROM.read(EEPROM_BASE + 1);
    if (count > maxLen) count = maxLen;

    for (uint8_t i = 0; i < count; ++i) {
        uint8_t hi = EEPROM.read(EEPROM_BASE + 2 + i * 2);
        uint8_t lo = EEPROM.read(EEPROM_BASE + 2 + i * 2 + 1);
        dest[i] = ((uint16_t)hi << 8) | lo;
    }
    return count;
}

void StationStore::save(const uint16_t* src, uint8_t count) {
    if (count > MAX_STATIONS) count = MAX_STATIONS;

    EEPROM.update(EEPROM_BASE,     MAGIC);
    EEPROM.update(EEPROM_BASE + 1, count);

    for (uint8_t i = 0; i < count; ++i) {
        EEPROM.update(EEPROM_BASE + 2 + i * 2,     (uint8_t)(src[i] >> 8));
        EEPROM.update(EEPROM_BASE + 2 + i * 2 + 1, (uint8_t)(src[i] & 0xFF));
    }
}

void StationStore::invalidate() {
    EEPROM.update(EEPROM_BASE, 0x00);
}

// ---------------------------------------------------------------------------
// Serial JSON interface
// ---------------------------------------------------------------------------

void StationStore::handleSerial(const uint16_t* stations, uint8_t count) {
    while (Serial.available()) {
        char c = (char)Serial.read();

        if (c == '\n' || c == '\r') {
            if (_serialPos == 0) continue;          // blank line
            _serialBuf[_serialPos] = '\0';
            _serialPos = 0;

            // Command: DUMP → send current stations as JSON
            if (strncmp(_serialBuf, "DUMP", 4) == 0) {
                dumpJSON(stations, count);
            }
            // Command: JSON array → load & save stations
            else if (_serialBuf[0] == '[') {
                uint16_t parsed[MAX_STATIONS];
                uint8_t  parsedCount = 0;
                if (parseJSON(_serialBuf, parsed, MAX_STATIONS, parsedCount)) {
                    save(parsed, parsedCount);
                    Serial.print(F("{\"ok\":true,\"saved\":"));
                    Serial.print(parsedCount);
                    Serial.println(F("}"));
                } else {
                    Serial.println(F("{\"ok\":false,\"error\":\"parse_failed\"}"));
                }
            }
        } else {
            if (_serialPos < sizeof(_serialBuf) - 1) {
                _serialBuf[_serialPos++] = c;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void StationStore::dumpJSON(const uint16_t* stations, uint8_t count) {
    // Output: {"stations":[{"freq":9600,"mhz":"96.0"},...],"count":N}
    Serial.print(F("{\"stations\":["));
    for (uint8_t i = 0; i < count; ++i) {
        if (i > 0) Serial.print(',');
        uint16_t f   = stations[i];
        uint16_t mhz = f / 100;
        uint8_t  dec = (f % 100) / 10;
        Serial.print(F("{\"freq\":"));
        Serial.print(f);
        Serial.print(F(",\"mhz\":\""));
        Serial.print(mhz);
        Serial.print('.');
        Serial.print(dec);
        Serial.print(F("\"}"));
    }
    Serial.print(F("],\"count\":"));
    Serial.print(count);
    Serial.println('}');
}

bool StationStore::parseJSON(const char* buf, uint16_t* dest, uint8_t maxLen, uint8_t& outCount) {
    // Accepts a plain frequency array: [9600,9980,10000]
    outCount = 0;
    const char* p = buf;

    while (*p && *p != '[') ++p;
    if (*p != '[') return false;
    ++p;

    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == ']') break;

        if (*p >= '0' && *p <= '9') {
            uint16_t val = 0;
            while (*p >= '0' && *p <= '9') {
                val = val * 10 + (*p - '0');
                ++p;
            }
            if (outCount < maxLen) {
                dest[outCount++] = val;
            }
        } else {
            ++p;  // skip commas, spaces, etc.
        }
    }
    return outCount > 0;
}
