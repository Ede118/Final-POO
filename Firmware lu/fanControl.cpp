#include "fanControl.h"
#include "logger.h"
#include <Arduino.h>

FanControl::FanControl(int aPin, int aFanDelay) {
  fan_delay = aFanDelay * 1000;
  nextShutdown = 0;
  pin = aPin;
#ifndef SIMULATION
 pinMode(pin , OUTPUT);
  digitalWrite(pin , LOW);
#endif
  state = false;
}

void FanControl::enable(bool value) {
  if (value) {
    state = true;
#ifndef SIMULATION
    digitalWrite(pin, HIGH);
#endif
  } else {
#ifndef SIMULATION
    disable();
#endif
  }
}

void FanControl::disable() {
  state = false;
  nextShutdown = millis() + fan_delay;
  update();
}

void FanControl::update() {
  if (!state) {
     if (millis() >= nextShutdown) {
#ifndef SIMULATION
       digitalWrite(pin, LOW);
#endif
     }
  }
}

bool FanControl::getState(){
    return state;
  }
