#include "FMRadio.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Character helpers (file-scope statics — not exposed in the header)
// ---------------------------------------------------------------------------

static inline bool rdsIsAlpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static inline bool rdsIsUpper(char c) { return c >= 'A' && c <= 'Z'; }
static inline bool rdsIsLower(char c) { return c >= 'a' && c <= 'z'; }
static inline bool rdsIsDigit(char c) { return c >= '0' && c <= '9'; }
static inline bool rdsIsWordEnd(char c) { return c == '\0' || c == ' ' || c == 0x0D; }
static inline bool rdsCI(char a, char b) {   // case-insensitive compare
    if (a >= 'a' && a <= 'z') a -= 32;
    if (b >= 'a' && b <= 'z') b -= 32;
    return a == b;
}

/// Returns true when the '.' at str[i] is part of a recognised domain/URL
/// pattern and should be exempt from the letter–dot–letter sandwich rule.
///
/// Recognised forms (case-insensitive):
///   Prefix : www.
///   TLDs   : .pl  .de  .fm  .com
///   (extend the TLD table below to add more)
static bool rdsIsDomainDot(const char* str, uint8_t i) {
    // www. prefix — three 'w' chars immediately before the dot
    if (i >= 3 &&
        rdsCI(str[i - 3], 'w') && rdsCI(str[i - 2], 'w') && rdsCI(str[i - 1], 'w'))
        return true;

    // TLD suffix — read up to 4 chars after the dot
    char n1 = str[i + 1];
    char n2 = (n1 != '\0') ? str[i + 2] : '\0';
    char n3 = (n2 != '\0') ? str[i + 3] : '\0';
    char n4 = (n3 != '\0') ? str[i + 4] : '\0';

    if (rdsCI(n1, 'p') && rdsCI(n2, 'l') && rdsIsWordEnd(n3)) return true; // .pl
    if (rdsCI(n1, 'd') && rdsCI(n2, 'e') && rdsIsWordEnd(n3)) return true; // .de
    if (rdsCI(n1, 'f') && rdsCI(n2, 'm') && rdsIsWordEnd(n3)) return true; // .fm
    if (rdsCI(n1, 'c') && rdsCI(n2, 'o') && rdsCI(n3, 'm') &&
        rdsIsWordEnd(n4)) return true;                                       // .com

    return false;
}

// ---------------------------------------------------------------------------
// RDS quality validation
// ---------------------------------------------------------------------------

/// PS validation rules (strict):
///   • Whitelist: [A-Za-z 0-9 , .]  — everything else is rejected.
///   • No lowercase letter immediately followed by an uppercase letter
///     (mid-word bit-flip indicator, e.g. "rAdio").
///   • No two identical consecutive punctuation chars (e.g. "..", ",,").
///   • Must contain at least 2 non-space characters.
bool FMRadio::validatePS(const char* str) {
    if (!str || str[0] == '\0') return false;

    uint8_t nonSpace   = 0;
    uint8_t camelCount = 0; // lower→upper transitions (max 1 allowed: "MeloRadio")
    char    prev       = '\0';

    for (uint8_t i = 0; i < 8 && str[i] != '\0'; i++) {
        char c = str[i];

        // Strict whitelist
        if (!rdsIsAlpha(c) && !rdsIsDigit(c) && c != ' ' && c != '.' && c != ',')
            return false;

        if (c != ' ') nonSpace++;

        // No consecutive spaces
        if (c == ' ' && prev == ' ') return false;

        bool cIsPunct = (c == '.' || c == ',');

        if (cIsPunct) {
            // Punctuation cannot open a PS string
            if (i == 0) return false;
            // Space immediately before punctuation: " ," or " ."
            if (prev == ' ') return false;
            // Letter–punct–letter sandwich: "T.K", "K,Z"
            // Exception: dot that is part of a domain (e.g. "tokfm.pl" fits in PS)
            char next = str[i + 1];
            bool domainDot = (c == '.' && rdsIsDomainDot(str, i));
            if (rdsIsAlpha(prev) && rdsIsAlpha(next) && !domainDot) return false;
        }

        if (prev) {
            // Lowercase → uppercase: allow exactly one such boundary (CamelCase
            // brand names like "MeloRadio"); two or more transitions = corruption.
            if (rdsIsLower(prev) && rdsIsUpper(c)) {
                if (++camelCount > 1) return false;
            }

            // Consecutive identical punctuation (e.g. "..", ",,")
            if ((prev == '.' || prev == ',') && prev == c) return false;
        }
        prev = c;
    }

    return nonSpace >= 2;
}

/// RT validation rules (permissive but anomaly-aware):
///   • Only printable ASCII (0x20–0x7E).
///   • Digit immediately followed by a letter → rejected.
///   • Letter immediately followed by a digit that is itself followed by
///     another non-space character → rejected.
///     Exception: letter → digit → (space | end) is fine, e.g. "PR1 FM".
///   • Lowercase → uppercase mid-word (no preceding space) → rejected.
///   • Two consecutive identical punctuation characters → rejected.
///   • Must contain at least 2 non-space characters.
bool FMRadio::validateRT(const char* str) {
    if (!str || str[0] == '\0') return false;

    uint8_t nonSpace   = 0;
    uint8_t camelCount = 0; // lower→upper transitions (max 1 allowed)
    char    prev       = '\0';

    for (uint8_t i = 0; i < 64 && str[i] != '\0' && str[i] != 0x0D; i++) {
        char c = str[i];

        // Must be printable ASCII
        if (c < 0x20 || c > 0x7E) return false;

        if (c != ' ') nonSpace++;

        // No consecutive spaces
        if (c == ' ' && prev == ' ') return false;

        // "Sentence punctuation": chars that delimit clauses/sentences and
        // should never be wedged between letters or follow a space directly.
        // Excludes '-', '(', ')', '"', '\'' which can legitimately touch letters.
        bool cIsSentPunct = (c == '.' || c == ',' || c == ':' ||
                             c == ';' || c == '!' || c == '?');

        if (cIsSentPunct && prev != '\0') {
            // Space immediately before punctuation: " ,", " .", " :" etc.
            if (prev == ' ') return false;
            // Letter-punct-letter sandwich: "T.K", "K,Z", "A:B"
            // Exception: dot that belongs to a domain/URL (www.x, x.pl, x.de, x.com)
            char next = str[i + 1];
            bool domainDot = (c == '.' && rdsIsDomainDot(str, i));
            if (rdsIsAlpha(prev) && rdsIsAlpha(next) && !domainDot) return false;
        }

        if (prev) {
            // Digit immediately followed by a letter (e.g. "5MHz").
            // Exception: the letter ends its word (e.g. "24h", "8x") — symmetric
            // with the letter→digit exception.
            if (rdsIsDigit(prev) && rdsIsAlpha(c)) {
                char next = str[i + 1];
                if (next != ' ' && next != '\0' && next != 0x0D) return false;
            }

            // Letter followed by digit — only allowed if the digit ends a "word"
            // (next char is space, end-of-string, or CR).  Rejects "PR1FM" but
            // allows "PR1 FM" and "PR1" at end of string.
            if (rdsIsAlpha(prev) && rdsIsDigit(c)) {
                char next = str[i + 1];
                if (next != ' ' && next != '\0' && next != 0x0D) return false;
            }

            // Lowercase → uppercase: allow exactly one such boundary (CamelCase);
            // two or more transitions indicate corruption (e.g. "rAdIo").
            if (rdsIsLower(prev) && rdsIsUpper(c)) {
                if (++camelCount > 1) return false;
            }

            // Consecutive identical punctuation (e.g. "..", ",,", "::")
            bool prevIsPunct = !rdsIsAlpha(prev) && !rdsIsDigit(prev) && prev != ' ';
            bool curIsPunct  = !rdsIsAlpha(c)    && !rdsIsDigit(c)    && c  != ' ';
            if (prevIsPunct && curIsPunct && prev == c) return false;
        }
        prev = c;
    }

    return nonSpace >= 2;
}

// ---------------------------------------------------------------------------
// Scan
// ---------------------------------------------------------------------------

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
    _lastEvaluatedFreq = 0;
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
            if (now - _lastScanAction >= SCAN_DELAY) {
                _radio.seek(1, 1);
                _lastScanAction = now;
                _scanState = EVALUATING;
            }
            break;

        case EVALUATING:
            if (now - _lastScanAction >= EVAL_DELAY) {
                uint16_t foundFreq = _radio.getRealFrequency();

                const bool wrapped = (_totalFound > 0 &&
                                      foundFreq <= _foundStations[_totalFound - 1]);
                const bool stuck   = (foundFreq == _lastEvaluatedFreq);
                const bool atStart = (foundFreq <= FREQ_MIN);

                if (atStart || wrapped || stuck) {
                    Log.notice(F("FMRadio: Auto-scan complete. Found: %d" CR), _totalFound);
                    _scanState = IDLE;
                    break;
                }

                _lastEvaluatedFreq = foundFreq;

                const bool rssiOk   = (_radio.getRssi() > RSSI_THRESHOLD);
                const bool stereoOk = (!STEREO_SCAN_GATE || _radio.isStereo());
                if (rssiOk && stereoOk) {
                    if (_totalFound < MAX_STORED) {
                        _foundStations[_totalFound++] = foundFreq;
                        Log.verbose(F("FMRadio: Found %d.%02d MHz" CR),
                                    foundFreq / 100, foundFreq % 100);
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

// ---------------------------------------------------------------------------
// Init / frequency / volume
// ---------------------------------------------------------------------------

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
    if (_frequency == freq) return;

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
int8_t  FMRadio::getRSSI()         { return static_cast<int8_t>(_radio.getRssi()); }

// ---------------------------------------------------------------------------
// RDS — public
// ---------------------------------------------------------------------------

/// Called every loop().  Internally throttled to one library read every 80 ms.
///
/// Validation + temporal redundancy ("soft error correction"):
///   PS — validatePS() must pass AND the new value must match the previous
///        read (_psCandidate).  RDS group 0A repeats the 8-char PS name
///        continuously; a second identical read confirms the data is stable.
///   RT — validateRT() must pass AND the new value must match _rtCandidate.
///        RT cycles every few seconds; requiring two matching reads adds
///        ~1 cycle of latency but eliminates single-burst corruption.
void FMRadio::updateRDS() {
    uint32_t now = millis();
    if ((uint32_t)(now - _lastRDSUpdate) < POLLING_RDS) return;
    _lastRDSUpdate = now;

    char *ps, *si, *rt, *tim;
    if (!_radio.getRdsAllData(&ps, &si, &rt, &tim)) return;

    // --- PS: accept on first valid read ---
    // No consistency gate: many stations use dynamic/scrolling PS, which means
    // consecutive reads always differ — a "seen twice" gate would never fire.
    if (ps && ps[0] != '\0' && validatePS(ps)) {
        if (strncmp(_rds.stationName, ps, 8) != 0) {
            strncpy(_rds.stationName, ps, 8);
            _rds.stationName[8] = '\0';
            _rds.stationValid = true;
        }
    }

    // --- RT: accept on first valid read ---
    if (rt && rt[0] != '\0' && validateRT(rt)) {
        if (strncmp(_rds.radioText, rt, 64) != 0) {
            _rds.textChangeCounter++;
            strncpy(_rds.radioText, rt, 64);
            _rds.radioText[64] = '\0';
            _rds.textValid = true;
        }
    }

    // --- SI (station info) — no validation, rarely corrupted ---
    if (si && si[0] != '\0') {
        strncpy(_rds.stationInfo, si, 32);
        _rds.stationInfo[32] = '\0';
    }

    // --- CT (clock-time from group 4A) — fixed format, validated in getRDSDateTime() ---
    if (tim && tim[0] != '\0') {
        strncpy(_rds.time, tim, 19);
        _rds.time[19] = '\0';
        _rds.timeValid = true;
    }

    _rds.pty = _radio.getRdsProgramType();
    _rds.tp  = _radio.getRdsTrafficProgramCode();
}

// ---------------------------------------------------------------------------
// RDS datetime helper
// ---------------------------------------------------------------------------

bool FMRadio::getRDSDateTime(uint8_t& day, uint8_t& month, uint8_t& year,
                             uint8_t& hour, uint8_t& minute) {
    if (!_rds.timeValid || _rds.time[0] == '\0') return false;

    const char* t = _rds.time;
    uint8_t len = 0;
    while (t[len] != '\0') len++;

    if (len < 16) return false;

    year   = static_cast<uint8_t>((t[2]  - '0') * 10 + (t[3]  - '0'));
    month  = static_cast<uint8_t>((t[5]  - '0') * 10 + (t[6]  - '0'));
    day    = static_cast<uint8_t>((t[8]  - '0') * 10 + (t[9]  - '0'));
    hour   = static_cast<uint8_t>((t[11] - '0') * 10 + (t[12] - '0'));
    minute = static_cast<uint8_t>((t[14] - '0') * 10 + (t[15] - '0'));

    return (month > 0 && month <= 12 && day > 0 && day <= 31
            && hour < 24 && minute < 60);
}

// ---------------------------------------------------------------------------
// RDS — private
// ---------------------------------------------------------------------------

void FMRadio::resetRDS() {
    _rds.stationName[0] = '\0';
    _rds.radioText[0]   = '\0';
    _rds.stationInfo[0] = '\0';
    _rds.time[0]        = '\0';

    _rds.stationValid      = false;
    _rds.textValid         = false;
    _rds.timeValid         = false;
    _rds.textChangeCounter = 0;

    _radio.clearRdsBuffer();
}