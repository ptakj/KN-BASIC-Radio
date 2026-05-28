#ifndef KNX_RADIO_CONTROL_H
#define KNX_RADIO_CONTROL_H

#include <Arduino.h>
#include "BAOS832.h"
#include "FMRadio.h"

class KNXRadioControl {
public:
    static constexpr uint16_t DP_CMD_CH_NEXT_PREV = 1;  // DPT 1.007
    static constexpr uint16_t DP_CMD_VOL_UP_DOWN  = 2;  // DPT 1.007
    static constexpr uint16_t DP_CMD_VOL_ABS      = 3;  // DPT 5.001
    static constexpr uint16_t DP_STAT_FREQ        = 4;  // DPT 9.001
    static constexpr uint16_t DP_STAT_RSSI        = 5;  // DPT 6.010
    static constexpr uint16_t DP_STAT_STATION     = 6;  // DPT 16.000
    static constexpr uint16_t DP_STAT_RDS_TEXT    = 7;  // DPT 16.000 (Marquee)
    static constexpr uint16_t DP_STAT_TIME        = 8;  // DPT 10.001
    static constexpr uint16_t DP_STAT_DATE        = 9;  // DPT 11.001

    KNXRadioControl(uint8_t rxPin = 6, uint8_t txPin = 5);
    ~KNXRadioControl();

    bool begin();
    void update(FMRadio* radio);

private:
    BAOS832 baos_;
    
    uint16_t _lastFreq;
    uint8_t  _lastVol;
    int8_t   _lastRssi;
    char     _lastStation[15];
    char     _lastRdsText[65];
    
    uint8_t  _rdsScrollOffset;
    uint32_t _lastRdsScrollMs;
    
    uint8_t  _lastHour, _lastMinute;
    uint8_t  _lastDay, _lastMonth, _lastYear;

    uint32_t _lastPollMs;
    static constexpr uint32_t POLL_INTERVAL_MS = 500; 
    static constexpr uint32_t SCROLL_INTERVAL_MS = 2000; 

    void handleKnxCommand(FMRadio* radio, uint16_t dpId, const uint8_t* data, uint8_t len);
    void sendFloatDPT9(uint16_t dpId, float value);
    void sendStringDPT16(uint16_t dpId, const char* str, uint8_t startIndex);
    void sendTimeDPT10(uint16_t dpId, uint8_t hour, uint8_t minute, uint8_t second);
    void sendDateDPT11(uint16_t dpId, uint8_t day, uint8_t month, uint8_t year);
    void floatToF16(float value, uint8_t* buffer);
};

#endif