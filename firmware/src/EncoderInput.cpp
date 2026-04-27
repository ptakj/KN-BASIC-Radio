#include "EncoderInput.h"

void EncoderInput::begin(uint8_t clkPin, uint8_t dtPin, uint8_t swPin) {
    _clkPin = clkPin;
    _dtPin  = dtPin;
    _swPin  = swPin;

    pinMode(_clkPin, INPUT_PULLUP);
    pinMode(_dtPin,  INPUT_PULLUP);
    pinMode(_swPin,  INPUT_PULLUP);

    _lastClk      = digitalRead(_clkPin);
    // Track button raw state using the same "true = pressed" convention as update().
    // Pin is HIGH when idle (pull-up), LOW when pressed (active-low).
    _lastSwRaw    = (digitalRead(_swPin) == LOW);
    _buttonState  = false;
    _pendingPress = false;
    _pendingSteps = 0;
    _debounceTime = 0;
}

void EncoderInput::update() {
    // --- Rotary encoder (CLK edge + DT direction) ---
    uint8_t clk = digitalRead(_clkPin);
    if (clk != _lastClk) {
        if (digitalRead(_dtPin) != clk) {
            ++_pendingSteps;
        } else {
            --_pendingSteps;
        }
        _lastClk = clk;
    }

    // --- Button with software debounce ---
    bool swRaw = (digitalRead(_swPin) == LOW); // true = pressed (active-low pull-up)
    uint32_t now = millis();

    if (swRaw != _lastSwRaw) {
        // Raw state changed: restart the debounce timer.
        _lastSwRaw    = swRaw;
        _debounceTime = now;
    } else if ((uint32_t)(now - _debounceTime) >= DEBOUNCE_MS &&
               swRaw != _buttonState) {
        // Raw state has been stable long enough and differs from debounced state.
        if (swRaw && !_buttonState) {
            // Rising edge (not-pressed → pressed): fire the one-shot flag.
            _pendingPress = true;
        }
        _buttonState = swRaw;
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
