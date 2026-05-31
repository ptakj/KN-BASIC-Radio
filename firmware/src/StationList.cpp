#include "StationList.h"

// Deklaracje stringów w PROGMEM
const char name0[] PROGMEM = "RMF FM";
const char name1[] PROGMEM = "Radio ZET";
const char name2[] PROGMEM = "PR1 Jedynka";
const char name3[] PROGMEM = "Eska Wroclaw";
const char name4[] PROGMEM = "Radio Wroclaw";
const char name5[] PROGMEM = "TOK FM";
const char name6[] PROGMEM = "PR3 Trojka";

// Deklaracja tablicy we Flash
const Station stations[] PROGMEM = {
    { name0, 9600 },
    { name1, 9980 },
    { name2, 10000 },
    { name3, 10080 },
    { name4, 10310 },
    { name5, 10470 },
    { name6, 10750 },
};

const uint8_t StationList::count = sizeof(stations) / sizeof(stations[0]);

Station StationList::at(uint8_t index) {
    index = index % count;
    Station s;
    // Odczytywanie wartości bezpośrednio z pamięci programu (Flash)
    s.name = (const char*)pgm_read_ptr(&stations[index].name);
    s.frequency = pgm_read_word(&stations[index].frequency);
    return s;
}

uint8_t StationList::size() {
    return count;
}