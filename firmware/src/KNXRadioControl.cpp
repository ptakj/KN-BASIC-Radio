#include "KNXRadioControl.h"
#include <math.h>

KNXRadioControl::KNXRadioControl()
    : baos_(),
      _isConnected(false),
      _lastReconnectAttemptMs(0),
      _lastFreq(0),
      _lastVol(255),
      _lastRssi(-127),
      _lastRdsChangeCounter(255),
      _rdsScrollOffset(0),
      _lastRdsScrollMs(0),
      _lastHour(255),
      _lastMinute(255),
      _lastDay(0),
      _lastMonth(0),
      _lastYear(0),
      _lastPollMs(0)
{
    for (uint8_t i = 0; i < sizeof(_lastStation); i++) _lastStation[i] = '\0';
}

KNXRadioControl::~KNXRadioControl() {}

bool KNXRadioControl::begin() {
    Log.notice(F("KNXRadio: Initializing BAOS832..." CR));
    _isConnected = baos_.begin();
    return _isConnected;
}

void KNXRadioControl::update(FMRadio* radio) {
    if (!radio) return;

    uint32_t now = millis();

    if (!_isConnected) {
        if (now - _lastReconnectAttemptMs >= RECONNECT_INTERVAL_MS) {
            _lastReconnectAttemptMs = now;
            Log.notice(F("KNXRadio: Retrying connection to BAOS832..." CR));
            _isConnected = baos_.begin();
        }
        if (!_isConnected) return;
    }

    uint8_t  payload[40];
    uint8_t  size    = 0;
    int16_t  msgType = baos_.checkMessages(payload, &size);

    if (msgType == 0x00) {
        _isConnected = baos_.begin();
        return;
    }

    if (msgType == BAOS832::SUB_DP_VALUE_IND) {
        if (size >= 4) {
            uint16_t count  = (payload[2] << 8) | payload[3];
            uint8_t  offset = 4;
            for (uint16_t i = 0; i < count && (offset + 4) <= size; i++) {
                uint16_t       dpId    = (payload[offset] << 8) | payload[offset + 1];
                uint8_t        len     = payload[offset + 3];
                if ((offset + 4 + len) > size) break;
                const uint8_t* valData = &payload[offset + 4];
                handleKnxCommand(radio, dpId, valData, len);
                offset += 4 + len;
            }
        }
    }

    if (now - _lastPollMs >= POLL_INTERVAL_MS) {
        _lastPollMs = now;

        // --- Frequency ---
        uint16_t currentFreq = radio->getFrequency();
        if (currentFreq != _lastFreq) {
            _lastFreq = currentFreq;
            sendFloatDPT9(DP_STAT_FREQ, currentFreq / 100.0f);
        }

        // --- Volume ---
        uint8_t currentVol = radio->getVolume();
        if (currentVol != _lastVol) {
            _lastVol = currentVol;
            uint8_t valPercent = (currentVol * 100) / FMRadio::VOLUME_MAX;
            baos_.setDatapointValue(DP_CMD_VOL_ABS, 0x03, &valPercent, 1);
        }

        // --- RSSI ---
        int8_t currentRssi = radio->getRSSI();
        if (abs(currentRssi - _lastRssi) > 2) {
            _lastRssi = currentRssi;
            uint8_t rssiU8 = static_cast<uint8_t>(currentRssi);
            baos_.setDatapointValue(DP_STAT_RSSI, 0x03, &rssiU8, 1);
        }

        // --- RDS PS station name ---
        // NOTE: RDSData now uses char arrays — check first char, not pointer null.
        const FMRadio::RDSData& rds = radio->getRDS();
        if (rds.stationValid && rds.stationName[0] != '\0') {
            bool diff = false;
            for (uint8_t i = 0; i < 14; i++) {
                char c = rds.stationName[i];
                if (c != _lastStation[i]) diff = true;
                if (c == '\0' || c == 0x0D) break;
            }
            if (diff) {
                for (uint8_t i = 0; i < 14; i++) {
                    char c = rds.stationName[i];
                    _lastStation[i] = (c >= 0x20 && c < 0x7F) ? c : ' ';
                    if (rds.stationName[i] == '\0' || rds.stationName[i] == 0x0D) {
                        for (uint8_t j = i; j < 15; j++) _lastStation[j] = '\0';
                        break;
                    }
                }
                sendStringDPT16(DP_STAT_STATION, _lastStation, 0);
            }
        }

        // --- RDS date/time ---
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

    // --- RDS radio text with KNX scroll ---
    // NOTE: RDSData now uses char arrays — check first char, not pointer null.
    const FMRadio::RDSData& rds = radio->getRDS();
    if (rds.textValid && rds.radioText[0] != '\0') {

        if (rds.textChangeCounter != _lastRdsChangeCounter) {
            _lastRdsChangeCounter = rds.textChangeCounter;
            _rdsScrollOffset  = 0;
            _lastRdsScrollMs  = now;
            sendStringDPT16(DP_STAT_RDS_TEXT, rds.radioText, 0);
        }

        uint8_t textLen = 0;
        while (rds.radioText[textLen] != '\0'
               && rds.radioText[textLen] != 0x0D
               && textLen < 64) {
            textLen++;
        }

        if (textLen > 14) {
            if (now - _lastRdsScrollMs >= SCROLL_INTERVAL_MS) {
                _lastRdsScrollMs = now;
                _rdsScrollOffset++;
                if (_rdsScrollOffset >= textLen) _rdsScrollOffset = 0;
                sendStringDPT16(DP_STAT_RDS_TEXT, rds.radioText, _rdsScrollOffset);
            }
        }
    }
}

void KNXRadioControl::handleKnxCommand(FMRadio* radio, uint16_t dpId,
                                        const uint8_t* data, uint8_t len) {
    if (len == 0) return;
    switch (dpId) {
        case DP_CMD_CH_NEXT_PREV: radio->seek(data[0] != 0); break;
        case DP_CMD_VOL_UP_DOWN:  radio->volumeStep(data[0] ? 1 : -1); break;
        case DP_CMD_VOL_ABS:
            if (data[0] <= 100)
                radio->setVolume((data[0] * FMRadio::VOLUME_MAX) / 100);
            break;
    }
}

void KNXRadioControl::sendStringDPT16(uint16_t dpId, const char* str,
                                       uint8_t startIndex) {
    uint8_t buffer[14] = {};

    uint16_t len = 0;
    while (str[len] != '\0' && str[len] != 0x0D && len < 64) len++;

    if (startIndex < len) {
        int copyLen = len - startIndex;
        if (copyLen > 14) copyLen = 14;
        for (int i = 0; i < copyLen; i++) {
            char c = str[startIndex + i];
            buffer[i] = (c >= 0x20 && c < 0x7F) ? c : ' ';
        }
    } else {
        buffer[0] = ' ';
    }
    baos_.setDatapointValue(dpId, 0x03, buffer, 14);
}

void KNXRadioControl::sendFloatDPT9(uint16_t dpId, float value) {
    uint8_t buffer[2];
    floatToF16(value, buffer);
    baos_.setDatapointValue(dpId, 0x03, buffer, 2);
}

void KNXRadioControl::sendTimeDPT10(uint16_t dpId, uint8_t hour,
                                     uint8_t minute, uint8_t second) {
    uint8_t buffer[3];
    buffer[0] = hour   & 0x1F;
    buffer[1] = minute & 0x3F;
    buffer[2] = second & 0x3F;
    baos_.setDatapointValue(dpId, 0x03, buffer, 3);
}

void KNXRadioControl::sendDateDPT11(uint16_t dpId, uint8_t day,
                                     uint8_t month, uint8_t year) {
    uint8_t buffer[3];
    buffer[0] = day   & 0x1F;
    buffer[1] = month & 0x0F;
    buffer[2] = year  & 0x7F;
    baos_.setDatapointValue(dpId, 0x03, buffer, 3);
}

void KNXRadioControl::floatToF16(float value, uint8_t* buffer) {
    uint32_t v = static_cast<uint32_t>(round(fabsf(value) * 100.0f));
    int exp = 0;
    while (v > 2047 && exp < 15) { v >>= 1; exp++; }
    if (value < 0) { v = (~v + 1) & 0x07FF; }
    buffer[0] = static_cast<uint8_t>(((exp & 0x0F) << 3) | ((v >> 8) & 0x07));
    if (value < 0) buffer[0] |= 0x80;
    buffer[1] = static_cast<uint8_t>(v & 0xFF);
}