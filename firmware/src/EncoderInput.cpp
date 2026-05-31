#include "EncoderInput.h"

void EncoderInput::begin(uint8_t clkPin, uint8_t dtPin, uint8_t swPin) {
    _clkPin = clkPin;
    _dtPin  = dtPin;
    _swPin  = swPin;

    pinMode(_clkPin, INPUT_PULLUP);
    pinMode(_dtPin,  INPUT_PULLUP);
    pinMode(_swPin,  INPUT_PULLUP);

    _lastClk      = digitalRead(_clkPin);
    _lastSwRaw    = (digitalRead(_swPin) == LOW);
    _buttonState  = false;
    _pendingPress = false;
    _pendingLong  = false;
    _longFired    = false;
    _pendingSteps = 0;
    _debounceTime = 0;
    _pressStartTime = 0;
}

void EncoderInput::update() {
    uint32_t now = millis();
    // --- Rotary encoder ---
    uint8_t clk = digitalRead(_clkPin);
    if (clk != _lastClk) {
        _lastClk = clk;
        if (clk == HIGH && (uint32_t)(now - _lastRotTime) >= ROT_DEBOUNCE_MS) {
            if (digitalRead(_dtPin) != clk)
                ++_pendingSteps;
            else
                --_pendingSteps;
        }
    }

    // --- Button with debounce + long-press detection ---
    bool swRaw = (digitalRead(_swPin) == LOW);

    if (swRaw != _lastSwRaw) {
        _lastSwRaw    = swRaw;
        _debounceTime = now;
    } else if ((uint32_t)(now - _debounceTime) >= DEBOUNCE_MS &&
               swRaw != _buttonState) {

        _buttonState = swRaw;

        if (swRaw) {
            // Button just pressed (debounced)
            _pressStartTime = now;
            _longFired      = false;
        } else {
            // Button just released — if long press hadn't fired, it's a short press
            if (!_longFired) {
                _pendingPress = true;
            }
            _longFired = false;
        }
    }

    // Long-press: fires once while button is held
    if (_buttonState && !_longFired &&
        (uint32_t)(now - _pressStartTime) >= LONG_PRESS_MS) {
        _pendingLong = true;
        _longFired   = true;
    }
}

int8_t EncoderInput::getRotation() {
    int8_t steps  = _pendingSteps;
    _pendingSteps = 0;
    return steps;
}

bool EncoderInput::wasButtonPressed() {
    bool pressed  = _pendingPress;
    _pendingPress = false;
    return pressed;
}

bool EncoderInput::wasLongPressed() {
    bool lp      = _pendingLong;
    _pendingLong = false;
    return lp;
}