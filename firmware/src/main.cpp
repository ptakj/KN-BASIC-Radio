#include <Arduino.h>
#include "ArduinoRadio.h"

static ArduinoRadio radio;

void setup() {
    delay(500); // Short delay to allow serial monitor connection if needed
    radio.begin();
}

void loop() {
    radio.update();
}