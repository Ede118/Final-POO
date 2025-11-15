#include "servo_gripper.h"
#include <Arduino.h>

#if !defined(SIMULATION) || !SIMULATION
#include <Servo.h>

// Variable global para modo no-simulación
Servo servo_motor;  
#endif

Servo_Gripper::Servo_Gripper(int pin, float grip_degree, float ungrip_degree) {
  servo_pin = pin;
  servo_grip_deg = grip_degree;
  servo_ungrip_deg = ungrip_degree;
}

void Servo_Gripper::cmdOn() {
  #if !defined(SIMULATION) || !SIMULATION
  servo_motor.attach(servo_pin);
  servo_motor.write(servo_grip_deg);
  #endif
  
  delay(300);
  
  #if !defined(SIMULATION) || !SIMULATION
  servo_motor.detach();
  #endif
}

void Servo_Gripper::cmdOff() {
  #if !defined(SIMULATION) || !SIMULATION
  servo_motor.attach(servo_pin);
  servo_motor.write(servo_ungrip_deg);
  #endif
  
  delay(300);
  
  #if !defined(SIMULATION) || !SIMULATION
  servo_motor.detach();
  #endif
}
