#ifndef SERVO_GRIPPER_H_
#define SERVO_GRIPPER_H_

#ifndef SIMULATION
#include <Servo.h>
#endif

class Servo_Gripper{
public:
  Servo_Gripper(int pin, float grip_degree, float ungrip_degree);
  void cmdOn();
  void cmdOff();
private:
#ifndef SIMULATION
  Servo servo_motor;
#endif
  int servo_pin;
  float servo_grip_deg;
  float servo_ungrip_deg;
};

#endif
