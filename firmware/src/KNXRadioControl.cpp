#include "KNXRadioControl.h"
#include <math.h>

KNXRadioControl::KNXRadioControl(uint8_t rxPin, uint8_t txPin) 
    : baos_(rxPin, txPin), 
      _lastFreq(0), _lastVol(255), _lastRssi(-127), _lastPollMs(0),
      _rdsScrollOffset(0), _lastRdsScrollMs(0), 
      _lastHour(255), _lastMinute(255), _lastDay(0), _lastMonth(0), _lastYear(0)
{
    memset(_lastStation, 0, sizeof(_lastStation));
    memset(_lastRdsText, 0, sizeof(_lastRdsText));
}

KNXRadioControl::~KNXRadioControl() {}

bool KNXRadioControl::begin() {
    return baos_.begin();
}

void KNXRadioControl::update(FMRadio* radio) {
    if (!radio) return;

    // --- 1. Odbiór danych KNX ---
    uint8_t payload[128];
    uint8_t size = 0;
    int16_t msgType = baos_.checkMessages(payload, &size);
    if (msgType == 0x00) {
        baos_.begin();
        return;
    }

    if (msgType == BAOS832::SUB_DP_VALUE_IND) {
        if (size >= 4) {
            uint16_t count = (payload[2] << 8) | payload[3];
            uint8_t offset = 4;
            
            for (uint16_t i = 0; i < count && offset < size; i++) {
                uint16_t dpId = (payload[offset] << 8) | payload[offset + 1];
                uint8_t  len  = payload[offset + 3];
                const uint8_t* valData = &payload[offset + 4];
                
                handleKnxCommand(radio, dpId, valData, len);
                offset += 4 + len; 
            }
        }
    }

    uint32_t now = millis();

    // --- 2. Odpytywanie radia i wysyłanie stanów (Feedback) ---
    if (now - _lastPollMs >= POLL_INTERVAL_MS) {
        _lastPollMs = now;

        uint16_t currentFreq = radio->getFrequency();
        if (currentFreq != _lastFreq) {
            _lastFreq = currentFreq;
            sendFloatDPT9(DP_STAT_FREQ, currentFreq / 100.0f);
        }

        uint8_t currentVol = radio->getVolume();
        if (currentVol != _lastVol) {
            _lastVol = currentVol;
            uint8_t valPercent = (currentVol * 100) / FMRadio::VOLUME_MAX;
            baos_.setDatapointValue(DP_CMD_VOL_ABS, 0x03, &valPercent, 1);
        }

        int8_t currentRssi = radio->getRSSI();
        if (abs(currentRssi - _lastRssi) > 2) { 
            _lastRssi = currentRssi;
            uint8_t rssiU8 = static_cast<uint8_t>(currentRssi);
            baos_.setDatapointValue(DP_STAT_RSSI, 0x03, &rssiU8, 1);
        }

        char currentStation[15] = {0};
        if (radio->getRDSStationName(currentStation, sizeof(currentStation))) {
            if (strcmp(currentStation, _lastStation) != 0) {
                strncpy(_lastStation, currentStation, sizeof(_lastStation));
                _lastStation[sizeof(_lastStation) - 1] = '\0';
                sendStringDPT16(DP_STAT_STATION, _lastStation, 0);
            }
        }

        char currentText[65] = {0};
        if (radio->getRDSProgramInfo(currentText, sizeof(currentText))) {
            if (strcmp(currentText, _lastRdsText) != 0) {
                strncpy(_lastRdsText, currentText, sizeof(_lastRdsText));
                _rdsScrollOffset = 0;
                _lastRdsScrollMs = now;
                sendStringDPT16(DP_STAT_RDS_TEXT, _lastRdsText, 0);
            }
        }

        uint8_t rdsD, rdsMo, rdsY, rdsH, rdsMi;
        if (radio->getRDSDateTime(rdsD, rdsMo, rdsY, rdsH, rdsMi)) {
            if (rdsH != _lastHour || rdsMi != _lastMinute) {
                _lastHour = rdsH; _lastMinute = rdsMi;
                sendTimeDPT10(DP_STAT_TIME, rdsH, rdsMi, 0);
            }
            if (rdsD != _lastDay || rdsMo != _lastMonth || rdsY != _lastYear) {
                _lastDay = rdsD; _lastMonth = rdsMo; _lastYear = rdsY;
                sendDateDPT11(DP_STAT_DATE, rdsD, rdsMo, rdsY);
            }
        }
    }

    // --- 3. Marquee text (Przewijanie RDS) ---
    uint8_t textLen = strlen(_lastRdsText);
    if (textLen > 14) {
        if (now - _lastRdsScrollMs >= SCROLL_INTERVAL_MS) {
            _lastRdsScrollMs = now;
            _rdsScrollOffset++;
            if (_rdsScrollOffset >= textLen) _rdsScrollOffset = 0; 
            sendStringDPT16(DP_STAT_RDS_TEXT, _lastRdsText, _rdsScrollOffset);
        }
    }
}

void KNXRadioControl::handleKnxCommand(FMRadio* radio, uint16_t dpId, const uint8_t* data, uint8_t len) {
    if (len == 0) return;
    switch (dpId) {
        case DP_CMD_CH_NEXT_PREV: radio->seek(data[0] != 0); break;
        case DP_CMD_VOL_UP_DOWN:  radio->volumeStep(data[0] ? 1 : -1); break;
        case DP_CMD_VOL_ABS:
            if (data[0] <= 100) radio->setVolume((data[0] * FMRadio::VOLUME_MAX) / 100);
            break;
    }
}

void KNXRadioControl::sendStringDPT16(uint16_t dpId, const char* str, uint8_t startIndex) {
    uint8_t buffer[14] = {0}; 
    uint16_t len = strlen(str);
    if (startIndex < len) {
        int copyLen = len - startIndex;
        if (copyLen > 14) copyLen = 14;
        strncpy(reinterpret_cast<char*>(buffer), str + startIndex, copyLen);
    } else {
        buffer[0] = ' '; // Odstęp zanim tekst wróci na początek
    }
    baos_.setDatapointValue(dpId, 0x03, buffer, 14);
}

void KNXRadioControl::sendFloatDPT9(uint16_t dpId, float value) {
    uint8_t buffer[2];
    floatToF16(value, buffer);
    baos_.setDatapointValue(dpId, 0x03, buffer, 2);
}

void KNXRadioControl::sendTimeDPT10(uint16_t dpId, uint8_t hour, uint8_t minute, uint8_t second) {
    uint8_t buffer[3];
    buffer[0] = hour & 0x1F; 
    buffer[1] = minute & 0x3F;
    buffer[2] = second & 0x3F;
    baos_.setDatapointValue(dpId, 0x03, buffer, 3);
}

void KNXRadioControl::sendDateDPT11(uint16_t dpId, uint8_t day, uint8_t month, uint8_t year) {
    uint8_t buffer[3];
    buffer[0] = day & 0x1F; 
    buffer[1] = month & 0x0F;
    buffer[2] = year & 0x7F; 
    baos_.setDatapointValue(dpId, 0x03, buffer, 3);
}

void KNXRadioControl::floatToF16(float value, uint8_t* buffer) {
    uint32_t v = static_cast<uint32_t>(round(abs(value) * 100.0f));
    int exp = 0;
    while (v > 2047 && exp < 15) { v >>= 1; exp++; }
    if (value < 0) { v = (~v + 1) & 0x07FF; }
    buffer[0] = ((exp & 0x0F) << 3) | ((v >> 8) & 0x07);
    if (value < 0) buffer[0] |= 0x80; 
    buffer[1] = v & 0xFF;
}