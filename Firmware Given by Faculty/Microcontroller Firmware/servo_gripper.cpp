#include "servo_gripper.h"
#include <Arduino.h>

#ifndef SIMULATION
#include <Servo.h>
#endif

Servo_Gripper::Servo_Gripper(int pin, float grip_degree, float ungrip_degree){
  servo_pin = pin;
  servo_grip_deg = grip_degree;
  servo_ungrip_deg = ungrip_degree;
#ifndef SIMULATION
  Servo servo_motor;
#endif
}

void Servo_Gripper::cmdOn(){
#ifndef SIMULATION
  servo_motor.attach(servo_pin);
  servo_motor.write(servo_grip_deg);
#endif
  
  delay(300);
//    unsigned long ini = millis();
//    while (millis() - ini < 300);
#ifndef SIMULATION
  servo_motor.detach();
#endif
}

void Servo_Gripper::cmdOff(){
#ifndef SIMULATION
  servo_motor.attach(servo_pin);
  servo_motor.write(servo_ungrip_deg);
#endif
  delay(300);
//    unsigned long ini = millis();
//    while (millis() - ini < 300);
#ifndef SIMULATION
  servo_motor.detach();
#endif
}
