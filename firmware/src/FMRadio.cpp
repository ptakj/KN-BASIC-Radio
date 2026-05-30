#include "FMRadio.h"
#include <string.h>

void FMRadio::seek(bool up) {
    uint8_t direction = up ? 1 : 0;
    Log.notice(F("FMRadio: Seeking %s..." CR), up ? "UP" : "DOWN");
    _radio.seek(1, direction); 
    delay(100); 
    _frequency = _radio.getRealFrequency();
    resetRDS(); 
    Log.notice(F("FMRadio: Tuned to %d.%02d MHz" CR), _frequency / 100, _frequency % 100);
}

void FMRadio::autoScan() {
    Log.notice(F("FMRadio: Starting auto-scan..." CR));
    _totalFound = 0;
    setFrequency(FREQ_MIN);
    _lastScanAction = millis(); 
    _scanState = START_SCAN;    
}

void FMRadio::update() {
    if (_scanState == IDLE) return; 
    uint32_t now = millis();

    switch (_scanState) {
        case START_SCAN:
        case SEEKING:
            if (now - _lastScanAction >= 100) {
                _radio.seek(1, 1);
                _lastScanAction = now;
                _scanState = EVALUATING;
            }
            break;

        case EVALUATING:
            if (now - _lastScanAction >= 600) {
                uint16_t foundFreq = _radio.getRealFrequency();
                if (foundFreq <= FREQ_MIN || (_totalFound > 0 && foundFreq <= _foundStations[_totalFound - 1])) {
                    Log.notice(F("FMRadio: Auto-scan complete. Found: %d" CR), _totalFound);
                    _scanState = IDLE; 
                    break;
                }
                if (_radio.getRssi() > 22) {
                    if (_totalFound < MAX_STORED) {
                        _foundStations[_totalFound++] = foundFreq;
                        Log.verbose(F("FMRadio: Found %d.%02d MHz" CR), foundFreq/100, foundFreq%100);
                    } else {
                        _scanState = IDLE;
                        break;
                    }
                }
                _lastScanAction = now;
                _scanState = SEEKING; 
            }
            break;
        default:
            break;
    }
}

void FMRadio::begin(uint16_t startFreq, uint8_t startVolume) {
    _radio.setup();

    _radio.setRDS(true);
    _radio.setRdsFifo(true);

    _radio.clearRdsBuffer();

    _volume = (startVolume > VOLUME_MAX) ? VOLUME_MAX : startVolume;
    _radio.setVolume(_volume);
    _radio.setMute(false);
    setFrequency(startFreq);
}

void FMRadio::setFrequency(uint16_t freq) {
    if (freq < FREQ_MIN) freq = FREQ_MIN;
    if (freq > FREQ_MAX) freq = FREQ_MAX;

    if (_frequency == freq)
        return;

    _frequency = freq;

    _radio.setFrequency(_frequency);

    resetRDS();
}

uint16_t FMRadio::getFrequency() const { return _frequency; }

void FMRadio::setVolume(uint8_t vol) {
    if (vol > VOLUME_MAX) vol = VOLUME_MAX;
    _volume = vol;
    _radio.setVolume(_volume);
}

void FMRadio::volumeStep(int8_t direction) {
    if (direction > 0 && _volume < VOLUME_MAX) setVolume(_volume + 1);
    if (direction < 0 && _volume > VOLUME_MIN) setVolume(_volume - 1);
}

uint8_t FMRadio::getVolume() const { return _volume; }
int8_t FMRadio::getRSSI() { return static_cast<int8_t>(_radio.getRssi()); }

bool FMRadio::getRDSStationName(char* buffer, uint8_t bufSize) {
    if (!buffer || bufSize == 0) return false;
    buffer[0] = '\0';
    if (!_radio.getRdsReady()) return false;
    char* ps = _radio.getRdsStationName();
    if (!ps || ps[0] == '\0') return false;
    strncpy(buffer, ps, bufSize - 1);
    buffer[bufSize - 1] = '\0';
    return true;
}

bool FMRadio::getRDSProgramInfo(char* buffer, uint8_t bufSize) {
    if (!buffer || bufSize == 0) return false;
    buffer[0] = '\0';
    if (!_radio.getRdsReady()) return false;
    char* rt = _radio.getRdsProgramInformation();
    if (!rt || rt[0] == '\0') return false;
    strncpy(buffer, rt, bufSize - 1);
    buffer[bufSize - 1] = '\0';
    return true;
}

bool FMRadio::getRDSDateTime(uint8_t& day, uint8_t& month, uint8_t& year, uint8_t& hour, uint8_t& minute) {
    if (!_radio.getRdsReady()) return false;
    char* t = _radio.getRdsTime();
    if (!t || strlen(t) < 16) return false; 
    
    // Format PU2CLR: "YYYY-MM-DD HH:MM"
    year   = (t[2] - '0') * 10 + (t[3] - '0');
    month  = (t[5] - '0') * 10 + (t[6] - '0');
    day    = (t[8] - '0') * 10 + (t[9] - '0');
    hour   = (t[11] - '0') * 10 + (t[12] - '0');
    minute = (t[14] - '0') * 10 + (t[15] - '0');
    
    return (month > 0 && month <= 12 && day > 0 && day <= 31 && hour < 24 && minute < 60);
}

void FMRadio::sanitizePS(
    const char* src,
    char* dst,
    uint8_t size) {
    if (!src || !dst || size < 2)
        return;

    uint8_t j = 0;

    for (uint8_t i = 0; src[i] && j < size - 1; i++)
    {
        char c = src[i];

        if ((uint8_t)c < 32)
            continue;

        dst[j++] = c;
    }

    dst[j] = 0;

    while (j > 0 && dst[j - 1] == ' ')
    {
        dst[j - 1] = 0;
        j--;
    }
}

bool FMRadio::isDynamicPS(const char* txt) {
    if (!txt)
        return false;

    uint8_t digits = 0;

    for (uint8_t i = 0; txt[i]; i++)
    {
        if (isdigit(txt[i]))
            digits++;
    }

    if (digits >= 2)
        return true;

    if (strchr(txt, ':'))
        return true;

    return false;
}

void FMRadio::updateRDS()
{
    if (!_radio.getRdsReady())
        return;

    char* ps  = _radio.getRdsStationName();
    char* rt  = _radio.getRdsProgramInformation();
    char* si  = _radio.getRdsStationInformation();
    char* tim = _radio.getRdsTime();

    if (ps && ps[0])
    {
        char cleaned[16];

        sanitizePS(
            ps,
            cleaned,
            sizeof(cleaned));

        if (!isDynamicPS(cleaned))
        {
            if (strcmp(
                    cleaned,
                    _rds.stationName) != 0)
            {
                strncpy(
                    _rds.stationName,
                    cleaned,
                    sizeof(_rds.stationName)-1);

                _rds.stationValid = true;
            }
        }
    }

    if (rt && rt[0])
    {
        // Czyścimy i parsujemy tekst już tutaj, by inne moduły nie musiały tworzyć kopii
        char rtClean[65] = {};
        uint8_t w = 0;
        for (uint8_t i = 0; i < 64 && rt[i] != '\0'; ++i) {
            char c = rt[i];
            if (c == 0x0D) break;          // marker końca RT wg. standardu RDS
            if (c >= 0x20 && c < 0x7F) {   // tylko drukowalne ASCII
                rtClean[w++] = c;
            }
        }
        while (w > 0 && rtClean[w - 1] == ' ') --w;
        rtClean[w] = '\0';

        // Aktualizacja tylko przy nowym tekście
        if (w > 0 && strcmp(_rds.radioText, rtClean) != 0) {
            strncpy(_rds.radioText, rtClean, sizeof(_rds.radioText) - 1);
            _rds.textValid = true;
            _rds.textChangeCounter++; // Informujemy nasłuchujące moduły o zmianie
        }
    }

    if (si && si[0])
    {
        strncpy(
            _rds.stationInfo,
            si,
            sizeof(_rds.stationInfo)-1);
    }

    if (tim && tim[0])
    {
        strncpy(
            _rds.time,
            tim,
            sizeof(_rds.time)-1);

        _rds.timeValid = true;
    }

    _rds.pty =
        _radio.getRdsProgramType();

    _rds.tp =
        _radio.getRdsTrafficProgramCode();

    _lastRDSUpdate =
        millis();
}

bool FMRadio::getStationName(
    char* buffer,
    uint8_t size) {
    if (!_rds.stationValid)
        return false;

    strncpy(
        buffer,
        _rds.stationName,
        size - 1);

    buffer[size - 1] = 0;

    return true;
}

bool FMRadio::getRadioText(
    char* buffer,
    uint8_t size) {
    if (!_rds.textValid)
        return false;

    strncpy(
        buffer,
        _rds.radioText,
        size - 1);

    buffer[size - 1] = 0;

    return true;
}

void FMRadio::resetRDS() {
    memset(&_rds, 0, sizeof(_rds));

    memset(_lastRawPS, 0, sizeof(_lastRawPS));

    _sameNameCounter = 0;

    _radio.clearRdsBuffer();
}