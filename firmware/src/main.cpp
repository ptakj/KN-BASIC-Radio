#include <Arduino.h>
#include "ArduinoRadio.h"

static ArduinoRadio radio;

void setup() {
    radio.begin();
}

void loop() {
    radio.update();
}