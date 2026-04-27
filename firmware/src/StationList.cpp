#include "StationList.h"

// Polish FM stations receivable in the Wrocław area.
// Frequencies are in MHz × 100 (e.g. 9600 = 96.0 MHz).
const Station StationList::stations[] = {
    { "RMF FM",         9600 },   // 96.0 MHz
    { "Radio ZET",      9980 },   // 99.8 MHz
    { "PR1 Jedynka",   10000 },   // 100.0 MHz
    { "Eska Wroclaw",  10080 },   // 100.8 MHz
    { "Radio Wroclaw", 10310 },   // 103.1 MHz
    { "TOK FM",        10470 },   // 104.7 MHz
    { "PR3 Trojka",    10750 },   // 107.5 MHz
};

const uint8_t StationList::count =
    sizeof(StationList::stations) / sizeof(StationList::stations[0]);

const Station& StationList::at(uint8_t index) {
    return stations[index % count];
}

uint8_t StationList::size() {
    return count;
}
