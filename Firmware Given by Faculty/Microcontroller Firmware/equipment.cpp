#include "equipment.h"
#include <Arduino.h>

Equipment::Equipment(int equipment_pin){
  pin = equipment_pin;
#ifndef SIMULATION
  pinMode(pin, OUTPUT);
#endif
}

void Equipment::cmdOn(){
#ifndef SIMULATION
  digitalWrite(pin, HIGH);
#endif 
}

void Equipment::cmdOff(){
#ifndef SIMULATION
  digitalWrite(pin, LOW);  
#endif  
}
