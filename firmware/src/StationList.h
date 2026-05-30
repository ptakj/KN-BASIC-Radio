#pragma once
#include <stdint.h>
#include <avr/pgmspace.h>

struct Station {
    const char* name;
    uint16_t    frequency;
};

class StationList {
public:
    static const uint8_t count;
    static Station at(uint8_t index); // Zwracamy kopię obiektu, bo oryginał jest we Flash
    static uint8_t size();
};