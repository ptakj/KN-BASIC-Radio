#pragma once
#include <RDA5807.h>
#include <stdint.h>
#include <ArduinoLog.h>

class FMRadio {
public:
    enum ScanState { IDLE, START_SCAN, SEEKING, EVALUATING };

    static constexpr uint8_t  VOLUME_MIN   = 0;
    static constexpr uint8_t  VOLUME_MAX   = 15;
    static constexpr uint8_t  MAX_STORED   = 20;

    ScanState getScanState() const { return _scanState; }

    void     begin(uint16_t startFreq, uint8_t startVolume = 8);
    void     update();
    void     setFrequency(uint16_t freq);
    uint16_t getFrequency() const;

    void    setVolume(uint8_t vol);
    void    volumeStep(int8_t direction);
    uint8_t getVolume() const;
    int8_t  getRSSI();

    bool getRDSStationName(char* buffer, uint8_t bufSize);
    bool getRDSProgramInfo(char* buffer, uint8_t bufSize);
    
    // Nowa metoda wyciągająca pełną datę i czas
    bool getRDSDateTime(uint8_t& day, uint8_t& month, uint8_t& year, uint8_t& hour, uint8_t& minute);

    void seek(bool up);
    void autoScan();

    uint8_t  getTotalFound() const { return _totalFound; }
    uint16_t getStoredStation(uint8_t index) const {
        return (index < _totalFound) ? _foundStations[index] : FREQ_MIN;
    }

    struct RDSData {
        char stationName[16] = "";
        char radioText[65]   = "";
        char stationInfo[33] = "";
        char time[24]        = "";

        uint8_t pty = 0;
        bool tp = false;

        bool stationValid = false;
        bool textValid = false;
        bool timeValid = false;
        
        uint8_t textChangeCounter = 0; // Dodano: licznik zmiany tekstu (pomaga innym klasom)
    };
    
    void updateRDS();

    const RDSData& getRDS() const {
        return _rds;
    }

    bool getStationName(char* buffer, uint8_t size);
    bool getRadioText(char* buffer, uint8_t size);

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

    RDSData _rds;

    char _lastRawPS[16] = "";
    uint32_t _lastRDSUpdate = 0;
    char _stableStationName[16];
    uint8_t _sameNameCounter = 0;

    bool isDynamicPS(const char* txt);
    void sanitizePS(const char* src, char* dst, uint8_t size);
    void resetRDS();
};