#pragma once
#include <Arduino.h>
#include <stdint.h>

/// Rotary encoder with a push-button.
/// Call update() on every loop iteration, then consume events via
/// getRotation() and wasButtonPressed().
class EncoderInput {
public:
    /// Attach to hardware pins and set up pull-ups.
    void begin(uint8_t clkPin, uint8_t dtPin, uint8_t swPin);

    /// Sample hardware state; must be called every loop iteration.
    void update();

    /// Returns accumulated rotation steps since the last call, then clears.
    /// Positive = clockwise, negative = counter-clockwise.
    int8_t getRotation();

    /// Returns true exactly once per debounced button press (auto-clears).
    bool wasButtonPressed();

private:
    uint8_t  _clkPin       = 0;
    uint8_t  _dtPin        = 0;
    uint8_t  _swPin        = 0;

    uint8_t  _lastClk      = 0;
    int8_t   _pendingSteps = 0;

    bool     _lastSwRaw    = true;   // HIGH = not pressed (pull-up idle)
    bool     _buttonState  = false;  // Debounced pressed state
    bool     _pendingPress = false;  // One-shot press flag
    uint32_t _debounceTime = 0;

    static constexpr uint32_t DEBOUNCE_MS = 30;
};

