#include "RampsStepper.h"
#include "config.h"

#include <Arduino.h>

RampsStepper::RampsStepper(int aStepPin, int aDirPin, int aEnablePin, bool aInverse) {
  setReductionRatio(MAIN_GEAR_TEETH / MOTOR_GEAR_TEETH, MICROSTEPS * STEPS_PER_REV);
  stepPin = aStepPin;
  dirPin = aDirPin;
  enablePin = aEnablePin;
  inverse = aInverse;
  stepperStepPosition = 0;
  stepperStepTargetPosition;
#ifndef SIMULATION
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
#endif
  enable(false);
}

void RampsStepper::enable(bool value) {
  state = value;
#ifndef SIMULATION
  digitalWrite(enablePin, !value);
#endif
}

bool RampsStepper::isOnPosition() const {
  return stepperStepPosition == stepperStepTargetPosition;
}

int RampsStepper::getPosition() const {
  return stepperStepPosition;
}

void RampsStepper::setPosition(int value) {
  stepperStepPosition = value;
  stepperStepTargetPosition = value;
}

void RampsStepper::stepToPosition(int value) {
  stepperStepTargetPosition = value;
}

void RampsStepper::stepToPositionMM(float mm, float steps_per_mm) {
  stepperStepTargetPosition = mm * steps_per_mm;
}

void RampsStepper::stepRelative(int value) {
  value += stepperStepPosition;
  stepToPosition(value);
}

float RampsStepper::getPositionRad() const {
  return stepperStepPosition / radToStepFactor;
}

void RampsStepper::setPositionRad(float rad) {
  setPosition(rad * radToStepFactor);
}

void RampsStepper::stepToPositionRad(float rad) {
  stepperStepTargetPosition = rad * radToStepFactor;
}

void RampsStepper::stepRelativeRad(float rad) {
  stepRelative(rad * radToStepFactor);
}

void RampsStepper::update() {   
  while (stepperStepTargetPosition < stepperStepPosition) {  
#ifndef SIMULATION
    digitalWrite(dirPin, !inverse);
    digitalWrite(stepPin, HIGH);
    digitalWrite(stepPin, LOW);
#endif
    stepperStepPosition--;
  }
  
  while (stepperStepTargetPosition > stepperStepPosition) {    
#ifndef SIMULATION
    digitalWrite(dirPin, inverse);
    digitalWrite(stepPin, HIGH);
    digitalWrite(stepPin, LOW);
#endif
    stepperStepPosition++;
  }
}

void RampsStepper::setReductionRatio(float gearRatio, int stepsPerRev) {
  radToStepFactor = gearRatio * stepsPerRev / 2 / PI;
}

bool RampsStepper::getState() const {
  return state;
}
