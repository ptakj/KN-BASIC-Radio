#include "StationStore.h"
#include <Arduino.h>

// Static member definitions
uint8_t  StationStore::_parsedCount = 0;
uint16_t StationStore::_currentVal  = 0;
bool     StationStore::_inArray     = false;
bool     StationStore::_inNumber    = false;
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
        
        if (c == 'D') {
            if (!_inArray) dumpJSON(stations, count);
        } 
        else if (c == '[') {
            _inArray = true;
            _parsedCount = 0;
            _currentVal = 0;
            _inNumber = false;
            EEPROM.update(EEPROM_BASE, MAGIC); // Zapisujemy magic od razu
        } 
        else if (_inArray) {
            if (c >= '0' && c <= '9') {
                _currentVal = _currentVal * 10 + (c - '0');
                _inNumber = true;
            } 
            else if (c == ',' || c == ']') {
                if (_inNumber && _parsedCount < MAX_STATIONS) {
                    // Zapis liczby BEZPOŚREDNIO do EEPROM "w locie"
                    EEPROM.update(EEPROM_BASE + 2 + _parsedCount * 2,     (uint8_t)(_currentVal >> 8));
                    EEPROM.update(EEPROM_BASE + 2 + _parsedCount * 2 + 1, (uint8_t)(_currentVal & 0xFF));
                    _parsedCount++;
                }
                _currentVal = 0;
                _inNumber = false;
                
                if (c == ']') {
                    _inArray = false;
                    EEPROM.update(EEPROM_BASE + 1, _parsedCount); // Na koniec aktualizujemy licznik
                    Serial.print(F("{\"ok\":true,\"saved\":"));
                    Serial.print(_parsedCount);
                    Serial.println(F("}"));
                }
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